# CIDRA Framework - Agent Workflow

**Version**: 1.0.0  
**Purpose**: Complete workflow for documenting legacy systems

---

## 🎯 Overview

CIDRA uses **3 AI Agents** working in sequence:

```
Source Code → the_chunker → the_documenter → the_recommender → Documentation + Roadmap
```

Each agent has a specific role and passes data to the next.

---

## 🔄 Complete Workflow

### **Stage 0: the_chunker** (Optional but Recommended)

**Purpose**: Organize code into LLM-optimized chunks

**Input**: 
- Raw source code files in `Source Code/` directory
- `.cidra-config.json` with chunking strategy

**Process**:
1. Scans all files in `Source Code/`
2. Identifies relationships and dependencies
3. Groups related code together
4. Creates metadata about structure

**Output**:
- `/chunks/` directory with organized code
- `/chunks/metadata.json` with relationships
- `/chunks/summary.md` with overview

**Invocation**:
```
@THE_CHUNKER_AGENT analyze [SYSTEM_NAME] using adaptive strategy
```

**Time**: 10-15 minutes for medium system

**Why use it**:
- ✅ Better context for documenter
- ✅ Faster documentation generation
- ✅ Identifies shared components automatically

**Can skip if**:
- Code is already well-organized
- Small system (< 10 files)
- Want to document single component only

---

### **Stage 1: the_documenter** (Core - Required)

**Purpose**: Generate accurate documentation with zero hallucinations

**Input**:
- Source code from `Source Code/` directory (or `/chunks/` if chunker was used)
- `.cidra-config.json` with documentation rules
- Component/screen/class name to document

**Process**:
1. **Scan**: All files in Source Code/ for component references
2. **Count**: Exact line counts, methods, functions, structures
3. **Identify**: Shared vs unique components
4. **Extract**: Business logic, UI elements, data flow
5. **Cite**: Every code snippet with exact line numbers
6. **Validate**: Self-check against anti-hallucination rules

**Output**: **7 standard documentation files**
1. `01_SPECIFICATION.md` - Technical overview
2. `02_UI_MOCKUP.md` - Interface description
3. `03_TECHNICAL_ANALYSIS.md` - Detailed analysis
4. `04_BUSINESS_LOGIC.md` - Business rules
5. `05_CODE_ARTIFACTS.md` - Code snippets with citations
6. `README.md` - Quick reference
7. `VALIDATION_REPORT.md` - 100/100 accuracy verification

**Invocation**:
```
@THE_DOCUMENTER_AGENT document [TYPE] [COMPONENT_NAME]

Types: screen | class | component | module | program
```

**Time**: 20-30 minutes per component

**Quality guarantee**:
- ✅ 100% code-based (zero hallucinations)
- ✅ All code cited with line numbers
- ✅ Exact counts (no estimates)
- ✅ Careful language ("appears", "according to code")
- ✅ Limitations acknowledged

---

### **Stage 2: the_recommender** (Modernization - Optional)

**Purpose**: Provide modernization recommendations with ROI analysis

**Input**:
- Completed documentation from Stage 1
- `.cidra-config.json` with technology and business context
- System name and current technology

**Process**:
1. **Analyze**: Current technology stack and architecture
2. **Evaluate**: Modernization options (rewrite/refactor/replatform)
3. **Calculate**: ROI for each option
4. **Assess**: Technical and business risks
5. **Recommend**: Best path forward with priorities

**Output**:
- Technology comparison matrix
- Migration strategy document
- Risk assessment report
- ROI calculation
- Implementation roadmap with phases

**Invocation**:
```
@THE_RECOMMENDER_AGENT recommend modernization for [SYSTEM_NAME]
```

**Time**: 15-30 minutes per analysis

**Deliverables**:
- ✅ Multiple technology options
- ✅ ROI for each option
- ✅ Risk levels (Low/Medium/High)
- ✅ Recommended approach
- ✅ Phased implementation plan

---

## 📊 Data Flow Between Agents

### **Chunker → Documenter:**

```
the_chunker outputs:
├── /chunks/component_1.md
├── /chunks/component_2.md
├── /chunks/metadata.json → shared components identified
└── /chunks/summary.md → system overview

the_documenter uses:
├── metadata.json → knows which components are shared
├── component_X.md → focused context for documentation
└── summary.md → understanding of overall system
```

### **Documenter → Recommender:**

```
the_documenter outputs:
├── [COMPONENT_1]/
│   ├── 01_SPECIFICATION.md → architecture details
│   ├── 03_TECHNICAL_ANALYSIS.md → technical debt identified
│   ├── 04_BUSINESS_LOGIC.md → business rules extracted
│   └── VALIDATION_REPORT.md → quality metrics

the_recommender uses:
├── All SPECIFICATION.md → understand current state
├── All TECHNICAL_ANALYSIS.md → identify modernization needs
├── All BUSINESS_LOGIC.md → preserve business rules in migration
└── Quality metrics → calculate effort and ROI
```

---

## 🎯 Complete Example: MACCABI ICM Project

### **Real-world workflow:**

#### **Step 0: Setup** (5 minutes)
```bash
git clone https://github.com/iliyaruvinsky/enterprise_cidra_framework.git
cd MY_PROJECT
./setup.sh  # From maccabi_cidra example
```

#### **Step 1: Chunker** (10 minutes) - Optional
```
@THE_CHUNKER_AGENT analyze MACCABI_ICM using adaptive

Result: 133 files organized into logical chunks
```

#### **Step 2: Documenter** (25 min × 33 screens = 13.75 hours)
```
@THE_DOCUMENTER_AGENT document screen V_SELECT
@THE_DOCUMENTER_AGENT document screen V_MAIN
@THE_DOCUMENTER_AGENT document screen V_DETAIL
... repeat for all 33 screens

Result: 33 screens × 7 files = 231 documentation files
Quality: 100% accuracy (verified by client)
```

#### **Step 3: Recommender** (30 minutes)
```
@THE_RECOMMENDER_AGENT recommend modernization for MACCABI_ICM

Result: 
- Technology options: SAP Fiori, React, Angular
- Recommended: React with SAP integration
- ROI: 3-year payback
- Risk: Medium
- Strategy: Phased migration (6 screens → 33 screens)
```

---

## 📋 Workflow Patterns

### **Pattern 1: Single Component**

```
1. @THE_DOCUMENTER_AGENT document component LOGIN_SCREEN
2. Review 7 generated files
3. Check VALIDATION_REPORT.md (must be 100/100)
4. Done!
```

**Time**: ~30 minutes  
**Use when**: Learning CIDRA, small change, single component

---

### **Pattern 2: Full System Documentation**

```
1. @THE_CHUNKER_AGENT analyze SYSTEM_NAME using adaptive
2. For each component in chunks/summary.md:
   @THE_DOCUMENTER_AGENT document component [NAME]
3. Review all validation reports
4. @THE_RECOMMENDER_AGENT recommend modernization for SYSTEM_NAME
```

**Time**: 1-3 days depending on system size  
**Use when**: Complete system documentation needed

---

### **Pattern 3: Modernization Planning**

```
1. Ensure documentation complete (Stage 1)
2. @THE_RECOMMENDER_AGENT recommend modernization for SYSTEM
3. @THE_RECOMMENDER_AGENT compare technologies [TECH1] vs [TECH2]
4. @THE_RECOMMENDER_AGENT assess risk for migration to [TARGET]
5. Review recommendations and create roadmap
```

**Time**: 1-2 hours  
**Use when**: Planning technology migration

---

## 🔄 Iteration and Updates

### **When code changes:**

```
1. Update source files in Source Code/
2. Re-run documenter for changed components:
   @THE_DOCUMENTER_AGENT document component [CHANGED_COMPONENT]
3. Verify VALIDATION_REPORT.md still shows 100/100
```

### **When adding new components:**

```
1. Add code to Source Code/
2. Optionally re-run chunker (if structure changed significantly)
3. Document new component:
   @THE_DOCUMENTER_AGENT document component [NEW_COMPONENT]
```

---

## ✅ Quality Gates

### **After Chunker:**
- [ ] `/chunks/` directory exists
- [ ] `metadata.json` present
- [ ] All source files accounted for

### **After Documenter:**
- [ ] Exactly 7 files per component
- [ ] All code cited with line numbers
- [ ] VALIDATION_REPORT.md shows 100/100
- [ ] No invented content

### **After Recommender:**
- [ ] Multiple options presented
- [ ] ROI calculated
- [ ] Risks identified
- [ ] Clear recommendation provided

---

## 🎓 Best Practices

### **1. Always Start with Chunker for Large Systems**
If you have 50+ files, chunker saves hours of documentation time.

### **2. Document in Batches**
Document 5-10 components at a time, then review all validation reports.

### **3. Use Validation Reports as Quality Proof**
Share with stakeholders - 100/100 scores build confidence.

### **4. Run Recommender After Full Documentation**
Better recommendations when entire system is documented.

### **5. Keep Source Code/ Updated**
Always work with latest code for accurate documentation.

---

## 🚀 Real Results

### **MACCABI ICM Project:**
- **Input**: 33 SAP WebDynpro screens, 133 code files
- **Chunker**: Organized into logical groups
- **Documenter**: 33 screens × 7 files = 231 docs (13.75 hours)
- **Result**: 100% accuracy, 18x faster than manual
- **Client feedback**: "Built very well"

See: https://github.com/iliyaruvinsky/maccabi_cidra

---

**Follow this workflow for consistent, accurate documentation with CIDRA!**

