# ShrinkPharm - ניתוח טכני

**מזהה מסמך:** DOC-SHRINK-001
**תאריך יצירה:** 2026-02-03
**מקור:** source_code/ShrinkPharm/ (574 שורות מקור מאומתות)

---

## ניתוח פונקציות

### main() - נקודת כניסה

**מיקום:** `ShrinkPharm.c:35-359` (325 שורות)

**מטרה:** תזמור פעולת המחיקה כולה מהפעלה ועד יציאה.

**זרימת בקרה:**

1. **אתחול (שורות 68-116)**
   - קבלת מזהה תהליך, תאריך/שעה נוכחיים
   - הגדרת ספרייה וקובץ רישום יומן
   - זיהוי סביבת ייצור לעומת בדיקה לפי שם מארח
   - התקנת מטפל אותות עבור SIGFPE

2. **חיבור למסד נתונים (שורות 119-137)**
   - הגדרת פרמטרי ODBC (LOCK_TIMEOUT, DEADLOCK_PRIORITY, ODBC_PRESERVE_CURSORS)
   - ניסיון SQLMD_connect() עד MAIN_DB->Connected
   - שינה 10 שניות בין ניסיונות

3. **לולאת עיבוד ראשית (שורות 140-319)**
   - פתיחת סמן בקרה (ShrinkPharmControlCur) לקריאת תצורות מחיקה מופעלות
   - לכל שורת תצורה:
     - הסרת רווחים מ-table_name ו-date_column_name
     - חישוב MinDateToRetain = SysDate - days_to_retain
     - הרצת שאילתת COUNT אופציונלית לחיזוי כמות
     - פתיחת סמן בחירה (ShrinkPharmSelectCur)
     - מחיקת שורות באמצעות WHERE CURRENT OF cursor
     - Commit כל commit_count מחיקות
     - שמירת סטטיסטיקות בחזרה לטבלת shrinkpharm
     - שחרור הצהרה דביקה

4. **ניקוי ויציאה (שורות 322-358)**
   - סגירת סמן בקרה
   - Commit סופי
   - ניתוק ממסד נתונים
   - חישוב ורישום סטטיסטיקות ריצה ביומן
   - יציאה עם קוד 0

**משתנים מקומיים:**

| משתנה | סוג | מטרה | שורה |
|-------|-----|------|------|
| retrys | int | (מוצהר אך נראה לא בשימוש) | 37 |
| mypid | int | מזהה תהליך | 38 |
| TotalRowsDeleted | int | ספירת שורות שנמחקו לטבלה | 40 |
| RowsDeletedSinceCommit | int | שורות מאז commit אחרון | 41 |
| RowsDeletedFullRun | int | סה"כ שורות שנמחקו בכל הטבלאות | 42 |
| SysDate | int | תאריך מערכת נוכחי | 43 |
| SysTime | int | שעת מערכת נוכחית | 44 |
| StartTime | int | שעת התחלת ריצה | 45 |
| EndTime | int | שעת סיום ריצה | 46 |
| RunLenMinutes | int | חישוב משך | 47 |
| DeletionsPerMinute | int | חישוב קצב | 48 |
| table_name | char[31] | משורת shrinkpharm | 53 |
| date_column_name | char[31] | משורת shrinkpharm | 54 |
| use_where_clause | short | דגל משורת shrinkpharm | 55 |
| days_to_retain | int | תקופת שמירה | 56 |
| commit_count | int | גודל אצווה | 57 |
| MinDateToRetain | int | תאריך חיתוך מחושב | 60 |
| DateOfRow | int | ערך תאריך משורת יעד | 61 |
| SelectCursorCommand | char[601] | מאגר SQL דינמי | 63 |
| PredictQtyCommand | char[601] | מאגר SQL דינמי | 64 |
| DeleteCommand | char[301] | מאגר SQL דינמי | 65 |

---

### TerminateHandler() - מטפל אותות

**מיקום:** `ShrinkPharm.c:370-430` (61 שורות)

**מטרה:** טיפול באותות קטלניים/מסיימים עם ניקוי מסודר.

**פרמטרים:**
- `signo` (int): מספר האות שהתקבל

**זרימת בקרה:**

1. **איפוס טיפול באותות (שורה 379)**
   - התקנה מחדש של מטפל דרך sigaction

2. **זיהוי לולאה אינסופית (שורות 386-396)**
   - אם אותו אות באותו זמן כמו קריאה אחרונה, ביטול מיידי
   - מונע רקורסיה אינסופית בטיפול באותות

3. **פעולות ספציפיות לאות (שורות 405-424)**
   - SIGFPE: RollbackAllWork, תיאור כ-"floating-point error"
   - SIGSEGV: RollbackAllWork, תיאור כ-"segmentation error"
   - SIGTERM: ללא rollback, תיאור כ-"user-requested shutdown"
   - ברירת מחדל: "check manual page for SIGNAL"

4. **רישום יומן ויציאה (שורות 426-429)**
   - רישום הודעה עם מספר ותיאור האות

**משתנים סטטיים:**

| משתנה | סוג | מטרה | שורה |
|-------|-----|------|------|
| last_signo | int | מספר אות קודם | 374 |
| last_called | int | זמן קריאה קודמת | 375 |

**הערת איכות קוד:** על פי הקוד בשורות 388-390 ו-426-429, הודעות היומן מפנות בטעות ל-"As400UnixServer" במקום ל-"ShrinkPharm". נראה כי זהו ארטיפקט של העתק-הדבק מרכיב אחר.

---

## ניתוח אופרטורי SQL

על פי `MacODBC_MyOperators.c:90-130`:

### ShrinkPharmControlCur (שורות 90-99)

**מטרה:** קריאת תצורות מחיקה מופעלות מטבלת בקרה

**SQL:**
```sql
SELECT table_name, days_to_retain, date_column_name,
       use_where_clause, commit_count
FROM   shrinkpharm
WHERE  purge_enabled <> 0
ORDER BY table_name
```

**תצורה:**
- CursorName: "ShrinkPharmCtrlCur"
- NumOutputColumns: 5
- OutputColumnSpec: "char(30), int, char(30), short, int"

### ShrinkPharmSaveStatistics (שורות 102-110)

**מטרה:** עדכון סטטיסטיקות ביצוע בטבלת בקרה

**SQL:**
```sql
UPDATE shrinkpharm
SET    last_run_date = ?,
       last_run_time = ?,
       last_run_num_purged = ?
WHERE CURRENT OF ShrinkPharmCtrlCur
```

**תצורה:**
- NumInputColumns: 3
- InputColumnSpec: "int, int, int"

### ShrinkPharmSelectQuantity (שורות 113-117)

**מטרה:** הרצת שאילתת COUNT דינמית לחיזוי כמות מחיקה

**SQL:** NULL (מסופק דינמית על ידי הקורא)

**דוגמת SQL דינמי (מ-ShrinkPharm.c:195-197):**
```sql
SELECT CAST(count(*) AS INTEGER) FROM <table> WHERE <datecol> < <MinDateToRetain>
```

**תצורה:**
- NumOutputColumns: 1
- OutputColumnSpec: "int"

### ShrinkPharmSelectCur (שורות 120-125)

**מטרה:** פתיחת סמן לסריקת שורות למחיקה

**SQL:** NULL (מסופק דינמית על ידי הקורא)

**דוגמאות SQL דינמי (מ-ShrinkPharm.c:204-212):**

עם פסוקית WHERE:
```sql
SELECT <datecol> FROM <table> WHERE <datecol> < <MinDateToRetain>
```

ללא פסוקית WHERE:
```sql
SELECT <datecol> FROM <table>
```

**תצורה:**
- CursorName: "ShrinkPharmSelCur"
- NumOutputColumns: 1
- OutputColumnSpec: "int"

### ShrinkPharmDeleteCurrentRow (שורות 127-130)

**מטרה:** מחיקת השורה הנוכחית מסמן הבחירה

**SQL:** NULL (מסופק דינמית על ידי הקורא)

**SQL דינמי (מ-ShrinkPharm.c:215):**
```sql
DELETE FROM <table> WHERE CURRENT OF ShrinkPharmSelCur
```

**תצורה:**
- StatementIsSticky: 1 (לביצועים בלולאה הפנימית)

---

## מבני נתונים

### שורת טבלת בקרה (מסקנה מ-SQL)

על בסיס `MacODBC_MyOperators.c:91-99`:

| עמודה | סוג | מטרה |
|-------|-----|------|
| table_name | char(30) | טבלת יעד למחיקה |
| days_to_retain | int | תקופת שמירה בימים |
| date_column_name | char(30) | עמודת תאריך להשוואה |
| use_where_clause | short | דגל: 1=שימוש ב-WHERE ב-SELECT, 0=סינון בקוד |
| commit_count | int | גודל אצווה ל-commit |
| purge_enabled | (לא נבחר) | מסנן: <> 0 אומר מופעל |
| last_run_date | int | מעודכן על ידי ShrinkPharmSaveStatistics |
| last_run_time | int | מעודכן על ידי ShrinkPharmSaveStatistics |
| last_run_num_purged | int | מעודכן על ידי ShrinkPharmSaveStatistics |

### מבנה Signal Action

על פי `ShrinkPharm.c:28`:

```c
struct sigaction sig_act_terminate;
```

מוגדר בשורות 100-102:
- sa_handler = TerminateHandler
- sa_mask = NullSigset (ריק)
- sa_flags = 0

---

## טיפול בשגיאות

### זיהוי שגיאות SQL

| פונקציה | מטרה | דוגמת שימוש |
|---------|------|-------------|
| SQLERR_error_test() | בדיקה אם התרחשה שגיאה | `ShrinkPharm.c:152, 168, 231, 247, 268` |
| SQLERR_code_cmp() | השוואת SQLCODE עם קבוע | `ShrinkPharm.c:162, 241, 276` |

### קבועי שגיאה בשימוש

| קבוע | משמעות | שימוש |
|------|--------|-------|
| SQLERR_end_of_fetch | אין עוד שורות | יציאה מלולאת שליפה |
| SQLERR_no_rows_affected | DELETE לא השפיע | רישום אזהרה |

### דפוס זרימת שגיאות

על פי הקוד ב-`ShrinkPharm.c:152-171`:

```c
if (SQLERR_error_test()) {
    break;  // יציאה מלולאה נוכחית בשגיאה
}
```

התוכנית משתמשת בדפוס "do { ... } while(0)" (שורות 140-320) לאפשר טיפול נקי בשגיאות עם הצהרות break.

---

## שיקולי ביצועים

### אסטרטגיית Commit באצוות

על פי הקוד ב-`ShrinkPharm.c:286-291`:

```c
if (RowsDeletedSinceCommit >= commit_count) {
    CommitAllWork ();
    RowsDeletedSinceCommit = 0;
}
```

גישה זו של אצוות:
- מפחיתה גידול יומן טרנזקציות
- מאפשרת התקדמות חלקית אם מופסק
- ניתנת להגדרה לכל טבלה דרך עמודת commit_count

### אופטימיזציית הצהרה דביקה

על פי `MacODBC_MyOperators.c:127-130`:

```c
case ShrinkPharmDeleteCurrentRow:
    SQL_CommandText = NULL;
    StatementIsSticky = 1;  // הימנעות מתקורת PREPARE חוזרת
    break;
```

הצהרת ה-DELETE מסומנת "דביקה" להימנעות מתקורת PREPARE חוזרת בלולאת המחיקה הפנימית. היא משוחררת לאחר עיבוד כל טבלה (`ShrinkPharm.c:313`).

### ODBC_PRESERVE_CURSORS

על פי הקוד ב-`ShrinkPharm.c:126`:

```c
ODBC_PRESERVE_CURSORS = 1;  // כך כל הסמנים יישמרו לאחר COMMITs.
```

זה מאפשר לסמן הבקרה החיצוני להישאר תקף לאורך commit-ים באצוות, מה שמאפשר את דפוס הסמנים המקוננים.

---

## הערות חוסן

### בעיות פוטנציאליות שזוהו

1. **סיכון מצביע NULL (שורה 72)**
   - `strcpy(LogDir, getenv("MAC_LOG"))` עלול לקרוס אם MAC_LOG לא מוגדר

2. **ארטיפקט העתק-הדבק (שורות 388-390, 426-429)**
   - הודעות יומן מפנות ל-"As400UnixServer" במקום ל-"ShrinkPharm"

3. **משתנים לא בשימוש (שורות 30-32)**
   - `need_rollback`, `recs_to_commit`, `recs_committed` מוצהרים אך לא בשימוש

4. **SQL דינמי מערכי DB (שורות 195-216)**
   - שמות טבלאות ועמודות משובצים מ-DB ללא אימות
   - אם טבלת הבקרה נפגעת, טבלאות שרירותיות עלולות להיות מושפעות

### אמצעי הגנה קיימים

1. **זיהוי לולאה אינסופית (שורות 386-396)**
   - מונע רקורסיה אינסופית בטיפול באותות

2. **Commit-ים באצוות (שורות 286-291, 297-301)**
   - התקדמות חלקית נשמרת אם מופסק

3. **עדיפות Deadlock מתחת לרגיל (שורה 125)**
   - מוותר לפעולות זמן-אמת בתחרות

---

*מסמך נוצר על ידי סוכן המתעד של CIDRA*
*מזהה משימה: DOC-SHRINK-001*
