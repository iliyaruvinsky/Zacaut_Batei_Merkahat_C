# ShrinkPharm - ארטיפקטים של קוד

**מזהה מסמך:** DOC-SHRINK-001
**תאריך יצירה:** 2026-02-03
**מקור:** source_code/ShrinkPharm/ (574 שורות מקור מאומתות)

---

## קטעי קוד מרכזיים

### כותרת קובץ (ShrinkPharm.c:1-11)

```c
/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*  			  ShrinkPharm.c				                      */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Purpose :													  */
/*       ODBC equivalent of ShrinkDoctor for use in the MS-SQL	  */
/*		 Pharmacy application									  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */
```

### הצהרות גלובליות (ShrinkPharm.c:13-32)

```c
#define MAIN
char *GerrSource = __FILE__;

#include <MacODBC.h>

void	TerminateHandler	(int signo);

static int	restart_flg		= 0;
int	caught_signal			= 0;	// DonR 30Nov2003: הימנעות משגיאת הידור "unresolved external".
int TikrotProductionMode	= 1;	// להימנעות משגיאת הידור "unresolved external".

struct sigaction	sig_act_terminate;

static int	need_rollback			= 0;
static int	recs_to_commit			= 0;
static int	recs_committed			= 0;
```

**הערה:** משתנים `need_rollback`, `recs_to_commit` ו-`recs_committed` נראים מוצהרים אך לא בשימוש בקוד הנוכחי.

---

### תצורת מסד נתונים (ShrinkPharm.c:119-137)

```c
// חיבור למסד נתונים.
// הגדרת תכונות MS-SQL עבור זמן קצוב שאילתה ועדיפות deadlock.
// עבור ShrinkPharm, אנחנו רוצים זמן קצוב ארוך יחסית (כך שנהיה די
// סבלניים) ועדיפות deadlock מתחת לרגיל (מכיוון שאנחנו רוצים
// להעדיף פעולות אפליקציה בזמן-אמת על פני תחזוקה).
LOCK_TIMEOUT			= 1000;	// מילישניות.
DEADLOCK_PRIORITY		= -2;	// 0 = רגיל, -10 עד 10 = עדיפות נמוכה עד גבוהה.
ODBC_PRESERVE_CURSORS	= 1;	// כך כל הסמנים יישמרו לאחר COMMITs.

do
{
    SQLMD_connect ();
    if (!MAIN_DB->Connected)
    {
        sleep (10);
        GerrLogFnameMini ("ShrinkPharm_log", GerrId, "\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
    }
}
while (!MAIN_DB->Connected);
```

---

### חישוב תאריך מחיקה (ShrinkPharm.c:187-191)

```c
strip_spaces (table_name);
strip_spaces (date_column_name);
MinDateToRetain = IncrementDate (SysDate, 0 - days_to_retain);

GerrLogFnameMini ("ShrinkPharm_log", GerrId, "\nPurge from %s where %s < %d...", table_name, date_column_name, MinDateToRetain);
```

---

### יצירת SQL דינמי (ShrinkPharm.c:193-216)

```c
if (use_where_clause)
{
    sprintf (	PredictQtyCommand,
                "SELECT	CAST (count(*) AS INTEGER) FROM %s WHERE %s < %d",
                table_name, date_column_name, MinDateToRetain);

    ExecSQL (	MAIN_DB, ShrinkPharmSelectQuantity, &PredictQtyCommand, &PredictedQuantity, END_OF_ARG_LIST	);

    GerrLogFnameMini ("ShrinkPharm_log", GerrId, "%'d rows to delete, SQLCODE = %d.", PredictedQuantity, SQLCODE);


    sprintf (	SelectCursorCommand,
                "SELECT %s FROM %s WHERE %s < %d",
                date_column_name, table_name, date_column_name, MinDateToRetain);
}
else
{
    sprintf (	SelectCursorCommand,
                "SELECT %s FROM %s",
                date_column_name, table_name);
}

sprintf (DeleteCommand, "DELETE FROM %s WHERE CURRENT OF ShrinkPharmSelCur", table_name);
```

---

### מחיקה ו-Commit באצווה (ShrinkPharm.c:266-294)

```c
ExecSQL (	MAIN_DB, ShrinkPharmDeleteCurrentRow, &DeleteCommand, END_OF_ARG_LIST	);

if (SQLERR_error_test ())
{
    break;
}
else
{
    // לא אמור לקרות שנקבל no-rows-affected על פקודת CURRENT OF, אבל
    // נבדוק בכל זאת.
    if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
    {
        GerrLogFnameMini ("ShrinkPharm_log", GerrId, "DELETE operation didn't delete anything!");
    }
    else
    {
        TotalRowsDeleted++;
        RowsDeletedSinceCommit++;
        RowsDeletedFullRun++;

        if (RowsDeletedSinceCommit >= commit_count)
        {
            CommitAllWork ();
            RowsDeletedSinceCommit = 0;
        }	// צריך לבצע commit.
    }	// לא no-rows-affected.
}	// לא שגיאת מסד נתונים.
```

---

### תיעוד סטטיסטיקות (ShrinkPharm.c:303-316)

```c
// כתיבת תוצאות ליומן ולטבלת ShrinkPharm.
SysDate	= GetDate ();
SysTime = GetTime ();
ExecSQL (	MAIN_DB, ShrinkPharmSaveStatistics,
            &SysDate, &SysTime, &TotalRowsDeleted, END_OF_ARG_LIST	);

// מכיוון ש-ShrinkPharmDeleteCurrentRow דביק (לשיפור ביצועים,
// מכיוון שמשמש שוב ושוב בלולאה הפנימית) אנחנו צריכים לשחרר אותו לפני
// מעבר לטבלה הבאה.
CloseCursor		(	MAIN_DB, ShrinkPharmSelectCur	);
FreeStatement	(	MAIN_DB, ShrinkPharmDeleteCurrentRow	);

GerrLogFnameMini ("ShrinkPharm_log", GerrId, "Purged %'d rows with %s < %d from %s.",
             TotalRowsDeleted, date_column_name, MinDateToRetain, table_name);
```

---

### TerminateHandler (ShrinkPharm.c:370-430)

```c
void	 TerminateHandler (int signo)
{
    char		*sig_description;
    int			time_now;
    static int	last_signo	= 0;
    static int	last_called	= 0;


    // איפוס טיפול באותות עבור האות שנתפס.
    sigaction (signo, &sig_act_terminate, NULL);

    caught_signal	= signo;
    time_now		= GetTime();


    // זיהוי לולאות אינסופיות וסיום התהליך "ידנית".
    if ((signo == last_signo) && (time_now == last_called))
    {
        GerrLogReturn (GerrId,
                       "As400UnixServer aborting endless loop (signal %d). Terminating process.",
                       signo);

        SQLMD_disconnect ();
        SQLERR_error_test ();

        ExitSonProcess ((signo == 0) ? MAC_SERV_SHUT_DOWN : MAC_EXIT_SIGNAL);
    }
    else
    {
        last_signo	= signo;
        last_called	= time_now;
    }


    // הפקת הודעה ידידותית ואינפורמטיבית בקובץ יומן שרת SQL.
    switch (signo)
    {
        case SIGFPE:
            RollbackAllWork ();
            sig_description = "floating-point error - probably division by zero";
            break;

        case SIGSEGV:
            RollbackAllWork ();
            sig_description = "segmentation error";
            break;

        case SIGTERM:
            sig_description = "user-requested program shutdown";
            break;

        default:
            sig_description = "check manual page for SIGNAL";
            break;
    }

    GerrLogReturn (GerrId,
                   "As400UnixServer received Signal %d (%s). Terminating process.",
                   signo,
                   sig_description);
}
```

**הערת איכות קוד:** הודעות היומן בשורות 388-390 ו-426-429 מפנות בטעות ל-"As400UnixServer" במקום ל-"ShrinkPharm". נראה כי זהו ארטיפקט של העתק-הדבק.

---

## הגדרות אופרטורי SQL

### MacODBC_MyOperators.c - סמן בקרה (שורות 90-99)

```c
case ShrinkPharmControlCur:
            SQL_CommandText		=	"	SELECT		table_name,			days_to_retain,		date_column_name,	"
                                    "				use_where_clause,	commit_count							"
                                    "	FROM		shrinkpharm													"
                                    "	WHERE		purge_enabled	<> 0										"
                                    "	ORDER BY	table_name													";
            CursorName			=	"ShrinkPharmCtrlCur";
            NumOutputColumns	=	5;
            OutputColumnSpec	=	"	char(30), int, char(30), short, int	";
            break;
```

### MacODBC_MyOperators.c - שמירת סטטיסטיקות (שורות 102-110)

```c
case ShrinkPharmSaveStatistics:
            SQL_CommandText		=	"	UPDATE	shrinkpharm					"
                                    "	SET		last_run_date		= ?,	"
                                    "			last_run_time		= ?,	"
                                    "			last_run_num_purged	= ?		"
                                    "	WHERE CURRENT OF ShrinkPharmCtrlCur	";
            NumInputColumns		=	3;
            InputColumnSpec		=	"	int, int, int	";
            break;
```

### MacODBC_MyOperators.c - בחירת כמות (שורות 113-117)

```c
case ShrinkPharmSelectQuantity:
            SQL_CommandText		=	NULL;	// טקסט SQL משתנה מוגדר על ידי הרוטינה הקוראת. פלט יהיה INTEGER אחד (COUNT(*)).
            NumOutputColumns	=	1;
            OutputColumnSpec	=	"	int	";
            break;
```

### MacODBC_MyOperators.c - סמן בחירה (שורות 120-125)

```c
case ShrinkPharmSelectCur:
            SQL_CommandText		=	NULL;	// טקסט SQL משתנה מוגדר על ידי הרוטינה הקוראת. פלט יהיה INTEGER אחד (תאריך).
            CursorName			=	"ShrinkPharmSelCur";
            NumOutputColumns	=	1;
            OutputColumnSpec	=	"	int	";
            break;
```

### MacODBC_MyOperators.c - מחיקת שורה נוכחית (שורות 127-130)

```c
case ShrinkPharmDeleteCurrentRow:
            SQL_CommandText		=	NULL;	// טקסט SQL משתנה מוגדר על ידי הרוטינה הקוראת.
            StatementIsSticky	=	1;
            break;
```

---

## תצורת בנייה

### Makefile (שורות 1-26)

```makefile
include ../Util/Defines.mak

# Files
# ----
SRCS	=	ShrinkPharm.c
OBJS	=	ShrinkPharm.o
#INTERMS	=	ShrinkPharm.c	#Marianna 24.05.2020
TARGET = ShrinkPharm.exe

include  ../Util/Rules.mak

# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET):  $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc -D_GNU_SOURCE

ShrinkPharm.o			:	ShrinkPharm.c
	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	ShrinkPharm.o	ShrinkPharm.c -lodbc -D_GNU_SOURCE

clean:
	rm $(TARGET) $(OBJS)
```

---

## מקום שמור לפסוקיות WHERE מותאמות

### MacODBC_MyCustomWhereClauses.c (שורות 1-10)

```c


/*
		case WHERE_TEMPMEMB:	// Dummy רק להדגים מה צריך להגדיר.
					WhereClauseText		=	"	cardid = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		= "int";
					break;
*/

```

קובץ זה מכיל רק דוגמה כהערה, המציין שזהו מקום שמור להגדרות פסוקיות WHERE מותאמות עתידיות.

---

## חישוב סטטיסטיקות זמן ריצה (ShrinkPharm.c:330-356)

```c
// חישוב זמן ריצה כולל בשניות, ואז המרה לדקות לפלט.
// אם החישוב ההתחלתי נותן לנו משך ריצה שלילי, הריצה עברה
// חצות - אז הוסף יום אחד (בשניות).
EndTime = GetTime ();
RunLenMinutes  = (3600 * (EndTime   / 10000)) + (60 * ((EndTime   % 10000) / 100)) + (EndTime   % 100);
RunLenMinutes -= (3600 * (StartTime / 10000)) + (60 * ((StartTime % 10000) / 100)) + (StartTime % 100);

if (RunLenMinutes < 0)
    RunLenMinutes += 24 * 60 * 60;

// זה לא אמור לקרות בשום תרחיש ריאליסטי, אבל רק למקרה,
// הימנעות מחלוקה באפס.
if (RunLenMinutes == 0)
    RunLenMinutes = 1;


// חישוב קצב מחיקות לדקה. שים לב שאנחנו מחשבים על בסיס שניות *לפני*
// שאנחנו מעגלים לקבלת מספר דקות הריצה!
DeletionsPerMinute = (RowsDeletedFullRun * 60) / RunLenMinutes;

// המרת אורך ריצה לדקות, עם עיגול למעלה.
RunLenMinutes = (RunLenMinutes + 30) / 60;


GerrLogFnameMini ("ShrinkPharm_log", GerrId, "\nShrinkPharm.exe finished %d at %d.", SysDate, SysTime);
GerrLogFnameMini ("ShrinkPharm_log", GerrId, "%'d total rows deleted in %'d minutes.", RowsDeletedFullRun, RunLenMinutes);
GerrLogFnameMini ("ShrinkPharm_log", GerrId, "%'d average rows deleted / minute.", DeletionsPerMinute);
```

**הערה:** נראה כי ערכי זמן מאוחסנים כמספרים שלמים בפורמט HHMMSS (לדוגמה, 143025 = 14:30:25).

---

*מסמך נוצר על ידי סוכן המתעד של CIDRA*
*מזהה משימה: DOC-SHRINK-001*
