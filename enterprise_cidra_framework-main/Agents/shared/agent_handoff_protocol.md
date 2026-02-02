# CIDRA Agent Handoff Protocol

**Version**: 1.0.0
**Purpose**: Define deterministic file-based communication between CIDRA agents
**Scope**: Chunker → Documenter → Recommender pipeline

---

## Overview

CIDRA agents communicate through **deterministic files** with stable names and locations. This ensures reliable pipeline execution regardless of which IDE or execution method is used.

---

## Pipeline Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CIDRA Pipeline                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│   Stage 0              Stage 1                 Stage 2               │
│   ┌─────────────┐     ┌─────────────┐        ┌─────────────┐        │
│   │  CHUNKER    │────▶│ DOCUMENTER  │───────▶│ RECOMMENDER │        │
│   └─────────────┘     └─────────────┘        └─────────────┘        │
│         │                    │                      │                │
│         ▼                    ▼                      ▼                │
│   CHUNKS/             Screens/<COMP>/       RECOMMENDATIONS/        │
│   repository.json     01_SPECIFICATION.md   RECOMMENDATION_REPORT    │
│   graph.json          02_UI_MOCKUP.md       recommendation.json     │
│   DOCUMENTER_         03_TECHNICAL.md       run_manifest.json       │
│   INSTRUCTIONS.md     ...                                           │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Output Directories

| Agent | Output Path | Purpose |
|-------|-------------|---------|
| **Chunker** | `CHUNKS/` | Optimized code chunks for LLM digestion |
| **Documenter** | `Screens/<COMPONENT>/` | 7-file documentation per component |
| **Recommender** | `RECOMMENDATIONS/<COMPONENT>/` | Modernization recommendations |

---

## Handoff Artifacts

### Chunker → Documenter

**Output Directory**: `CHUNKS/`

**Required Files**:

| File | Purpose | Format |
|------|---------|--------|
| `repository.json` | All chunks with metadata | JSON |
| `graph.json` | Dependency relationships | JSON |
| `analysis.json` | Codebase inventory | JSON |
| `DOCUMENTER_INSTRUCTIONS.md` | Human-readable handoff | Markdown |
| `run_manifest.json` | Machine-readable run info | JSON |

**DOCUMENTER_INSTRUCTIONS.md Template**:
```markdown
# Chunker Output - Instructions for Documenter

## Run Information
- **Run ID**: 2026-01-28T10:30:00Z
- **Codebase**: [project_name]
- **Files Processed**: 789
- **Chunks Created**: 1,247

## Key Findings
- Languages detected: ABAP, WebDynpro
- Shared components: 8 identified
- Component Controller: CHUNKS/chunk_008.json

## Next Steps for Documenter
1. Read repository.json for chunk inventory
2. Use graph.json for dependency analysis
3. Start with simple components first
4. Check shared components before documenting

## Components to Document
[List of components with priority]
```

---

### Documenter → Recommender

**Output Directory**: `Screens/<COMPONENT>/`

**Required Files** (7-file structure):

| File | Purpose |
|------|---------|
| `01_SCREEN_SPECIFICATION.md` | Technical specification |
| `02_UI_MOCKUP.md` | UI structure |
| `03_TECHNICAL_ANALYSIS.md` | Code analysis |
| `04_BUSINESS_LOGIC.md` | Business rules |
| `05_CODE_ARTIFACTS.md` | Code snippets |
| `README.md` | Overview |
| `VALIDATION_REPORT.md` or `דוח_אימות_*.md` | Quality report |

**Validation Requirements**:
- All 7 files must exist
- Validation score must be 100/100
- No forbidden words present
- All counts verified

---

### Recommender Outputs

**Output Directory**: `RECOMMENDATIONS/<COMPONENT>/`

**Required Files**:

| File | Purpose | Audience |
|------|---------|----------|
| `RECOMMENDATION_REPORT.md` | Full report (20-40 pages) | Decision makers |
| `TECHNOLOGY_COMPARISON.md` | Tech alternatives | Architects |
| `ARCHITECTURE_DIAGRAMS.md` | Current vs target | Architects |
| `MIGRATION_ROADMAP.md` | Step-by-step plan | Developers |
| `RISK_ASSESSMENT.md` | Risk analysis | Managers |
| `COST_BENEFIT_ANALYSIS.md` | Financial model | Executives |
| `recommendation_structured.json` | Machine-readable | Stage 3 agent |
| `run_manifest.json` | Run metadata | Pipeline |

---

## run_manifest.json Schema

Every agent run produces a `run_manifest.json` file to enable pipeline orchestration:

```json
{
  "cidra_version": "1.0.0",
  "agent_id": "THE_CHUNKER_AGENT",
  "agent_version": "1.0.0",
  "run_id": "2026-01-28T10:30:00Z",
  "status": "completed",
  "duration_seconds": 720,

  "inputs": [
    {
      "path": "Source Code/ABAP/",
      "type": "directory",
      "files_count": 656
    },
    {
      "path": "Source Code/WD/",
      "type": "directory",
      "files_count": 133
    }
  ],

  "outputs": [
    {
      "path": "CHUNKS/repository.json",
      "type": "file",
      "chunks_count": 1247
    },
    {
      "path": "CHUNKS/graph.json",
      "type": "file",
      "relationships_count": 3450
    },
    {
      "path": "CHUNKS/DOCUMENTER_INSTRUCTIONS.md",
      "type": "file"
    }
  ],

  "next_agent": "THE_DOCUMENTER_AGENT",

  "notes": [
    "8 shared components identified",
    "Component Controller at chunk_008"
  ],

  "quality_metrics": {
    "coverage": 100,
    "chunk_size_compliance": 96,
    "metadata_completeness": 98.5
  }
}
```

---

## Prerequisite Checking

**Downstream agents MUST check prerequisites before execution.**

### Documenter Prerequisites
```yaml
required:
  - path: "CHUNKS/repository.json"
    check: "file_exists"
  - path: "CHUNKS/graph.json"
    check: "file_exists"

recommended:
  - path: "CHUNKS/DOCUMENTER_INSTRUCTIONS.md"
    check: "file_exists"
  - path: "CHUNKS/run_manifest.json"
    check: "file_exists AND status == completed"

on_missing:
  message: "Chunker output not found. Run /chunk first."
  suggest: "/chunk [source_path]"
```

### Recommender Prerequisites
```yaml
required:
  - path: "Screens/<COMPONENT>/README.md"
    check: "file_exists"
  - path: "Screens/<COMPONENT>/01_SCREEN_SPECIFICATION.md"
    check: "file_exists"

recommended:
  - path: "Screens/<COMPONENT>/VALIDATION_REPORT.md"
    check: "score == 100"

on_missing:
  message: "Documentation not found for <COMPONENT>. Run /document first."
  suggest: "/document <COMPONENT>"

on_invalid:
  message: "Documentation for <COMPONENT> has score < 100. Run /document:fix first."
  suggest: "/document:fix Screens/<COMPONENT>/"
```

---

## Error Handling

### Handoff Failure Scenarios

| Scenario | Detection | Resolution |
|----------|-----------|------------|
| Missing upstream output | File not found | Run upstream agent first |
| Incomplete output | Missing required files | Re-run upstream agent |
| Invalid output | Validation failed | Fix upstream issues |
| Version mismatch | Schema incompatible | Update agent version |

### Error Messages

```yaml
errors:
  MISSING_CHUNKS:
    code: "E001"
    message: "CHUNKS/ directory not found"
    action: "Run /chunk [source_path] to generate chunks"

  MISSING_DOCUMENTATION:
    code: "E002"
    message: "Documentation for {component} not found"
    action: "Run /document {component} to generate documentation"

  LOW_QUALITY_SCORE:
    code: "W001"
    message: "Documentation score is {score}/100, not 100/100"
    action: "Run /document:fix to repair issues"

  INCOMPLETE_HANDOFF:
    code: "E003"
    message: "run_manifest.json indicates incomplete run"
    action: "Re-run upstream agent"
```

---

## Best Practices

### For Agent Developers

1. **Always produce run_manifest.json** - Enables pipeline orchestration
2. **Validate outputs before completing** - Don't mark done if incomplete
3. **Include clear next_agent** - Enables automatic pipeline continuation
4. **Document limitations** - Help downstream agents understand constraints

### For Pipeline Orchestrators

1. **Check prerequisites** - Don't run if upstream incomplete
2. **Validate handoff files** - Ensure required artifacts exist
3. **Handle errors gracefully** - Provide clear resolution steps
4. **Log transitions** - Track pipeline progress

### For Users

1. **Follow the pipeline order** - Chunker → Documenter → Recommender
2. **Check status before proceeding** - Use /chunk:status, /document:validate
3. **Don't skip steps** - Each stage depends on previous
4. **Review handoff instructions** - Read DOCUMENTER_INSTRUCTIONS.md

---

## Registry Integration

This protocol is enforced via `registry.yaml`:

```yaml
THE_CHUNKER_AGENT:
  consumes:
    - type: "source_code"
      path: "Source Code/"
  produces:
    - type: "chunks"
      path: "CHUNKS/repository.json"
    - type: "relationships"
      path: "CHUNKS/graph.json"
    - type: "handoff"
      file: "CHUNKS/DOCUMENTER_INSTRUCTIONS.md"
    - type: "manifest"
      file: "CHUNKS/run_manifest.json"
  next: "THE_DOCUMENTER_AGENT"

THE_DOCUMENTER_AGENT:
  consumes:
    - type: "chunks"
      path: "CHUNKS/"
    - type: "source_code"
      path: "Source Code/"
  produces:
    - type: "documentation"
      path: "Screens/<COMPONENT>/"
      files: 7
    - type: "manifest"
      file: "Screens/<COMPONENT>/run_manifest.json"
  next: "THE_RECOMMENDER_AGENT"

THE_RECOMMENDER_AGENT:
  consumes:
    - type: "documentation"
      path: "Screens/<COMPONENT>/"
  produces:
    - type: "recommendations"
      path: "RECOMMENDATIONS/<COMPONENT>/"
    - type: "manifest"
      file: "RECOMMENDATIONS/<COMPONENT>/run_manifest.json"
  next: "THE_APPLICATOR_AGENT"  # Future
```

---

*Agent Handoff Protocol v1.0.0*
*CIDRA Framework*
