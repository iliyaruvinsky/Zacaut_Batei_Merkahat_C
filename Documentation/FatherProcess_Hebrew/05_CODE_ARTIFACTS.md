# FatherProcess - ארטיפקטים של קוד

**רכיב**: FatherProcess
**מזהה משימה**: DOC-FATHER-001
**תאריך יצירה**: 2026-02-02

---

## קטעי קוד מרכזיים

### 1. כותרת קובץ (FatherProcess.c:1-20)

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

### 2. הגדרות פרה-פרוססור (FatherProcess.c:23-76)

```c
#define MAIN
#define FATHER
#define NO_PRN

static   char   *GerrSource = __FILE__;

#include <MacODBC.h>
#include "MsgHndlr.h"

// ... מאקרואי הדפסת אבחון (שורות 33-65) ...

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

### 3. מצביעי TABLE גלובליים (FatherProcess.c:78-117)

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

### 4. מערך InstanceControl (FatherProcess.c:119-122)

```c
// DonR 09Aug2022: הוספת מערך חדש לאחסון מידע בקרת מופעים. זה
// לתמיכה במיקרו-שירותים, שבהם ייתכן שיהיו מספר תוכניות מרובות-מופעים
// פועלות כחלק מהאפליקציה.
TInstanceControl    InstanceControl [MAX_PROC_TYPE_USED + 1];
```

### 5. התקנת מטפל אותות (FatherProcess.c:239-253)

```c
// DonR 10Apr2022: הגדרת מטפל אותות עבור Signal 15 (SIGTERM),
// שיפעיל כיבוי מערכת "רך".
memset ((char *)&NullSigset, 0, sizeof(sigset_t));

// DonR 11Aug2022: אתחול מערך InstanceControl החדש.
memset ((char *)&InstanceControl, 0, sizeof(InstanceControl));

sig_act_terminate.sa_handler    = TerminateHandler;
sig_act_terminate.sa_mask       = NullSigset;
sig_act_terminate.sa_flags      = 0;

if (sigaction (SIGTERM, &sig_act_terminate, NULL) != 0)
{
    GerrLogMini (GerrId, "FatherProcess: can't install signal handler for SIGTERM" GerrErr, GerrStr);
}
```

### 6. חיבור למסד נתונים עם ניסיונות חוזרים (FatherProcess.c:259-271)

```c
// חיבור למסד נתונים.
// DonR 15Jan2020: SQLMD_connect (שמתרגם ל-INF_CONNECT) כעת
// מטפל בחיבורי ODBC כמו גם חיבור DB Informix הישן.
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

### 7. רצף אתחול - אתחול ליבה (FatherProcess.c:299-351)

```c
// קבלת named pipe של התוכנית הנוכחית והאזנה עליו.
GetCurrNamedPipe (CurrNamedPipe);
ABORT_ON_ERR (ListenSocketNamed (CurrNamedPipe));

// יצירת סמפור מערכת מכבי.
ABORT_ON_ERR (CreateSemaphore ());

// אתחול זיכרון משותף
ABORT_ON_ERR (InitFirstExtent (SharedSize));

// יצירה ורענון של כל הטבלאות הנדרשות בזיכרון משותף
prn ("FatherProcess: Create & Refresh all needed tables in shared memory.\n");
for (len = 0; TableTab[len].table_name[0]; len++)
{
    ABORT_ON_ERR (CreateTable (TableTab[len].table_name, table_ptrs[len]));

    if (TableTab[len].refresh_func != NULL)
    {
        // אם מצביע פונקציית הרענון של טבלת הזיכרון אינו NULL, קרא לה
        ABORT_ON_ERR ((*TableTab[len].refresh_func)(len));

        // יצירה והכנסה של נתוני עדכון-טבלה עבור טבלת זיכרון משותף זו.
        strcpy (updt_data.table_name, TableTab[len].table_name);
        updt_data.update_date = GetDate();
        updt_data.update_time = GetTime();
        state = AddItem (updt_tablep, (ROW_DATA)&updt_data);
        // ...
    }
}

// ... אתחול סטטוס (שורות 337-348) ...

// הוספת עצמי לטבלת תהליכי הזיכרון המשותף.
ABORT_ON_ERR (AddCurrProc (0, FATHERPROC_TYPE, 0, PHARM_SYS | DOCTOR_SYS));
```

### 8. כניסה ללולאת השמירה (FatherProcess.c:471)

```c
// זוהי לולאת ה-"watchdog" הראשית - היא ממשיכה לרוץ עד ש
// אפליקציית מכבי צריכה להיכבות.
//
// DonR 11Apr2022: הוספת (!caught_signal) כתנאי - אם נתפוס
// אות (בדרך כלל 15 == SIGTERM) נרצה לסיים את כל
// האפליקציה.
for (shut_down = MAC_FALS; (shut_down == MAC_FALS) && (!caught_signal); )
```

### 9. זיהוי תהליכי בן מתים (FatherProcess.c:673-698)

```c
// איסוף סטטוס של כל תהליכי הבן.
son_pid = waitpid   (   (pid_t) -1,     // ספר לי על כל תהליך בן
                        &status,        // סטטוס יציאה
                        WNOHANG     );  // אל תחכה.

// אם אף תהליך בן לא הסתיים, בדוק את טבלת תהליכים-שמתו בזיכרון המשותף.
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

### 10. טיפול בהודעות IPC (FatherProcess.c:1123-1197)

```c
state = GetSocketMessage (500000, buf, sizeof (buf), &len, CLOSE_SOCKET);

if ((state == MAC_OK) && (len > 0))
{
    switch ((match = ListNMatch (buf, PcMessages)))
    {
        case -1:            // הודעה לא ידועה.
                            GerrLogMini (GerrId, "FatherProcess got unknown ipc message '%s'", buf);
                            break;

        case LOAD_PAR:      // טעינת פרמטרים מ-DB לזיכרון משותף.
                            BREAK_ON_ERR (SqlGetParamsByName (FatherParams, PAR_RELOAD));
                            BREAK_ON_ERR (sql_dbparam_into_shm (parm_tablep, stat_tablep, PAR_RELOAD));
                            break;

        case STRT_PH_ONLY:  // הפעלת מערכת בית מרקחת.
        case STRT_DC_ONLY:  // הפעלת מערכת רופאים.
                            // ...
                            run_system ((match == STRT_PH_ONLY) ? PHARM_SYS : DOCTOR_SYS);
                            // ...

        case SHUT_DWN:      // כיבוי מסודר של כל המערכת.
                            set_sys_status (GOING_DOWN, 0, 0);
                            continue;

        case STDN_IMM:      // כיבוי מיידי של כל המערכת.
                            set_sys_status (SYSTEM_DOWN, 0, 0);
                            shut_down = MAC_TRUE;
                            continue;
    }
}
```

### 11. רצף כיבוי - הריגת ילדים (FatherProcess.c:1365-1379)

```c
// שליחת אות סיום לתהליך בן.
switch (proc_data.proc_type)
{
    case SQLPROC_TYPE:          // SqlServer.exe
    case AS400TOUNIX_TYPE:      // As400UnixServer.exe
    case DOCSQLPROC_TYPE:       // DocSqlServer.exe
    case PURCHASE_HIST_TYPE:    // PurchaseHistoryServer

                KillSignal = 15;    // SIGTERM == הריגה "רכה".
                break;

    default:    KillSignal = 9;     // הריגה "קשה".
                break;
}

kill (proc_data.pid, KillSignal);
```

### 12. רצף ניקוי (FatherProcess.c:1441-1453)

```c
// ניתוק ממסד נתונים
SQLMD_disconnect ();

// סגירת סוקטים והסרת קובץ named pipe
DisposeSockets ();

// הסרת מקטעי זיכרון משותף
KillAllExtents ();

// מחיקת סמפור
// DonR 11Apr2022: למניעת שגיאות בזמן שתהליכים אחרים נכבים, חכה קצת
usleep (1 * 1000000);
DeleteSemaphore ();
```

### 13. פונקציית TerminateHandler (FatherProcess.c:1937-1972)

```c
void     TerminateHandler (int signo)
{
    char        *sig_description;

    // איפוס טיפול באותות עבור האות שנתפס.
    sigaction (signo, &sig_act_terminate, NULL);

    caught_signal   = signo;

    // הפקת הודעה ידידותית ואינפורמטיבית בקובץ יומן שרת SQL.
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

## תצורה

### סמני SQL (MacODBC_MyOperators.c:89-113)

**FP_params_cur** (שורות 89-102):
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

**FP_setup_cur** (שורות 105-113):
```c
case FP_setup_cur:
    SQL_CommandText     =   "   SELECT      program_name,   parameter_name,     parameter_value "
                            "   FROM        setup_new                                           "
                            "   ORDER BY    program_name, parameter_name                        ";
    NumOutputColumns    =   3;
    OutputColumnSpec    =   "   varchar(32), varchar(32), varchar(32)   ";
    break;
```

### פרמטרי טבלת Setup (נטענים בזמן ריצה)

על בסיס דפוסי קוד, נראה כי טבלת `setup_new` מכילה:

| עמודה | סוג | מטרה |
|-------|-----|------|
| program_name | varchar(32) | מזהה תוכנית |
| parameter_name | varchar(32) | שם פרמטר |
| parameter_value | varchar(32) | ערך פרמטר |
| program_type | short | קבוע סוג תהליך |
| instance_control | short | דגל הפעלת מרובי מופעים |
| startup_instances | short | מופעים התחלתיים להפעלה |
| max_instances | short | מקסימום מופעים מותר |
| min_free_instances | short | מינימום מופעים זמינים |

---

## פעולות מסד נתונים

### דפוס טעינת פרמטרים (FatherProcess.c:1516-1560)

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

            // בניית מפתח ואחסון בזיכרון משותף
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

**תלויות בנייה**:
- מקושר לספריית ODBC (`-lodbc`)
- משתמש בהרחבות GNU source (`-D_GNU_SOURCE`)
- כולל כותרות כלליות ו-SQL (`$(GEN_INCS)`, `$(SQL_INCS)`)

---

## הפניית מבני נתונים

### מ-Global.h (דרך MacODBC.h include)

**PROC_DATA** - שורת רישום תהליכים:
- `proc_name` - שם תוכנית
- `log_file` - נתיב קובץ יומן
- `named_pipe` - נתיב נקודת קצה IPC
- `pid` - מזהה תהליך
- `proc_type` - קבוע סוג תהליך
- `system` - דגל תת-מערכת (PHARM_SYS/DOCTOR_SYS)
- `start_time` - חותמת זמן הפעלת תהליך
- `retrys` - מונה ניסיונות הפעלה מחדש
- `busy_flg` - דגל עסוק
- `eicon_port` - פורט Eicon (ישן)

**PARM_DATA** - שורת פרמטרים:
- `par_key` - מפתח (פורמט: `program_name.param_name`)
- `par_val` - ערך

**STAT_DATA** - שורת סטטוס מערכת:
- `status` - סטטוס מערכת כוללת
- `pharm_status` - סטטוס תת-מערכת בית מרקחת
- `doctor_status` - סטטוס תת-מערכת רופאים
- `param_rev` - מונה גרסת פרמטרים
- `tsw_flnum` - מספר קובץ T-switch
- `sons_count` - ספירת תהליכי בן

**DIPR_DATA** - שורת תהליך שמת:
- `pid` - מזהה תהליך
- `status` - סטטוס יציאה

**TInstanceControl** - בקרת מרובי מופעים:
- `ProgramName` - שם קובץ הרצה
- `program_type` - קבוע סוג תהליך
- `program_system` - דגל תת-מערכת
- `instance_control` - דגל מופעל
- `startup_instances` - ספירה התחלתית
- `max_instances` - ספירה מקסימלית
- `min_free_instances` - מינימום זמינים
- `instances_running` - ספירה נוכחית

---

*נוצר על ידי סוכן המתעד של CIDRA - DOC-FATHER-001*
