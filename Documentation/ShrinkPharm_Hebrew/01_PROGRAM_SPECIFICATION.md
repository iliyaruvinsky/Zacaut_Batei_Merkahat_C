# ShrinkPharm - מפרט התוכנית

**מזהה מסמך:** DOC-SHRINK-001
**תאריך יצירה:** 2026-02-03
**מקור:** source_code/ShrinkPharm/ (574 שורות מקור מאומתות)

---

## סקירה כללית

ShrinkPharm הוא כלי תחזוקת מסד נתונים עצמאי עבור מערכת הבריאות של מכבי. על פי כותרת הקובץ ב-`ShrinkPharm.c:7-10`, הוא מתואר כ-"מקבילה ל-ShrinkDoctor מבוססת ODBC לשימוש באפליקציית בית המרקחת MS-SQL."

נראה כי תוכנית זו היא כלי אצווה שמוחק שורות ישנות מטבלאות מסד נתונים מוגדרות על בסיס מדיניות שמירת נתונים לפי תאריך. בניגוד לתהליכי שרת הפועלים לאורך זמן במאגר קוד זה (כגון SqlServer או As400UnixServer), ShrinkPharm אינו משתמש בתבנית האתחול `InitSonProcess()` - הוא מתחבר ישירות למסד הנתונים דרך `SQLMD_connect()` ויוצא עם השלמת המשימה.

---

## מטרה

על בסיס ניתוח הקוד, נראה כי ShrinkPharm:

1. מתחבר למסד הנתונים דרך ODBC (`ShrinkPharm.c:128-137`)
2. קורא תצורת מחיקה מטבלת בקרה בשם `shrinkpharm` (`MacODBC_MyOperators.c:90-99`)
3. עבור כל שורת תצורה מופעלת, מוחק שורות מסד נתונים ישנות מתקופת השמירה המוגדרת
4. מתעד סטטיסטיקות ביצוע בחזרה לטבלת הבקרה
5. רושם תוצאות פעולה ביומן ויוצא

---

## מבנה קבצים

| קובץ | שורות | מטרה |
|------|------:|------|
| ShrinkPharm.c | 431 | תוכנית ראשית - חיבור למסד נתונים, לוגיקת מחיקה, סטטיסטיקות |
| MacODBC_MyOperators.c | 133 | הגדרות אופרטורי ODBC SQL (5 אופרטורים) |
| MacODBC_MyCustomWhereClauses.c | 10 | מקום שמור לפסוקיות WHERE מותאמות (כרגע לא בשימוש) |
| Makefile | 26 | תצורת בנייה ל-Unix |
| **סה"כ מקור** | **574** | ספירה מדויקת מאומתת |
| **עם בנייה** | **600** | כולל Makefile |

---

## פונקציות

| פונקציה | קובץ | שורות | מטרה |
|---------|------|------:|------|
| main | ShrinkPharm.c | 35-359 (324 שורות) | נקודת כניסה - מתחבר ל-DB, קורא תצורת מחיקה, עובר על טבלאות יעד, מוחק שורות ישנות, מתעד סטטיסטיקות |
| TerminateHandler | ShrinkPharm.c | 370-430 (60 שורות) | מטפל אותות עבור SIGFPE, SIGSEGV, SIGTERM - מבצע rollback ורושם סיום ביומן |

---

## משתנים גלובליים

על פי הקוד ב-`ShrinkPharm.c:14-32`:

| משתנה | סוג | ערך התחלתי | מטרה | הערות |
|-------|-----|------------|------|-------|
| GerrSource | char* | __FILE__ | זיהוי מקור לרישום שגיאות | שורה 14 |
| restart_flg | static int | 0 | נראה לא בשימוש | שורה 24 |
| caught_signal | int | 0 | מאחסן מספר אות שנתפס | שורה 25 |
| TikrotProductionMode | int | 1 | דגל מצב ייצור לעומת בדיקה | שורה 26 |
| sig_act_terminate | struct sigaction | (לא מאותחל) | תצורת מטפל אותות | שורה 28 |
| need_rollback | static int | 0 | נראה לא בשימוש | שורה 30 |
| recs_to_commit | static int | 0 | נראה לא בשימוש | שורה 31 |
| recs_committed | static int | 0 | נראה לא בשימוש | שורה 32 |

**הערה:** שלושה משתנים גלובליים (`need_rollback`, `recs_to_commit`, `recs_committed`) נראים מוצהרים אך לא בשימוש בקוד הנוכחי.

---

## הגדרות פרה-פרוססור

| הוראה | שורה | מטרה |
|-------|-----:|------|
| #define MAIN | 13 | מסמן זהו קובץ התוכנית הראשי |
| #include \<MacODBC.h\> | 16 | קובץ כותרת שכבת תשתית ODBC |

---

## תלויות

### קבצי כותרת (מ-ShrinkPharm.c:16)

| כותרת | מספק |
|-------|------|
| MacODBC.h | שכבת הפשטה של מסד נתונים ODBC (כולל GenSql.h) |

### ספריות חיצוניות (מ-Makefile:19)

| ספריה | מטרה |
|-------|------|
| -lodbc | קישוריות מסד נתונים ODBC |

### פונקציות GenLib בשימוש

על בסיס ניתוח הקוד:

| פונקציה | מטרה | דוגמת שימוש |
|---------|------|-------------|
| GetDate() | קבלת תאריך מערכת נוכחי כמספר שלם | `ShrinkPharm.c:69` |
| GetTime() | קבלת שעת מערכת נוכחית כמספר שלם | `ShrinkPharm.c:70` |
| IncrementDate() | הוספה/הפחתה ימים מתאריך | `ShrinkPharm.c:189` |
| strip_spaces() | הסרת רווחים מובילים/נגררים ממחרוזת | `ShrinkPharm.c:187-188` |
| GerrLogFnameMini() | כתיבה לקובץ יומן בשם נתון | `ShrinkPharm.c:75-77` |
| GerrLogReturn() | רישום שגיאה וחזרה | `ShrinkPharm.c:113-115` |
| ExitSonProcess() | יציאה עם קוד סטטוס | `ShrinkPharm.c:395` |

### פונקציות GenSql/ODBC בשימוש

| פונקציה | מטרה | דוגמת שימוש |
|---------|------|-------------|
| SQLMD_connect() | חיבור למסד נתונים | `ShrinkPharm.c:130` |
| SQLMD_disconnect() | ניתוק ממסד נתונים | `ShrinkPharm.c:328` |
| DeclareAndOpenCursorInto() | הצהרה ופתיחת סמן עם משתני פלט | `ShrinkPharm.c:147-150` |
| FetchCursor() | שליפת שורה הבאה מסמן | `ShrinkPharm.c:160` |
| CloseCursor() | סגירת סמן | `ShrinkPharm.c:312, 322` |
| ExecSQL() | הרצת הצהרת SQL | `ShrinkPharm.c:199, 266, 306-307` |
| FreeStatement() | שחרור הצהרה מוכנה | `ShrinkPharm.c:313` |
| CommitAllWork() | אישור עסקה | `ShrinkPharm.c:288, 299, 327` |
| RollbackAllWork() | ביטול עסקה | `ShrinkPharm.c:408, 413` |
| SQLERR_error_test() | בדיקת שגיאות SQL | `ShrinkPharm.c:152` |
| SQLERR_code_cmp() | השוואת SQLCODE עם קבוע | `ShrinkPharm.c:162` |

---

## אופרטורי SQL

על פי `MacODBC_MyOperators.c:90-130`:

| מזהה אופרטור | שם סמן | מטרה |
|-------------|--------|------|
| ShrinkPharmControlCur | ShrinkPharmCtrlCur | SELECT תצורות מחיקה מופעלות מטבלת shrinkpharm |
| ShrinkPharmSaveStatistics | (אין) | UPDATE של shrinkpharm עם סטטיסטיקות ריצה |
| ShrinkPharmSelectQuantity | (אין) | שאילתת COUNT דינמית לחיזוי כמות |
| ShrinkPharmSelectCur | ShrinkPharmSelCur | סמן SELECT דינמי לשורות למחיקה |
| ShrinkPharmDeleteCurrentRow | (אין) | DELETE דינמי WHERE CURRENT OF (דביק) |

---

## פרמטרי תצורה

### הגדרות מסד נתונים (ShrinkPharm.c:124-126)

| פרמטר | ערך | מטרה |
|-------|-----|------|
| LOCK_TIMEOUT | 1000 | זמן קצוב לנעילה במילישניות |
| DEADLOCK_PRIORITY | -2 | מתחת לרגיל (טווח -10 עד 10) להעדפת פעולות זמן-אמת |
| ODBC_PRESERVE_CURSORS | 1 | שימור סמנים לאחר פעולות COMMIT |

### תצורת זמן ריצה

| הגדרה | מקור | מטרה |
|-------|------|------|
| ספריית יומן | getenv("MAC_LOG") | ספרייה לקבצי יומן (`ShrinkPharm.c:72`) |
| שם קובץ יומן | "ShrinkPharm_log" | שם קובץ יומן מקודד קשיח (`ShrinkPharm.c:73`) |

### זיהוי בדיקה/ייצור (ShrinkPharm.c:79-89)

התוכנית מזהה סביבות בדיקה לפי שם מארח. אם שם המארח מתאים לאחד מהערכים הבאים, `TikrotProductionMode` מוגדר ל-0:
- linux01-test
- linux01-qa
- pharmlinux-test
- pharmlinux-qa

---

## מגבלות תיעוד

### מה לא ניתן לקבוע מהקוד בלבד:

- הסכמה והממשל המדויקים של טבלת הבקרה `shrinkpharm` (DDL, מי יכול לערוך אותה)
- אילו טבלאות יעד מוגדרות למחיקה בייצור
- כיצד ומתי ShrinkPharm מתוזמן לרוץ (cron, systemd, מתזמן משימות, או הפעלה ידנית)
- הקשר עם תוכנית "ShrinkDoctor" המקורית המוזכרת בהערות

### מה ידוע מהקוד:

- ShrinkPharm הוא כלי אצווה עצמאי (לא שרת פועל לאורך זמן)
- הוא קורא תצורה מטבלת מסד הנתונים `shrinkpharm`
- הוא מוחק שורות על בסיס השוואות עמודת תאריך
- הוא מבצע commit באצוות ומתעד סטטיסטיקות
- הוא משתמש בעדיפות deadlock מתחת לרגיל להעדפת פעולות זמן-אמת

---

*מסמך נוצר על ידי סוכן המתעד של CIDRA*
*מזהה משימה: DOC-SHRINK-001*
