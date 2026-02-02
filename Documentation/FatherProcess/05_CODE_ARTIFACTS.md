# FatherProcess - Code Artifacts

**Component**: FatherProcess
**Task ID**: DOC-FATHER-001
**Generated**: 2026-02-02

---

## Key Code Snippets

### 1. File Header (FatherProcess.c:1-20)

```c
/*=======================================================================
||                                                                      ||
||                          FatherProcess.c                             ||
||                                                                      ||
||======================================================================||
||                                                                      ||
||  Description:                                                        ||
||  -----------                                                         ||
||  Server for starting the MACABI system and keeping all servers up.   ||
||  Servers to start up and parameters are read from database.          ||
||                                                                      ||
||                                                                      ||
||  Usage:                                                              ||
||  -----                                                               ||
||  FatherProcess.exe                                                   ||
||                                                                      ||
||======================================================================||
||  WRITTEN BY  : Ely Levy ( Reshuma )                                  ||
||  DATE        : 30.05.1996                                            ||
=======================================================================*/
```

### 2. Preprocessor Defines (FatherProcess.c:23-76)

```c
#define MAIN
#define FATHER
#define NO_PRN

static   char   *GerrSource = __FILE__;

#include <MacODBC.h>
#include "MsgHndlr.h"

// ... diagnostic print macros (lines 33-65) ...

#define Conflict_Test(r)                                \
    if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)   \
{                                                       \
    r = MAC_TRUE;                                       \
    break;                                              \
}

#define PROG_KEY_LEN            31
#define PARAM_NAM_LEN           31
#define PARAM_VAL_LEN           31
```

### 3. Global TABLE Pointers (FatherProcess.c:78-117)

```c
TABLE       *parm_tablep,
            *proc_tablep,
            *updt_tablep,
            *stat_tablep,
            *tstt_tablep,
            *dstt_tablep,
            *prsc_tablep,
            *msg_recid_tablep,
            *dipr_tablep;

TABLE   **table_ptrs[] =
{
        &parm_tablep,/*             PARM_TABLE              */
        &proc_tablep,/*             PROC_TABLE              */
        &stat_tablep,/*             STAT_TABLE              */
        &tstt_tablep,/*             TSTT_TABLE              */
        &dstt_tablep,/*             DSTT_TABLE              */
        &updt_tablep,/*             UPDT_TABLE              */
        &msg_recid_tablep,  /* Message Rec_id table */
        &prsc_tablep,/*             PRSC_TABLE              */
        &dipr_tablep,/*             DIPR_TABLE              */
        NULL,        /*             NULL                    */
};
```

### 4. InstanceControl Array (FatherProcess.c:119-122)

```c
// DonR 09Aug2022: Add new array to store instance-control information. This
// is to support microservices, where we may have several multiple-instance
// programs running as part of the application.
TInstanceControl    InstanceControl [MAX_PROC_TYPE_USED + 1];
```

### 5. Signal Handler Installation (FatherProcess.c:239-253)

```c
// DonR 10Apr2022: Set up signal handler for Signal 15 (SIGTERM),
// which will trigger a "soft" system shutdown.
memset ((char *)&NullSigset, 0, sizeof(sigset_t));

// DonR 11Aug2022: Initialize new InstanceControl array.
memset ((char *)&InstanceControl, 0, sizeof(InstanceControl));

sig_act_terminate.sa_handler    = TerminateHandler;
sig_act_terminate.sa_mask       = NullSigset;
sig_act_terminate.sa_flags      = 0;

if (sigaction (SIGTERM, &sig_act_terminate, NULL) != 0)
{
    GerrLogMini (GerrId, "FatherProcess: can't install signal handler for SIGTERM" GerrErr, GerrStr);
}
```

### 6. Database Connection with Retry (FatherProcess.c:259-271)

```c
// Connect to database.
// DonR 15Jan2020: SQLMD_connect (which resolves to INF_CONNECT) now
// handles ODBC connections as well as the old Informix DB connection.
do
{
    SQLMD_connect ();
    if (!MAIN_DB->Connected)
    {
        sleep (10);
        GerrLogMini (GerrId, "\n\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
    }
}
while (!MAIN_DB->Connected);
```

### 7. Boot Sequence - Core Initialization (FatherProcess.c:299-351)

```c
// Get current program named pipe and listen on it.
GetCurrNamedPipe (CurrNamedPipe);
ABORT_ON_ERR (ListenSocketNamed (CurrNamedPipe));

// Create Maccabi system semaphore.
ABORT_ON_ERR (CreateSemaphore ());

// Init shared memory
ABORT_ON_ERR (InitFirstExtent (SharedSize));

// Create & Refresh all needed tables in shared memory
prn ("FatherProcess: Create & Refresh all needed tables in shared memory.\n");
for (len = 0; TableTab[len].table_name[0]; len++)
{
    ABORT_ON_ERR (CreateTable (TableTab[len].table_name, table_ptrs[len]));

    if (TableTab[len].refresh_func != NULL)
    {
        // If the memory table's refresh function pointer is non-NULL, call it
        ABORT_ON_ERR ((*TableTab[len].refresh_func)(len));

        // Create and insert table-updated data for this shared-memory table.
        strcpy (updt_data.table_name, TableTab[len].table_name);
        updt_data.update_date = GetDate();
        updt_data.update_time = GetTime();
        state = AddItem (updt_tablep, (ROW_DATA)&updt_data);
        // ...
    }
}

// ... status initialization (lines 337-348) ...

// Add myself to the shared-memory processes table.
ABORT_ON_ERR (AddCurrProc (0, FATHERPROC_TYPE, 0, PHARM_SYS | DOCTOR_SYS));
```

### 8. Watchdog Loop Entry (FatherProcess.c:471)

```c
// This is the main "watchdog" loop - it keeps running until the
// Maccabi application needs to be shut down.
//
// DonR 11Apr2022: Added (!caught_signal) as a condition - if we catch
// a signal (typically 15 == SIGTERM) we want to terminate the entire
// application.
for (shut_down = MAC_FALS; (shut_down == MAC_FALS) && (!caught_signal); )
```

### 9. Dead Child Detection (FatherProcess.c:673-698)

```c
// Collect status of all child processes.
son_pid = waitpid   (   (pid_t) -1,     // Tell me about any child process
                        &status,        // Exit status
                        WNOHANG     );  // Don't wait.

// If no child process terminated, check the shared-memory died-processes table.
if (son_pid == 0)
{
    state = ActItems (dipr_tablep, 1, GetItem, (OPER_ARGS)&dipr_data);

    if (state == MAC_OK)
    {
        son_pid = dipr_data.pid;
        status  = dipr_data.status;

        state = DeleteItem (dipr_tablep, (ROW_DATA)&dipr_data);
        // ...
        died_pr_flg = 1;
    }
    // ...
}
```

### 10. IPC Message Handling (FatherProcess.c:1123-1197)

```c
state = GetSocketMessage (500000, buf, sizeof (buf), &len, CLOSE_SOCKET);

if ((state == MAC_OK) && (len > 0))
{
    switch ((match = ListNMatch (buf, PcMessages)))
    {
        case -1:            // Unknown message.
                            GerrLogMini (GerrId, "FatherProcess got unknown ipc message '%s'", buf);
                            break;

        case LOAD_PAR:      // Load parameters from DB to shared memory.
                            BREAK_ON_ERR (SqlGetParamsByName (FatherParams, PAR_RELOAD));
                            BREAK_ON_ERR (sql_dbparam_into_shm (parm_tablep, stat_tablep, PAR_RELOAD));
                            break;

        case STRT_PH_ONLY:  // Start up pharmacy system.
        case STRT_DC_ONLY:  // Start up doctor system.
                            // ...
                            run_system ((match == STRT_PH_ONLY) ? PHARM_SYS : DOCTOR_SYS);
                            // ...

        case SHUT_DWN:      // Gracefully shut down the whole system.
                            set_sys_status (GOING_DOWN, 0, 0);
                            continue;

        case STDN_IMM:      // Immediate full-system shutdown.
                            set_sys_status (SYSTEM_DOWN, 0, 0);
                            shut_down = MAC_TRUE;
                            continue;
    }
}
```

### 11. Shutdown Sequence - Kill Children (FatherProcess.c:1365-1379)

```c
// Send terminate signal to child process.
switch (proc_data.proc_type)
{
    case SQLPROC_TYPE:          // SqlServer.exe
    case AS400TOUNIX_TYPE:      // As400UnixServer.exe
    case DOCSQLPROC_TYPE:       // DocSqlServer.exe
    case PURCHASE_HIST_TYPE:    // PurchaseHistoryServer

                KillSignal = 15;    // SIGTERM == "soft" kill.
                break;

    default:    KillSignal = 9;     // "Hard" kill.
                break;
}

kill (proc_data.pid, KillSignal);
```

### 12. Cleanup Sequence (FatherProcess.c:1441-1453)

```c
// Disconnect from database
SQLMD_disconnect ();

// Close sockets & dispose named pipe file
DisposeSockets ();

// Remove shared memory extents
KillAllExtents ();

// Delete semaphore
// DonR 11Apr2022: To avoid errors as other processes shut down, wait a bit
usleep (1 * 1000000);
DeleteSemaphore ();
```

### 13. TerminateHandler Function (FatherProcess.c:1937-1972)

```c
void     TerminateHandler (int signo)
{
    char        *sig_description;

    // Reset signal handling for the caught signal.
    sigaction (signo, &sig_act_terminate, NULL);

    caught_signal   = signo;

    // Produce a friendly and informative message in the SQL Server logfile.
    switch (signo)
    {
        case SIGFPE:
            sig_description = "floating-point error - probably division by zero";
            break;

        case SIGSEGV:
            sig_description = "segmentation error";
            break;

        case SIGTERM:
            sig_description = "user-requested shutdown";
            break;

        default:
            sig_description = "check manual page for SIGNAL";
            break;
    }

    GerrLogMini (GerrId,
                 "\n\nFatherProcess got signal %d (%s). Shutting down Maccabi System.", signo, sig_description);
}
```

---

## Configuration

### SQL Cursors (MacODBC_MyOperators.c:89-113)

**FP_params_cur** (lines 89-102):
```c
case FP_params_cur:
    SQL_CommandText     =   "   SELECT  parameter_name,     parameter_value,            "
                            "           program_type,       instance_control,           "
                            "           startup_instances,  max_instances,              "
                            "           min_free_instances                              "
                            "   FROM    setup_new                                       "
                            "   WHERE   program_name = ?    OR      program_name = ?    ";
    NumOutputColumns    =   7;
    OutputColumnSpec    =   "   varchar(32), varchar(32), short, short, short, short, short   ";
    NumInputColumns     =   2;
    InputColumnSpec     =   "   varchar(32), varchar(32)    ";
    break;
```

**FP_setup_cur** (lines 105-113):
```c
case FP_setup_cur:
    SQL_CommandText     =   "   SELECT      program_name,   parameter_name,     parameter_value "
                            "   FROM        setup_new                                           "
                            "   ORDER BY    program_name, parameter_name                        ";
    NumOutputColumns    =   3;
    OutputColumnSpec    =   "   varchar(32), varchar(32), varchar(32)   ";
    break;
```

### Setup Table Parameters (loaded at runtime)

Based on code patterns, the `setup_new` table appears to contain:

| Column | Type | Purpose |
|--------|------|---------|
| program_name | varchar(32) | Program identifier |
| parameter_name | varchar(32) | Parameter name |
| parameter_value | varchar(32) | Parameter value |
| program_type | short | Process type constant |
| instance_control | short | Multi-instance enabled flag |
| startup_instances | short | Initial instances to start |
| max_instances | short | Maximum allowed instances |
| min_free_instances | short | Minimum available instances |

---

## Database Operations

### Parameter Loading Pattern (FatherProcess.c:1516-1560)

```c
for (restart = MAC_TRUE, tries = 0; (tries < SQL_UPDATE_RETRIES) && (restart == MAC_TRUE); tries++)
{
    restart = MAC_FALS;

    do
    {
        OpenCursor (    MAIN_DB, FP_setup_cur    );
        Conflict_Test_Cur (restart);
        BREAK_ON_ERR (SQLERR_error_test ());

        while (1)
        {
            FetchCursor (   MAIN_DB, FP_setup_cur    );
            // ...
            Conflict_Test (restart);
            BREAK_ON_TRUE   (SQLERR_code_cmp (SQLERR_end_of_fetch));
            BREAK_ON_ERR    (SQLERR_error_test ());

            // Build key and store in shared memory
            sprintf (parm_data.par_key, "%.*s.%.*s", PROG_KEY_LEN, prog_name, PARAM_NAM_LEN, param_name);
            sprintf (parm_data.par_val, "%.*s", PARAM_VAL_LEN, param_value);
            state = AddItem (parm_tablep, (ROW_DATA)&parm_data);
        }

        CloseCursor (   MAIN_DB, FP_setup_cur    );
    }
    while (0);

    if (restart == MAC_TRUE)
    {
        sleep (ACCESS_CONFLICT_SLEEP_TIME);
    }
}
```

---

## Makefile (Makefile:1-44)

```makefile
# FatherProcess.mak

include ../Util/Defines.mak

# Files
SRCS=   FatherProcess.c
INCS=   $(GEN_INCS)         $(SQL_INCS)
OBJS=   FatherProcess.o
TARGET=FatherProcess.exe

include ../Util/Rules.mak

# Dependencies
.TARGET: all

all: $(TARGET)

$(TARGET): $(OBJS)
    $(CC)  -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc  -D_GNU_SOURCE

FatherProcess.o : FatherProcess.c
    $(CC) $(INC_OPTS) $(COMP_OPTS) FatherProcess.o      FatherProcess.c -lodbc -D_GNU_SOURCE

clean:
    rm $(TARGET) $(OBJS)
```

**Build Dependencies**:
- Links with ODBC library (`-lodbc`)
- Uses GNU source extensions (`-D_GNU_SOURCE`)
- Includes general and SQL includes (`$(GEN_INCS)`, `$(SQL_INCS)`)

---

## Data Structures Reference

### From Global.h (via MacODBC.h include)

**PROC_DATA** - Process registry row:
- `proc_name` - Program name
- `log_file` - Log file path
- `named_pipe` - IPC endpoint path
- `pid` - Process ID
- `proc_type` - Process type constant
- `system` - Subsystem flag (PHARM_SYS/DOCTOR_SYS)
- `start_time` - Process start timestamp
- `retrys` - Restart attempt counter
- `busy_flg` - Process busy flag
- `eicon_port` - Eicon port (legacy)

**PARM_DATA** - Parameters row:
- `par_key` - Key (format: `program_name.param_name`)
- `par_val` - Value

**STAT_DATA** - System status row:
- `status` - Whole-system status
- `pharm_status` - Pharmacy subsystem status
- `doctor_status` - Doctor subsystem status
- `param_rev` - Parameters revision counter
- `tsw_flnum` - T-switch file number
- `sons_count` - Child process count

**DIPR_DATA** - Died-process row:
- `pid` - Process ID
- `status` - Exit status

**TInstanceControl** - Multi-instance control:
- `ProgramName` - Executable name
- `program_type` - Process type constant
- `program_system` - Subsystem flag
- `instance_control` - Enabled flag
- `startup_instances` - Initial count
- `max_instances` - Maximum count
- `min_free_instances` - Minimum available
- `instances_running` - Current count

---

*Generated by CIDRA Documenter Agent - DOC-FATHER-001*
