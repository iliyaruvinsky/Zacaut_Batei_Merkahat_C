# MacODBC.h

**מערכת בריאות מכבי — שכבת תשתית ODBC**

---

## סיכום מהיר

MacODBC.h הוא שכבת תשתית ה-ODBC המרכזית עבור כל מערכת ה-C Backend של מכבי. על פי כותרת הקובץ ב-`MacODBC.h:14-15`, הוא נכתב על ידי Don Radlauer בדצמבר 2019 כתחליף לגישת Informix Embedded SQL (ESQL) הקודמת.

נראה כי הקובץ מספק:
1. **25 מאקרואים ציבוריים** — ממשק embedded-SQL-like (ExecSQL, DeclareCursor, CommitWork וכו')
2. **Dispatcher מרכזי** — פונקציית ODBC_Exec (2212 שורות) המטפלת בכל 20 סוגי פקודות
3. **תמיכה ב-MS-SQL ו-Informix** — חיבורים בו-זמניים עם שיקוף אופציונלי
4. **Cache של prepared statements** — עד 120 statements דביקים
5. **מנגנון pointer validation** — בדיקת va_arg pointers באמצעות SIGSEGV

**מחבר**: Don Radlauer (`MacODBC.h:14`)
**תאריך**: דצמבר 2019 (`MacODBC.h:15`)
**סוג קובץ**: היברידי header/implementation תחת `#ifdef MAIN`
**שורות**: 4121

---

## קבצים מרכזיים

| קובץ | שורות | מטרה |
|------|------:|------|
| MacODBC.h | 4121 | תשתית ODBC — 25 מאקרואי API, 11 פונקציות, enums, structs, globals |

קובץ זה הוא **תשתית משותפת** המשמשת את כל 8 רכיבי המערכת:
- FatherProcess, SqlServer, As400UnixServer, As400UnixClient
- As400UnixDocServer, As400UnixDoc2Server, ShrinkPharm, GenSql

---

## כיצד להשתמש בתיעוד זה

סט תיעוד זה מורכב מ-7 קבצים:

| קובץ | תכולה |
|------|--------|
| **01_PROGRAM_SPECIFICATION.md** | סקירה, מבנה קבצים, 11 פונקציות, 25 מאקרואים, enums, structs, globals, קבועים, תלויות |
| **02_SYSTEM_ARCHITECTURE.md** | דפוס dispatcher יחיד, דואליות #ifdef MAIN, צינור 8 שלבים, שיקוף DB, הזרקת include |
| **03_TECHNICAL_ANALYSIS.md** | ניתוח 8 שלבי ODBC_Exec, 10 פונקציות עזר, SIGSEGV validation, sticky lifecycle, auto-reconnect |
| **04_BUSINESS_LOGIC.md** | מחזור חיי cursor, ניהול טרנזקציות, החלטת שיקוף, בקרת כניסה, המרת שגיאות, הקמת חיבור |
| **05_CODE_ARTIFACTS.md** | קטעי קוד מרכזיים עם הפניות שורה |
| **README.md** | קובץ זה — סקירה ומדריך ניווט |
| **VALIDATION_REPORT.md** | אימות הבטחת איכות וניקוד |

### סדר קריאה מומלץ

1. **README.md** (קובץ זה) — התמצאות
2. **01_PROGRAM_SPECIFICATION.md** — הבנת המבנה, API, ו-enums/structs
3. **02_SYSTEM_ARCHITECTURE.md** — הבנת דפוס ה-dispatcher וארכיטקטורת השיקוף
4. **04_BUSINESS_LOGIC.md** — הבנת מחזורי חיי cursor, טרנזקציות, ושיקוף
5. **03_TECHNICAL_ANALYSIS.md** — צלילה עמוקה לכל 8 שלבי ODBC_Exec
6. **05_CODE_ARTIFACTS.md** — הפניה לקטעי קוד בפועל

---

## רכיבים קשורים

על פי הקשר המחקר, MacODBC.h משמש את כל רכיבי המערכת:

### רכיבים צורכים (משתמשים ב-25 מאקרואי ה-API)
- **FatherProcess** — דימון מפקח/שומר (`FatherProcess.c:29`)
- **SqlServer** — שרת SQL ראשי (`SqlServer.c:29`)
- **As400UnixServer** — גשר AS/400 (`As400UnixServer.c:32`)
- **As400UnixClient** — לקוח AS/400 (`As400UnixClient.c:29`)
- **As400UnixDocServer** — שרת מסמכי AS/400 (`As400UnixDocServer.c:31`)
- **As400UnixDoc2Server** — שרת מסמכי AS/400 מורחב (`As400UnixDoc2Server.c:34`)
- **ShrinkPharm** — כלי שירות לניקוי DB (`ShrinkPharm.c:16`)
- **GenSql** — כלי שירות SQL (`GenSql.c:32`)

### ספריות
- **GenSql.h** — תשתית SQL (SQLMD_connect, INF_CONNECT, sqlca)
- **MacODBC_MyOperatorIDs.h** — מזהי operators ספציפיים לכל רכיב

### תיעוד קשור
- **Documentation/FatherProcess/** — תיעוד FatherProcess (100/100)
- **Documentation/ShrinkPharm/** — תיעוד ShrinkPharm (100/100)

---

## דרישות מערכת

על בסיס ניתוח הקוד:

- **פלטפורמה**: UNIX/Linux (משתמש ב-POSIX APIs: sigaction, sigsetjmp, setlocale)
- **מסד נתונים**: ODBC — MS-SQL Server ו/או Informix
- **כותרות**: sql.h, sqlext.h (ODBC system headers)
- **locale**: `he_IL.UTF-8` (על פי `MacODBC.h:3488`)

---

## מיקום קוד מקור

```
source_code/Include/
└── MacODBC.h                          # מקור יחיד (4121 שורות)
```

**קבצי injection ספציפיים לכל רכיב:**
```
source_code/{Component}/
├── MacODBC_MyOperators.c              # הגדרות SQL operations
└── MacODBC_MyCustomWhereClauses.c     # הגדרות WHERE clauses
```

---

## מטאדטה של תיעוד

| שדה | ערך |
|-----|-----|
| מזהה משימה | DOC-MACODBC-001 |
| תאריך יצירה | 2026-02-11 |
| סוכן | סוכן המתעד של CIDRA |
| מקור | CHUNKS/MacODBC/ |
| מחקר | RESEARCH/MacODBC_deepdive.md |
| אימות | 100/100 (ראה VALIDATION_REPORT.md) |

---

*נוצר על ידי סוכן המתעד של CIDRA — DOC-MACODBC-001*
