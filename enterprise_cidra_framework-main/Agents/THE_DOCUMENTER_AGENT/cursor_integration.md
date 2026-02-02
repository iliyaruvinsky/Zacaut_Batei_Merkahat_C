# Cursor/VS Code Integration for The Documenter Agent

**Version**: 1.0.0
**For**: Cursor AI Editor and VS Code with AI assistants
**Purpose**: Automatic rule enforcement when documenting code

---

## Quick Start for Cursor Users

### Option 1: Add to Chat Context (Easiest)

When you want to document code in Cursor, start your chat with:

```
@THE_DOCUMENTER_AGENT/cursor_integration.md
@THE_DOCUMENTER_AGENT/sap_plugin.yaml

Document screen V_MY_SCREEN following the agent methodology above.
```

### Option 2: Add All Agent Files to Context

For full agent capability:

```
@THE_DOCUMENTER_AGENT/agent_specification.md
@THE_DOCUMENTER_AGENT/sap_plugin.yaml
@THE_DOCUMENTER_AGENT/validation_framework.md

Document screen V_MY_SCREEN with MACCABI configuration (100/100 quality).
Source: WD/
```

### Option 3: Use Code Snippets

Type trigger prefixes in markdown files:
- `doc-sap-spec` → SAP screen specification header
- `doc-node-shared` → Shared context node template
- `doc-node-unique` → Unique context node template
- `doc-methods` → Methods section
- `doc-limitations` → Limitations section (required)
- `doc-validation` → Validation report header
- `prompt-doc` → AI prompt for documenting

---

## Documenter Agent Rules for AI

When documenting code, **ALWAYS** follow these rules:

### Core Mission
Document with **100% accuracy** and **zero hallucinations**.
Everything must be traceable to actual source code.

### Mandatory Rules

#### 1. VERIFY BEFORE CLAIMING
- Read files after editing to confirm changes
- Never report success without verification
- Admit failures immediately and fix

#### 2. EXACT COUNTING ONLY
```powershell
# ALWAYS use exact commands:
(Get-Content FILE).Count  # PowerShell
wc -l FILE                # Bash

# Count field-by-field, not structure-by-structure
# NEVER: "~6 nodes", "approximately 12 methods"
# ALWAYS: "6 nodes (counted exactly)", "12 methods"
```

#### 3. CAREFUL LANGUAGE ALWAYS
```markdown
✅ CORRECT:
"According to the code, the screen appears to display..."
"Based on the C-spec (line 234), the program seems to..."

❌ WRONG:
"The screen displays..."
"The program processes..."
```

#### 4. FORBIDDEN WORDS
Never use:
- English: `advanced`, `smart`, `intelligent`, `sophisticated`
- Hebrew: `מתקדם`, `חכם`, `אינטליגנטי`, `מתוחכם`
- Estimates: `approximately`, `around`, `~`, `several`, `many`

#### 5. CROSS-REFERENCE CHECK (MANDATORY)

**For SAP WebDynpro ABAP:**
```bash
# Check Component Controller (file 0008):
grep "[NODE_NAME]" WD/0008_*.txt

# If found → "### NodeName (shared from Component Controller!)"
# If not → "### NodeName (unique to this screen)"
```

**For AS/400:**
```bash
# Check copy members:
grep '/COPY\|/INCLUDE' [MAIN_PROGRAM]

# For each data structure, verify in copy member
# If found → "### DS_NAME (shared from COPYMEMBER!)"
# If not → "### DS_NAME (unique to this program)"
```

#### 6. LIMITATIONS SECTION (REQUIRED)

Every specification MUST include:

```markdown
## מגבלות תיעוד / Documentation Limitations

### מה שלא ניתן לדעת / What CANNOT be determined from code:
- Database table contents
- External system behavior
- Business rule meanings
- [List actual unknowns]

### מה שכן ידוע / What IS known from code:
- Code structure (exact counts)
- Program flow
- Data definitions
- [List actual knowns]
```

#### 7. LINE NUMBER REFERENCES

Always include:
```markdown
✅ "The CALC_BALANCE procedure (P-spec line 234) appears to..."
❌ "The CALC_BALANCE procedure appears to..."
```

### SAP WebDynpro ABAP Workflow

```
Phase 1: Analyze (5 min)
├─ Scan all 133 files: ls WD/*.txt | wc -l
├─ Find relevant files: grep -l "V_SCREEN_NAME" WD/*.txt
├─ Identify Interface: *_S_*.txt
├─ Identify Implementation: *_B_*.txt
└─ Check Component Controller: WD/0008_*.txt

Phase 2: Count (5 min)
├─ Line counts: (Get-Content FILE).Count
├─ Context Nodes: grep 'begin of Element_' INTERFACE | wc -l
├─ Methods: grep 'METHODS' INTERFACE | wc -l
└─ Actions: Check case Handler_Name in Implementation

Phase 3: Cross-Reference (5 min)
├─ For each Context Node:
│   ├─ grep "NODE_NAME" WD/0008_*.txt
│   └─ Mark as shared or unique
└─ Check Event Handlers: grep "ON_" IMPLEMENTATION

Phase 4: Document (15 min)
├─ Create 7 files (see templates)
├─ Use careful language throughout
├─ Include exact counts
├─ Mark shared/unique elements
├─ Add limitations section
└─ Reference line numbers

Phase 5: Validate (5 min)
├─ File count = 7 ✓
├─ Forbidden words = 0 ✓
├─ Careful language ≥ 5 ✓
├─ Estimate language = 0 ✓
├─ Limitations section present ✓
├─ Cross-references marked ✓
└─ Calculate score (target: 100/100)
```

### AS/400 Workflow

```
Phase 1: Analyze (5 min)
├─ Scan all source members
├─ Find relevant files
├─ Identify copy members: grep '/COPY\|/INCLUDE'
└─ Identify DDS files (if applicable)

Phase 2: Count (5 min)
├─ Total lines: wc -l FILE
├─ Spec counts: grep -c '^[[:space:]]*[Hh][[:space:]]'
├─ Data structures: grep -c 'DS[[:space:]]'
├─ Procedures: grep -c 'dcl-proc' or P-specs
└─ Count fields manually in each DS

Phase 3: Cross-Reference (5 min)
├─ Read copy members
├─ For each data structure:
│   ├─ Check if defined in copy member
│   └─ Mark as shared or unique
└─ Check procedure prototypes

Phase 4-5: Same as SAP
```

### Required File Structure

Create exactly 7 files:

1. **01_[TYPE]_SPECIFICATION.md**
   - Component details (exact line counts)
   - Data structures / Context Nodes (with shared/unique marking)
   - Procedures / Methods
   - Limitations section (REQUIRED)

2. **02_UI_[INTERFACE].md**
   - UI elements (SAP) or Display file (AS/400)

3. **03_TECHNICAL_ANALYSIS.md**
   - Detailed breakdown

4. **04_BUSINESS_LOGIC.md**
   - Program flow (with careful language)

5. **05_CODE_ARTIFACTS.md**
   - Key code snippets

6. **README.md**
   - Quick overview
   - Key characteristics

7. **VALIDATION_REPORT_[NAME].md**
   - All validation checks
   - Quality score (100/100)

### Quality Validation

Target: **100/100** (only perfect scores acceptable)

Check list:
- [ ] File count = 7
- [ ] Forbidden words = 0
- [ ] Careful language present (≥5 instances)
- [ ] Estimate language = 0
- [ ] Limitations section present
- [ ] Cross-references marked (if applicable)
- [ ] Line counts accurate
- [ ] Validation report present

Scoring deductions:
```
- Forbidden word: -5 each
- Missing careful language: -10
- Estimate language: -15 each
- Missing limitations: -20
- Missing cross-reference: -25
- Wrong file count: -30
- Inaccurate line count: -40
```

---

## VS Code Tasks Available

Press `Ctrl+Shift+P` → "Tasks: Run Task" → Select:

### Documenter Agent Tasks:
- **🤖 Documenter: Analyze Screen (Phase 1)** - Scan files, identify relevant, count lines
- **🤖 Documenter: Count Elements (Phase 2)** - Count nodes, methods, actions
- **🤖 Documenter: Check Cross-References (Phase 3)** - Verify shared elements
- **🤖 Documenter: Validate Quality (100/100)** - Run all quality checks
- **🤖 Documenter: AI Prompt Generator** - Generate ready-to-use AI prompt

### Existing MACCABI Tasks:
- **📊 Scan All Code Files** - Count total source files
- **🔍 Find Screen Files** - Search for screen references
- **📏 Count File Lines Exactly** - Exact line count for file
- **🔗 Check Component Controller** - Check shared nodes
- **✅ Validate Screen Documentation** - Quick validation
- **📈 Project Progress Report** - Overall progress
- **🎯 Quick Quality Check** - Check for hallucinations

---

## Code Snippets Available

Type these prefixes in markdown files, then press `Tab`:

### SAP WebDynpro ABAP:
- `doc-sap-spec` - Complete specification header
- `doc-node-shared` - Shared context node template
- `doc-node-unique` - Unique context node template
- `doc-methods` - Methods section with structure
- `doc-actions-empty` - Empty actions section
- `doc-limitations` - Limitations section
- `doc-validation` - Validation report header
- `doc-check` - Individual check item
- `doc-readme` - Complete README template

### AS/400:
- `doc-as400-spec` - AS/400 program specification header
- `doc-as400-ds-shared` - Shared data structure from copy member

### General:
- `doc-careful` - Careful language examples
- `prompt-doc` - AI prompt for documenting
- `prompt-validate` - AI prompt for validation

---

## Example Usage in Cursor

### Example 1: Document SAP Screen

**In Cursor chat:**
```
@THE_DOCUMENTER_AGENT/cursor_integration.md
@THE_DOCUMENTER_AGENT/sap_plugin.yaml

Document screen V_DETAIL for MACCABI ICM project.

Configuration: maccabi_icm (100/100 quality, 7 files)
Source: WD/

Follow 5-phase workflow as described above.
```

**Cursor will:**
1. Scan all 133 files
2. Find relevant files for V_DETAIL
3. Count all elements exactly
4. Check Component Controller
5. Create 7 documentation files
6. Validate quality (100/100)

### Example 2: Validate Existing Documentation

**In Cursor chat:**
```
@THE_DOCUMENTER_AGENT/validation_framework.md

Validate documentation at:
WD/SCREENS/11_V_TREATMENT_HISTORY_SCREEN/

Run all checks and report score.
```

**Cursor will:**
1. Check file count
2. Scan for forbidden words
3. Check careful language
4. Check estimate language
5. Verify limitations section
6. Check cross-references
7. Report score (target: 100/100)

### Example 3: Use Snippet Then Ask AI to Complete

**In markdown file:**
1. Type `doc-sap-spec` and press `Tab`
2. Template appears with placeholders
3. Fill in what you know (screen name, line counts)
4. Select template and ask Cursor: "Complete this specification using exact counts from WD/ files"

---

## Integration with Existing MACCABI Workflow

This Documenter Agent integrates seamlessly with your existing MACCABI workflow:

### Before (Manual Documentation):
1. Read 133 files manually
2. Count elements manually
3. Write documentation manually
4. Hope for accuracy

**Time**: 18 hours per screen
**Accuracy**: Varies

### After (With Documenter Agent):
1. Run "Analyze Screen" task (5 min)
2. Run "Count Elements" task (5 min)
3. Ask AI to document with agent rules (15 min)
4. Run "Validate Quality" task (5 min)

**Time**: ~30 minutes per screen
**Accuracy**: 100/100 proven (9 screens completed)

---

## Reference Examples

**Gold Standard**: `WD/SCREENS/07_V_APPROVE_SCREEN/`
- 100/100 on first attempt
- 10 files (7 docs + 3 validation reports)
- Reference this for structure

**Simple Example**: `WD/SCREENS/17_V_DIAGNOSIS_SCREEN/`
- 6 Context Nodes (all shared)
- 0 Actions
- Simple structure pattern

**With Fixes Documented**: `WD/SCREENS/11_V_TREATMENT_HISTORY_SCREEN/`
- Shows validation report with fixes
- Initial 70/100 → Final 100/100
- Learn from mistakes documented

---

## Success Metrics (Proven in MACCABI)

✅ **9 screens** documented at 100/100 accuracy
✅ **Zero hallucinations** across all documentation
✅ **18x productivity** (18 hours → 1 hour per screen)
✅ **Client feedback**: "Built very well"
✅ **Consistency**: Same quality across all screens

---

## Quick Reference Card

**When Cursor asks about documenting:**

1. ✅ Add `@THE_DOCUMENTER_AGENT/cursor_integration.md` to context
2. ✅ Run Phase 1 task (Analyze) for exact counts
3. ✅ Ask Cursor to follow 5-phase workflow
4. ✅ Run validation task to verify 100/100
5. ✅ Fix any issues until perfect score

**Remember:**
- Scan all files first
- Count exactly (no estimates)
- Check cross-references
- Use careful language
- Include limitations
- Only 100/100 acceptable

---

*Save this file and reference it with @ in Cursor for automatic rule enforcement*
*Or add it to your Cursor composer context for multi-file documentation tasks*
