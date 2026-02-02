# The Recommender Agent - Cursor Integration Guide

**Version**: 1.0.0
**Date**: November 3, 2025
**Purpose**: Activate and use The Recommender in Cursor IDE

---

## 🚀 Quick Start (30 seconds)

**In Cursor chat, type:**

```
@THE_RECOMMENDER_AGENT/agent_specification.md
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml
@WD/SCREENS/03_V_DETAIL_SCREEN/README.md

Analyze V_DETAIL screen and recommend modernization approach.

Context:
- Team: 4 developers (know ABAP, learning React/Spring Boot)
- Timeline: 12 months
- Budget: $300K
- Requirements: 99.9% uptime, HIPAA compliance

Produce complete recommendation report with:
- Technology alternatives
- Architecture pattern
- Migration strategy
- Risk assessment
- Cost-benefit analysis
- Implementation roadmap
```

**That's it!** The agent will produce a comprehensive recommendation in 5-10 minutes.

---

## 📋 What The Recommender Does

### Input
**Consumes documentation from the_documenter**:
- 01_SCREEN_SPECIFICATION.md - Technical specs
- 02_UI_MOCKUP.md - UI structure
- 03_TECHNICAL_ANALYSIS.md - Code analysis
- 04_BUSINESS_LOGIC.md - Business logic
- 05_CODE_ARTIFACTS.md - Code snippets
- README.md - Overview
- דוח_אימות_*.md - Validation report

**Plus context**:
- Team profile (skills, size, experience)
- Business constraints (budget, timeline, compliance)
- Strategic goals (cloud migration, cost reduction)

### Process
**5-Phase Analysis**:
1. **Intake & Discovery** (10%) - Parse documentation, gather context
2. **Analysis & Scoring** (25%) - Assess complexity, risk, readiness
3. **Option Generation** (30%) - Map to modern technologies, patterns
4. **Evaluation & Ranking** (20%) - Score and rank alternatives
5. **Recommendation & Roadmap** (15%) - Package final recommendation

### Output
**Comprehensive Recommendation**:
- Technology alternatives (backend, frontend, database, infra)
- Architecture pattern (microservices, modular monolith, etc.)
- Migration strategy (rewrite, refactor, replatform, retire)
- Risk assessment (technical, business, team, testing, timeline)
- Cost-benefit analysis (investment, ROI, payback period)
- Implementation roadmap (phases, goals, deliverables)

---

## 🎯 Three Ways to Use

### Method 1: **Full Analysis** (Recommended for Complex Components)

**For**: Complete modernization recommendation with all details

**Command**:
```
@THE_RECOMMENDER_AGENT/agent_specification.md
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml
@WD/SCREENS/03_V_DETAIL_SCREEN/README.md
@WD/SCREENS/03_V_DETAIL_SCREEN/03_TECHNICAL_ANALYSIS.md

Analyze V_DETAIL screen and recommend full modernization approach.

Context:
- Current: ABAP + WebDynpro, 52 context nodes, 140+ event handlers, 30K LOC
- Team: 4 developers (2 backend ABAP, 2 frontend WebDynpro), no React experience
- Timeline: 12 months for full migration
- Budget: $300K total
- Requirements:
  - 99.9% uptime (currently 98%)
  - < 2 second page load (currently 8 seconds)
  - HIPAA compliance (healthcare data)
  - Must support 500+ concurrent users

Strategic goals:
- Cloud migration (AWS preferred)
- Reduce licensing costs (currently $50K/year SAP)
- Modern UX (mobile-friendly)

Produce full recommendation with:
1. Technology stack recommendation (backend + frontend + database + infra)
2. Architecture pattern (show current vs target diagrams)
3. Migration strategy (phases with timeline and costs)
4. Risk assessment (score all 5 factors, identify top 5 risks)
5. Cost-benefit analysis (3-year projection with ROI)
6. Implementation roadmap (detailed phases with deliverables)
7. Success criteria (technical, business, team)

Output formats:
- RECOMMENDATION_REPORT.md (markdown)
- recommendation_structured.json (for development_agent)
```

**Expected time**: 10-15 minutes
**Output**: 35-page comprehensive report + JSON

---

### Method 2: **Technology Comparison Only**

**For**: Quick answer to "Should we use X or Y?"

**Command**:
```
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml

Compare React vs Angular for replacing WebDynpro UI.

Context:
- Current: WebDynpro (component-based, data binding, event handlers)
- Team: 4 developers, no modern frontend experience
- Timeline: 6 months
- Budget: $200K

Criteria to compare:
- Learning curve for ABAP developers
- Component model match to WebDynpro
- Ecosystem maturity
- Enterprise adoption
- Tooling quality
- Long-term viability

Rank both options and recommend one with justification.
```

**Expected time**: 2-3 minutes
**Output**: TECHNOLOGY_COMPARISON.md (5-7 pages)

---

### Method 3: **Risk Assessment Only**

**For**: Quick risk check before committing to migration

**Command**:
```
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml
@WD/SCREENS/11_V_TREATMENT_HISTORY_SCREEN/README.md

Assess migration risk for V_TREATMENT_HISTORY screen.

Calculate scores for:
- Technical complexity (LOC, complexity, dependencies)
- Business criticality (users, revenue, compliance)
- Team readiness (skills, domain knowledge)
- Testing coverage (automation, test env)
- Timeline pressure (deadline)

Provide:
- Overall risk score (0-100)
- Risk level (LOW/MEDIUM/HIGH)
- Top 5 risks with mitigations
- Recommendation (safe to rewrite vs incremental refactor)
```

**Expected time**: 3-5 minutes
**Output**: RISK_ASSESSMENT.md (8-10 pages)

---

## 📊 Use Cases

### Use Case 1: **Single Screen Modernization**

**Scenario**: Modernize V_DIAGNOSIS (simple screen)

**Input Documentation**:
```
@WD/SCREENS/17_V_DIAGNOSIS_SCREEN/01_SCREEN_SPECIFICATION.md
@WD/SCREENS/17_V_DIAGNOSIS_SCREEN/04_BUSINESS_LOGIC.md
@WD/SCREENS/17_V_DIAGNOSIS_SCREEN/README.md
```

**Analysis**:
- Complexity: LOW (6 context nodes, 0 actions, ~350 LOC)
- Risk: LOW (24/100)
- Recommendation: **Rewrite** in React (2-week sprint)

**Recommendation Output**:
```markdown
## Recommendation: V_DIAGNOSIS Screen

### Strategy: Complete Rewrite
**Confidence**: 95%
**Risk Score**: 24/100 (LOW)

### Recommended Stack:
- Frontend: React + TypeScript
- Backend: Spring Boot (REST API)
- Database: PostgreSQL
- Infrastructure: AWS (ECS + RDS)

### Migration Approach:
**Phase 1**: Build React UI (1 week, $10K)
**Phase 2**: Migrate business logic to Spring Boot (1 week, $10K)
**Parallel Run**: 2 weeks validation
**Cutover**: After 95% user acceptance

### Total Cost: $25K (incl. testing & contingency)
### Timeline: 4 weeks (including parallel run)
### Expected Benefits:
- Page load: 8s → < 2s
- Development speed: 3x faster for future changes
- User satisfaction: 65% → 90%
```

---

### Use Case 2: **Full System Modernization**

**Scenario**: Modernize all 33 MACCABI ICM screens

**Command**:
```
@THE_RECOMMENDER_AGENT/agent_specification.md
@WD/SCREENS/*/README.md

Analyze all 33 screens and produce master modernization plan.

Context:
- 33 screens: 8 simple, 15 medium, 10 complex
- Team: 6 developers (4 backend, 2 frontend), growing to 10
- Timeline: 18-24 months
- Budget: $1.5M total
- Goals: Cloud migration, 50% cost reduction, modern UX

Produce:
1. Screen-by-screen analysis (complexity, risk, priority)
2. Recommended migration order (simple → complex)
3. Master timeline (18-24 months, phased rollout)
4. Total cost breakdown (by phase and category)
5. Expected ROI (3-year projection)
6. Resource plan (team composition over time)
```

**Output**: MASTER_MODERNIZATION_PLAN.md (80-100 pages)

**Contents**:
- Executive summary (2 pages)
- Screen analysis matrix (33 screens ranked by priority)
- Technology stack recommendation
- Architecture evolution (monolith → modular monolith → microservices)
- 6-phase roadmap (3-4 months each)
- Financial model (costs, benefits, ROI)
- Risk mitigation plan

---

### Use Case 3: **Architecture Pattern Selection**

**Scenario**: "Should we go microservices or modular monolith?"

**Command**:
```
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml
@PROJECT_OVERVIEW_MACCABI_ICM.md

Compare architecture patterns for MACCABI ICM modernization.

Current architecture:
- SAP NetWeaver monolith
- WebDynpro UI (33 screens)
- ABAP business logic (656 files)
- SAP BW database

Options to compare:
1. Microservices (full decomposition)
2. Modular Monolith (restructured monolith)
3. Modular Monolith → Microservices (gradual evolution)

Context:
- Team: 6 developers, growing to 10 (learning modern stack)
- Timeline: 18 months
- Risk tolerance: Medium (can't afford big bang failure)

Compare on:
- Deployment complexity
- Development speed
- Operational overhead
- Team learning curve
- Risk level
- Future flexibility

Recommend best approach with justification.
```

**Output**: ARCHITECTURE_COMPARISON.md (15-20 pages)

**Recommendation Preview**:
```markdown
## Recommended: Modular Monolith → Microservices (Gradual)

### Phase 1: Modular Monolith (Months 1-6)
- Restructure into modules (patient, treatment, payment)
- Single deployment (simple operations)
- Team learns modern stack (Spring Boot, React)
- Validates approach with low risk

### Phase 2: Extract Microservices (Months 7-18)
- Extract high-value services (treatment validation, payments)
- Independent scaling and deployment
- Team ready for distributed systems

### Justification:
- **Lower risk**: Team learns gradually, validates before committing
- **Faster initial delivery**: Modular monolith faster than microservices
- **Future flexibility**: Can become microservices later
- **Best fit**: Medium risk tolerance, learning team
```

---

## 🔍 Understanding the Output

### RECOMMENDATION_REPORT.md Structure

```markdown
# Modernization Recommendation: [COMPONENT_NAME]

## Executive Summary (2 pages)
- Current state summary
- Recommended approach
- Key benefits (top 3)
- Key risks (top 3)
- Investment required
- Expected ROI
- Timeline

## Detailed Analysis (10-15 pages)
### Technology Assessment
- Current stack analysis
- Recommended stack (backend, frontend, database, infra)
- Alternatives considered

### Architecture Pattern
- Current architecture (diagram)
- Target architecture (diagram)
- Migration path

### Migration Strategy
- Approach (rewrite/refactor/replatform)
- Phases with timeline
- Parallel run approach

## Risk Assessment (5-8 pages)
- Overall risk score
- Risk breakdown (5 factors)
- Top 5 risks with mitigations
- Risk matrix

## Cost-Benefit Analysis (3-5 pages)
- Total investment breakdown
- Annual benefits (operational, productivity, business value)
- 3-year projection
- ROI calculation
- Payback period

## Implementation Roadmap (5-10 pages)
- Phase-by-phase breakdown
- Goals and deliverables per phase
- Resource plan
- Success criteria

## Appendices (5-10 pages)
- Technology comparison matrices
- Alternative options
- Detailed calculations
- References
```

---

### recommendation_structured.json Format

**Purpose**: Machine-readable output for development_agent (Stage 3)

```json
{
  "metadata": {
    "component_name": "V_DETAIL_SCREEN",
    "analyzed_date": "2025-11-03",
    "analyzer": "the_recommender v1.0.0"
  },

  "recommendation": {
    "strategy": "incremental_refactor",
    "confidence": 0.92,
    "risk_score": 71,
    "risk_level": "HIGH"
  },

  "technology_stack": {
    "backend": {
      "primary": {"language": "Java", "framework": "Spring Boot", "confidence": 0.95}
    },
    "frontend": {
      "primary": {"framework": "React", "language": "TypeScript", "confidence": 0.95}
    }
  },

  "migration_phases": [
    {
      "phase": 1,
      "name": "Extract Business Logic",
      "duration_months": 2,
      "cost_usd": 80000,
      "goals": ["Create Spring Boot service", "..."],
      "deliverables": ["Spring Boot REST API", "..."]
    }
  ],

  "implementation_ready": {
    "for_stage_3_agent": true,
    "structured_tasks": [
      {
        "task": "Setup Spring Boot project",
        "template": "spring-boot-starter-web",
        "dependencies": ["spring-data-jpa", "postgresql"]
      }
    ]
  }
}
```

**Usage by Stage 3 (development_agent)**:
```
@recommendation_structured.json
@THE_DEVELOPMENT_AGENT/agent_specification.md

Implement Phase 1 of recommendation:
- Setup Spring Boot project per spec
- Implement all 23 methods from ABAP class
- Create REST API with OpenAPI spec
- Write unit tests (80% coverage target)
```

---

## 📈 Quality Validation

### Check Recommendation Quality

**After generating recommendation, validate:**

```
@THE_RECOMMENDER_AGENT/RECOMMENDATIONS/V_DETAIL_SCREEN/RECOMMENDATION_REPORT.md

Validate recommendation quality:
- Technology choices justified?
- All alternatives considered?
- Risk assessment comprehensive?
- Cost estimates realistic?
- Roadmap actionable?
- Success criteria measurable?

Report quality score (0-100) with issues found.
```

**Expected output**:
```markdown
## Recommendation Quality Report

**Overall Score**: 94/100

### Technology Choices (95/100)
✅ Primary choices well-justified
✅ Alternatives considered (React, Angular, Vue)
✅ Pros/cons documented
⚠️ Minor: Could expand on Go alternative

### Risk Assessment (92/100)
✅ All 5 factors scored
✅ Top 5 risks identified
✅ Mitigations provided
⚠️ Minor: Testing coverage assumptions not validated

### Cost-Benefit (96/100)
✅ Detailed cost breakdown
✅ 3-year projection
✅ ROI calculated
✅ Realistic assumptions

### Roadmap (93/100)
✅ Clear phases
✅ Deliverables defined
✅ Timeline reasonable
⚠️ Minor: Resource plan could be more detailed

### Issues Found: 3 (all minor)
1. Expand Go language alternative analysis
2. Validate testing coverage assumptions
3. Add more detail to resource plan

### Recommendation: APPROVED for stakeholder presentation
Minor improvements optional, not blocking.
```

---

## 🛠️ Troubleshooting

### Issue 1: Not Enough Context

**Problem**: Recommendation too generic, not specific to MACCABI

**Solution**:
```
Provide more context in the prompt:
- Team skills (specific: "4 ABAP developers, 0 React experience")
- Business constraints (specific: "HIPAA compliance required")
- Strategic goals (specific: "Migrate to AWS by Q4 2026")

Also @ mention:
- @PROJECT_OVERVIEW_MACCABI_ICM.md (project context)
- @ENTERPRISE_MODERNIZATION_SUITE_ARCHITECTURE.md (strategic vision)
```

---

### Issue 2: Risk Score Seems Wrong

**Problem**: Risk score 24/100 but component feels risky

**Solution**:
```
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml

Re-assess risk for V_DETAIL with updated factors:

Override default assumptions:
- Team readiness: LOW (not MEDIUM) - no React experience
- Testing coverage: HIGH (not MEDIUM) - zero test automation
- Timeline pressure: HIGH (not LOW) - aggressive 6-month deadline

Recalculate risk score and update recommendation.
```

---

### Issue 3: Want Different Technology

**Problem**: Recommended React but want to evaluate Vue.js

**Solution**:
```
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml

Compare React vs Vue.js for WebDynpro replacement.

Context: [same as before]

Criteria:
- Learning curve (weight: 30%) - Team has no modern frontend experience
- Component model match (weight: 25%) - Must match WebDynpro patterns
- Enterprise adoption (weight: 20%)
- Ecosystem maturity (weight: 15%)
- Long-term viability (weight: 10%)

Score both and recommend.
```

---

## 🎯 Best Practices

### 1. **Always Provide Context**

❌ **Bad**:
```
Recommend modernization for V_DETAIL.
```

✅ **Good**:
```
Recommend modernization for V_DETAIL.

Context:
- Team: 4 developers (2 backend ABAP, 2 frontend WebDynpro)
- Timeline: 12 months
- Budget: $300K
- Requirements: 99.9% uptime, HIPAA compliance
- Strategic goals: AWS migration, cost reduction
```

### 2. **Be Specific About Constraints**

❌ **Bad**:
```
Budget is limited.
```

✅ **Good**:
```
Budget: $300K total
Breakdown preference: 60% development, 20% training, 10% infra, 10% contingency
```

### 3. **Define Success Criteria**

❌ **Bad**:
```
Improve performance.
```

✅ **Good**:
```
Success criteria:
- Page load < 2 seconds (currently 8 seconds)
- 99.9% uptime (currently 98%)
- 90% user satisfaction (currently 65%)
- 50% faster feature development
```

### 4. **Validate Assumptions**

After receiving recommendation:
```
@RECOMMENDATION_REPORT.md

Assumptions made in this report:
1. Team can learn React in 2 months
2. AWS costs $2.5K/month
3. Migration takes 12 months

Validate these assumptions:
- Can team really learn React in 2 months given current workload?
- Are AWS costs accurate for 500 concurrent users?
- Is 12-month timeline realistic given only 4 developers?

Flag any unrealistic assumptions and provide alternative estimates.
```

---

## 💡 Pro Tips

### Tip 1: **Start with Simple Components**

```
Strategy: Prove approach with low-risk component first

1. Get recommendation for simple screen (e.g., V_DIAGNOSIS)
2. Implement as pilot (2-4 weeks)
3. Validate costs, timeline, team learning
4. Use learnings to refine recommendations for complex screens
```

### Tip 2: **Compare Multiple Strategies**

```
@THE_RECOMMENDER_AGENT/recommendation_strategies.yaml
@WD/SCREENS/03_V_DETAIL_SCREEN/README.md

Generate 3 alternative strategies for V_DETAIL:

Option 1: Complete Rewrite (Greenfield)
Option 2: Incremental Refactor (Brownfield)
Option 3: Replatform (Lift & Shift Plus)

Compare on:
- Risk (score)
- Cost (total)
- Timeline (months)
- Business value (ROI)

Rank all 3 and explain why top choice wins.
```

### Tip 3: **Use for Decision Documentation**

```
Purpose: Document why we chose React over Angular

@THE_RECOMMENDER_AGENT outputs TECHNOLOGY_COMPARISON.md

This serves as:
- Decision record (for future reference)
- Onboarding material (explains stack choices)
- Stakeholder communication (justifies investment)
```

### Tip 4: **Iterate on Recommendations**

```
First pass: Get initial recommendation
Review: Identify gaps or concerns
Second pass: Refine with additional context
Validate: Check quality score
Finalize: Ready for stakeholder presentation
```

---

## 📞 Getting Help

### If Stuck

**Ask the agent**:
```
@THE_RECOMMENDER_AGENT/agent_specification.md

I'm stuck at [specific step]. Here's what I'm trying to do:
[describe goal]

Here's what's not working:
[describe issue]

What should I do?
```

### Common Questions

**Q: How long does analysis take?**
A:
- Single component: 5-10 minutes
- Full system (33 screens): 2-3 hours
- Technology comparison: 2-3 minutes
- Risk assessment: 3-5 minutes

**Q: Can I customize recommendation strategies?**
A: Yes! Edit `recommendation_strategies.yaml`:
- Add new technology alternatives
- Adjust risk factor weights
- Customize cost models
- Define your own migration strategies

**Q: How accurate are cost estimates?**
A: Within 20% for typical projects when context is accurate. Improve accuracy by:
- Providing team salary data
- Validating infrastructure costs
- Including historical project data

**Q: Can I use for non-SAP technologies?**
A: Yes! The agent supports:
- AS/400 (RPG, COBOL)
- Legacy .NET
- Old Java (pre-Spring Boot)
- Mainframe systems
- Custom/proprietary systems

Just provide documentation from the_documenter.

---

## ✅ Activation Checklist

**Before starting:**
- [ ] Component documented by the_documenter (7 files)
- [ ] Project context defined (team, budget, timeline, goals)
- [ ] Success criteria identified
- [ ] Stakeholders ready to review recommendations

**During analysis:**
- [ ] Provide comprehensive context
- [ ] Validate assumptions
- [ ] Review alternatives considered
- [ ] Check quality score

**After recommendation:**
- [ ] Present to stakeholders
- [ ] Get approval/feedback
- [ ] Refine if needed
- [ ] Pass to Stage 3 (development_agent) when ready

---

## 🎯 Ready to Start?

**Simplest Command**:

```
@THE_RECOMMENDER_AGENT/agent_specification.md
@WD/SCREENS/17_V_DIAGNOSIS_SCREEN/README.md

Recommend modernization for V_DIAGNOSIS screen.
Team: 4 developers (ABAP background)
Timeline: 3 months
Budget: $50K
```

**The Recommender will handle the rest!** 🚀

---

*End of Cursor Integration Guide*
*The Recommender Agent v1.0.0*
