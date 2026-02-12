# MacODBC.h - ארכיטקטורת המערכת

**רכיב**: MacODBC (שכבת תשתית ODBC)
**מזהה משימה**: DOC-MACODBC-001
**תאריך יצירה**: 2026-02-11

---

## דפוס Dispatcher יחיד

על פי ניתוח הקוד, נראה כי MacODBC.h בנוי סביב דפוס dispatcher מרכזי יחיד:

```
25 מאקרואים ציבוריים (MacODBC.h:311-335)
        │
        ▼
   ODBC_Exec (MacODBC.h:446-2657, 2212 שורות)
        │
        ├─── ODBC_CommandType enum (20 ערכים, MacODBC.h:117-139)
        │
        └─── switch מרכזי (MacODBC.h:634-763)
             מפנה כל CommandType להתנהגות ספציפית
```

כל 25 המאקרואים הציבוריים (DeclareCursor, ExecSQL, CommitWork וכו') מנתבים ל-`ODBC_Exec` עם ערך `ODBC_CommandType` מתאים, כפי שנצפה ב-`MacODBC.h:311-335`.

---

## דואליות #ifdef MAIN

על פי מבנה הקובץ, MacODBC.h הוא קובץ היברידי header/implementation:

```
MacODBC.h
├── שורות 1-166:    הצהרות (enums, structs, includes) — תמיד פעילות
├── שורות 168-241:  #ifdef MAIN — אחסון משתנים גלובליים
├── שורות 242-273:  #else — הצהרות extern
├── שורות 276-420:  מאקרואים והצהרות פונקציות — תמיד פעילות
├── שורות 422-4120: #ifdef MAIN — 11 מימושי פונקציות
└── שורה 4121:      #endif MacODBC_H
```

נראה כי רכיב ה-`.c` שמגדיר `#define MAIN` לפני `#include <MacODBC.h>` מקבל את כל המימוש מקומפל לתוכו. כל רכיבי המערכת עושים זאת, כפי שנצפה ב-`RESEARCH/MacODBC_deepdive.md`.

---

## צינור 8 שלבים של ODBC_Exec

על פי ניתוח `ODBC_Exec` (שורות 446-2657), נראה כי הפונקציה עוברת 8 שלבים סדרתיים:

```
┌───────────────────────────────────────────────────────────────┐
│  שלב 1: פענוח פקודה ואתחול        (שורות 422-772)          │
│  ├── va_start, memset, אתחול דגלים                          │
│  └── switch על CommandType → הגדרת דגלי התנהגות              │
├───────────────────────────────────────────────────────────────┤
│  שלב 2: אתחול קריאה ראשונה        (שורות 775-887)          │
│  ├── memset של מערכי cache                                   │
│  ├── התקנת signal handler (SIGSEGV) באמצעות sigaction        │
│  └── בדיקת גודל bool בזמן ריצה (sizeof(bool) → BoolType)   │
├───────────────────────────────────────────────────────────────┤
│  שלב 3: טעינת מטאדטה של פעולה     (שורות 888-1072)         │
│  ├── SQL_GetMainOperationParameters → טקסט SQL, עמודות      │
│  ├── החלטת שיקוף (CommandIsMirrored)                        │
│  ├── find_FOR_UPDATE_or_GEN_VALUES → זיהוי סוג פקודה        │
│  └── SQL_GetWhereClauseParameters → WHERE clause אופציונלי   │
├───────────────────────────────────────────────────────────────┤
│  שלב 4: מחזור חיי Statement דביק   (שורות 1075-1231)       │
│  ├── טיפול בהחלפת ספק DB                                    │
│  ├── בקרת כניסה (admission control) — מקסימום 120            │
│  ├── וולידציית prepared-state (SQLNumParams)                  │
│  └── הקצאת SQLAllocStmt (ראשי + mirror)                      │
├───────────────────────────────────────────────────────────────┤
│  שלב 5: בניית SQL ו-PREPARE        (שורות 1233-1447)       │
│  ├── הרכבת SQL_CmdBuffer                                     │
│  ├── SQL_CustomizePerDB → החלפת טוקנים ספציפיים לספק         │
│  ├── SQLPrepare — כישלון מוביל ל-auto-reconnect               │
│  └── Mirror PREPARE על DB חלופי                               │
├───────────────────────────────────────────────────────────────┤
│  שלב 6: וולידציית מצביעים ו-Binding (שורות 1449-1976)      │
│  ├── איסוף מצביעים עם SIGSEGV validation                     │
│  ├── SQLBindCol לעמודות פלט                                   │
│  ├── SQLBindParameter לעמודות קלט                             │
│  └── SQLBindParameter ל-WHERE clause + mirror                 │
├───────────────────────────────────────────────────────────────┤
│  שלב 7: Execute, Mirror, Fetch     (שורות 1978-2288)       │
│  ├── SQLExecute על mirror (קודם!)                             │
│  ├── SQLExecute על DB ראשי                                    │
│  ├── SQLRowCount + טיפול בשגיאות                              │
│  ├── SQLFetchScroll SQL_FETCH_NEXT                            │
│  └── בדיקת cardinality (fetch שני)                            │
├───────────────────────────────────────────────────────────────┤
│  שלב 8: טרנזקציה, סגירה, החזרה    (שורות 2290-2657)       │
│  ├── Commit/Rollback (ENV level vs DBC level)                 │
│  ├── הגדרת isolation level                                    │
│  ├── SQLCloseCursor / SQLFreeHandle                           │
│  └── va_end, החזרת SQLCODE                                    │
└───────────────────────────────────────────────────────────────┘
```

---

## ארכיטקטורת שיקוף מסד נתונים

על פי הקוד, נראה כי המערכת תומכת בשיקוף כתיבות ל-DB חלופי:

```
┌─────────────┐          ┌──────────────┐
│   MAIN_DB   │          │   ALT_DB     │
│  (ראשי)    │          │  (mirror)    │
└──────┬──────┘          └──────┬───────┘
       │                        │
       │    ODBC_MIRRORING_ENABLED (MacODBC.h:180)
       │         │
       ▼         ▼
┌──────────────────────────────────────┐
│        ODBC_Exec                      │
│                                       │
│  1. CommandIsMirrored נקבע (שורה 918)│
│  2. SELECTs אינם משוקפים (שורה 994) │
│  3. MIRROR_DB = ההפך של Database      │
│     (שורה 1002)                       │
│  4. Mirror statement מוקצה (שורה 1224)│
│  5. Mirror PREPARE (שורה 1402)        │
│  6. Mirror Bind (שורות 1902-1975)     │
│  7. Mirror Execute קודם! (שורה 2029)  │
│  8. Commit: ENV level כש-ALTERNATE_DB │
│     _USED (שורה 2396)                 │
└──────────────────────────────────────┘
```

**נקודות מפתח בשיקוף:**
- על פי `MacODBC.h:2029`, ה-mirror מבוצע **לפני** ה-primary
- כישלונות mirror נרשמים אך אינם fatal, כפי שנצפה ב-`MacODBC.h:2047-2079`
- CommitAllWork משתמש ב-`SQLEndTran` ברמת ENV כשני DBs היו בשימוש, כפי שנצפה ב-`MacODBC.h:2396-2446`

---

## דפוס הזרקת Include

על פי הקוד, MacODBC.h משתמש בדפוס הזרקת `#include` להתאמה לכל רכיב:

```
SQL_GetMainOperationParameters (MacODBC.h:2661-2817)
    │
    └── switch(OperationIdentifier) שורה 2745
        │
        └── #include "MacODBC_MyOperators.c"  ← שורה 2747
            (כל רכיב מספק קובץ משלו)

SQL_GetWhereClauseParameters (MacODBC.h:2821-2902)
    │
    └── switch(WhereClauseIdentifier) שורה 2864
        │
        └── #include "MacODBC_MyCustomWhereClauses.c"  ← שורה 2866
            (כל רכיב מספק קובץ משלו)
```

**קבצי רכיב ידועים** (על פי `CHUNKS/MacODBC/analysis.json`):

| רכיב | MacODBC_MyOperators.c | MacODBC_MyCustomWhereClauses.c |
|-------|----------------------|-------------------------------|
| FatherProcess | `source_code/FatherProcess/` | `source_code/FatherProcess/` |
| SqlServer | `source_code/SqlServer/` | `source_code/SqlServer/` |
| As400UnixServer | `source_code/As400UnixServer/` | `source_code/As400UnixServer/` |
| As400UnixClient | `source_code/As400UnixClient/` | `source_code/As400UnixClient/` |
| As400UnixDocServer | `source_code/As400UnixDocServer/` | `source_code/As400UnixDocServer/` |
| As400UnixDoc2Server | `source_code/As400UnixDoc2Server/` | `source_code/As400UnixDoc2Server/` |
| ShrinkPharm | `source_code/ShrinkPharm/` | `source_code/ShrinkPharm/` |
| GenSql | `source_code/GenSql/` | לא רלוונטי |

---

## אינטגרציית רכיבים

על פי `RESEARCH/MacODBC_deepdive.md`, קבצי המקור הבאים כוללים את MacODBC.h ישירות:

```
source_code/FatherProcess/FatherProcess.c:29
source_code/SqlServer/SqlServer.c:29
source_code/SqlServer/SqlHandlers.c:25
source_code/SqlServer/MessageFuncs.c:32
source_code/SqlServer/ElectronicPr.c:21
source_code/SqlServer/DigitalRx.c:20
source_code/As400UnixClient/As400UnixClient.c:29
source_code/As400UnixServer/As400UnixServer.c:32
source_code/As400UnixDocServer/As400UnixDocServer.c:31
source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:34
source_code/ShrinkPharm/ShrinkPharm.c:16
source_code/GenSql/GenSql.c:32
```

---

## זרימת נתונים

```
GenSql.c:INF_CONNECT()
        │
        ▼
ODBC_CONNECT (MacODBC.h:3443-3794)
        │
        ├── SQLAllocEnv → ODBC_henv
        ├── SQLAllocConnect → DB.HDBC
        ├── SQLConnect (DSN, user, password)
        ├── MS-SQL: USE <dbname>, SET LOCK_TIMEOUT, SET DEADLOCK_PRIORITY
        ├── Informix: disable AUTOFREE
        └── AUTOCOMMIT off, dirty-read isolation

רכיב מקור:
        │
        ├── ExecSQL(op_id, &output_var, input_var)
        │       │
        │       ▼
        │   ODBC_Exec → SQL_GetMainOperationParameters
        │       │         └── #include "MacODBC_MyOperators.c"
        │       │                 → SQL text, column specs, flags
        │       │
        │       ├── ParseColumnList → ODBC_ColumnParams[]
        │       ├── SQL_CustomizePerDB → token rewriting
        │       ├── SQLPrepare → SQLBindCol/SQLBindParameter
        │       ├── SQLExecute (mirror first, then primary)
        │       └── SQLFetchScroll → output to caller
        │
        └── CommitWork / RollbackWork
                │
                ▼
            SQLEndTran (ENV level or DBC level)
```

---

## הערות אבטחה

על פי `RESEARCH/MacODBC_deepdive.md`:
- אישורי DB מועברים ל-`ODBC_CONNECT` כטקסט רגיל (DSN, username, password) ב-`MacODBC.h:3444-3448`
- `GenSql.c` מקבל ערכים אלה ממשתני סביבה (`MS_SQL_ODBC_USER`, `MS_SQL_ODBC_PASS`) ב-`GenSql.c:1088-1091`
- `setlocale(LC_ALL, "he_IL.UTF-8")` נקרא בזמן חיבור ב-`MacODBC.h:3488` — תלות ב-locale עברי
- מיקומים אלה מתועדים אך ערכים אינם מועתקים לתיעוד

---

*נוצר על ידי סוכן המתעד של CIDRA — DOC-MACODBC-001*
