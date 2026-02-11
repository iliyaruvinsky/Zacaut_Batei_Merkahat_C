		// Variables to set:
		//
		// SQL_CommandText		=	the SQL command, stripped of any "INTO" clause and without a final semicolon.
		//
		// NumOutputColumns		=	the number of columns SELECTed - if there are none, just leave it out
		//							and it defaults to zero.
		//
		// NumInputColumns		=	the number of columns passed to SQL as parameters, either for the WHERE
		//							clause or as values for INSERT or UPDATE. If there are none, just leave
		//							the variable out and it defaults to zero.
		//
		// StatementIsSticky	=	1 if the statement should be left PREPAREd for repeated use. Since the ODBC
		//							driver has a finite capacity for open statements, this parameter should
		//							be used sparingly; it's best used for statements that are (A) complex and
		//							(B) called very frequently in transactions that require high performance.
		//							If a statement will be used many times by a particular operation and then
		//							not used for a while, set it "sticky" and then execute FreeStatement() on
		//							it when you're done with it. This variable defaults to zero, so it needs
		//							to be set only when it's TRUE.
		//
		// CursorName			=	the name of the cursor. This is required ONLY for update cursors, so that
		//							"WHERE CURRENT OF" clauses will work. Otherwise leave it out, and it will
		//							default to NULL (= NOT an updatable cursor). Note that the ODBC interface
		//							routines will automatically add the "FOR UPDATE" clause if it isn't already
		//							present. IMPORTANT: Cursor names MUST start with a letter, and should be
		//							18 characters or less in length.
		//
		// OutputColumnSpec		=	a string with all the required type/length specifications for output columns.
		//							This can be in the form of C declaration types ("int,char[30]", etc.) or
		//							in the form of SQL row types ("smallint,varchar(30),integer", etc.) - the
		//							parsing routine is smart enough to interpret either way, or any mix.
		//							Delimiters can be spaces, commas, parentheses, brackets, or tabs, and variable
		//							type matching is case-insensitive. If there are no output columns, just leave
		//							this out and it defaults to NULL.
		//
		//							IMPORTANT: Because most of our decimal numbers are C type "double" stored
		//							in SQL "float" columns, "float" is translated to "double". If the C variable
		//							type is float, USE "REAL" instead of "float" or "double".
		//
		// InputColumnSpec		=	a string with all the required type/length specifications for output columns.
		//							This works exactly the same as OutputColumnSpec, described above.
		//
		// GenerateVALUES		=	if non-zero, the VALUES clause of an INSERT statment will be generated
		//							automatically. Note that this will work properly *only* if all INSERTed values
		//							come from variables - if some fields get constant values, you need to build
		//							your own custom VALUES clause.
		//
		// NeedsWhereClauseID	=	if non-zero, the argument after the SQL Operator ID specifies a "custom"
		//							WHERE clause, with its own logic and input variables (if any). This can
		//							be used any time you want to have variable methods of SELECTing the same
		//							information, so you don't have to create duplicate SQL. The main SQL command
		//							text MUST include a %s where the WHERE clause is meant to go! The actual
		//							word "WHERE" can be in the main SQL text or the "custom" text - but I think
		//							it will be more human-friendly if it's in the main text - i.e. WHERE %s.
		//
		// Convert_NF_to_zero	=	if non-zero, automatically convert a not-found error (SQLCODE 100) to a zero
		//							stored in the (integer) output column. This should be used ONLY for SQL
		//							operations where the output consists entirely of a COUNT function. Really,
		//							this shouldn't be necessary at all, since the database is supposed to return
		//							zero in COUNT if nothing was found - but at least in Informix, we're getting
		//							SQLCODE = 100 back from some operations, and it's better not to have to add
		//							new error-handling code all over the place. (DonR 21Jun2020)
		//
		// SuppressDiagnostics	=	If non-zero, suppress some diagnostic error messages. This is intended to be
		//							used for things like DROP TABLE commands where we don't really expect the
		//							table to be present, so we don't want to log errors when the SQLPrepare
		//							command fails.
		//
		// CommandIsMirrored	=	If non-zero, enable mirroring for this command. Mirroring applies to
		//							statements that are *not* SELECTs, and is globally enabled based on the
		//							environment variable ODBC_MIRRORING_ENABLED as well as having two different
		//							databases connected. If all these things are true, mirrored commands will
		//							be carried out in both databases, to keep specific tables synchronized.
		//							Typically this would involve normal INSERT/UPDATE/DELETE operations. For
		//							now at least, we are assuming that all mirrored commands involve simple
		//							SQL that does not need "translation" - so no "{TOP} N" or other stuff that
		//							involves different phrasing for Informix versus MS-SQL. Also, mirrored
		//							commands won't support stuff like WHERE CURRENT OF, since the cursor will
		//							exist only for the primary database.
		//
		//
		// Note that this code is inserted inside a switch statement - all you need to do is supply the cases.
		// The operator ID's for your program are defined in MacODBC_MyOperatorIDs.h, which - like this file -
		// should be in your specific program directory, NOT in the INCLUDE directory.


		// Include operations used by GenSql.ec
		#include "GenSql_ODBC_Operators.c"

//		case bypass_basket_cur:
//					SQL_CommandText		=	"	SELECT	illness_bit_number,		override_basket,	is_actual_disease,		use_diagnoses	"
//											"	FROM	illness_codes	"
//											"	WHERE	illness_bit_number	> 0	";
//					NumOutputColumns	=	4;
//					OutputColumnSpec	=	"	short,short,short,short	";
//					break;


		case AS400UDS_T3008_DEL_hospital_member:
					SQL_CommandText		=	"	DELETE FROM	hospital_member				"
											"	WHERE		member_id			= ?		"
											"	  AND		mem_id_extension	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


//		case AS400UDS_T3008_INS_hospital_member:
//					SQL_CommandText		=	"	INSERT INTO	hospital_member									"
//											"	(															"
//											"		member_id,			mem_id_extension,	entry_date,		"
//											"		discharge_date,		in_hospital,		hospital_num,	"
//											"		hospital_name,		department_name,	update_date,	"
//											"		update_time,		insertdate							"
//											"	)															";
//					NumInputColumns		=	11;
//					InputColumnSpec		=	"	int, short, int, int, short, int, char(25), char(30), int, int, int	";
//					GenerateVALUES		=	1;
//					break;


		case AS400UDS_T3008_INS_HospitalMsg:
					SQL_CommandText		=	"	INSERT INTO	HospitalMsg								"
											"	(													"
											"		contractor,		termid,			idnumber,		"
											"		idcode,			hospdate,		nursedate,		"
											"		contrname,		hospitalnumb,	typeresult,		"
											"		rescount,		resname1,		resultsize,		"
											"		result,			hospname,		departname,		"
											"		flg_system,		patalogyflg,	insertdate,		"
											"		inserttime										"
											"	)													";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	int, int, int, int, int, int, varchar(30), int, varchar(25), int, varchar(30),	"
											"	int, char(2048), varchar(25), varchar(30), int, int, int, int					";
					GenerateVALUES		=	1;
					break;


		case AS400UDS_T3008_READ_systexts:
					SQL_CommandText		=	"	SELECT	hosptyperesd,	hospresnamew,	hospresnamed,	"
											"			hosprelnamew,	hosprelnamed ,	termdescwin		"
											"	FROM	systexts										"
											"	WHERE	keytext = ?										";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	char(25), char(30), char(30), char(30), char(30), char(4)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


//		case AS400UDS_T3008_UPD_members:
//					SQL_CommandText		=	"	UPDATE	members						"
//
//											"	SET		in_hospital			= ?		"
//
//											"	WHERE	member_id			= ?		"
//											"	  AND	mem_id_extension	= ?		";
//					NumInputColumns		=	3;
//					InputColumnSpec		=	"	short, int, short	";
//					break;


		case DbInsertLabResultOtherRecord_get_labresother_count:
					SQL_CommandText		=	"	SELECT	COUNT(*)					"

											"	FROM	labresother					"

											"	WHERE	idnumber		=	?		"
											"	  AND	idcode			=	?		"
											"	  AND	reqcode			=	?		"
											"	  AND	labid			=	?		"
											"	  AND	route			=	?		"
											"	  AND	result			=	?		"
											"	  AND	lowerlimit		=	?		"
											"	  AND	upperlimit		=	?		"
											"	  AND	reqdate			=	?		"
											"	  AND	pregnantwoman	=	?		"
											"	  AND	completed		=	?		"
											"	  AND	reqid			=	?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	12;
					InputColumnSpec		=	"	int, int, int, int, int, int, int, int, int, int, int, int	";
					break;


		case DbInsertLabResultOtherRecord_INS_labresother:
					// DonR 05Apr2020: IdCode, LabId, PregnantWoman, and Completed are all populated from
					// short integers. The MS-SQL table should be set up accordingly!
					SQL_CommandText		=	"	INSERT INTO	LabResOther								"
											"	(													"
											"		Contractor,		IdCode,			IdNumber,		"
											"		LabId,			ReqId,			ReqCode,		"
											"		Route,			Result,			LowerLimit,		"

											"		UpperLimit,		Units,			ApprovalDate,	"
											"		ForwardDate,	ReqDate,		PregnantWoman,	"
											"		Completed,		TermId,			ReqId_Main,		"
											"		{Group},		insertdate,		inserttime		"
											"	)													";
					NumInputColumns		=	21;
					InputColumnSpec		=	"	int, short, int, short, int, int, int, int, int,					"
											"	int, char(15), int, int, int, short, short, int, int, int, int, int	";
					GenerateVALUES		=	1;
					break;
