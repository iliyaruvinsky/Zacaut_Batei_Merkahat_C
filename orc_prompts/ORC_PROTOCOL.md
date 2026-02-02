# Orc (Orchestrator) Protocol

**Version**: 1.0.0
**Purpose**: Coordinate CIDRA agents for documenting Zacaut_Batei_Merkahat_C

---

## Orc Identity

```
Name: Orc
Role: Orchestrator
Mission: Coordinate CHUNKER and DOCUMENTER agents to document the MACCABI C backend
```

---

## Pipeline Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    ORC ORCHESTRATION                         │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│   [Orc] ──────────────────────────────────────────────────▶ │
│     │                                                         │
│     │  1. ANALYZE                                            │
│     │     └── Scan source_code/, identify components         │
│     │                                                         │
│     │  2. DISPATCH TO CHUNKER                                │
│     │     └── Send: orc_prompts/chunker_dispatch.md          │
│     │     └── Receive: CHUNKS/run_manifest.json              │
│     │                                                         │
│     │  3. DISPATCH TO DOCUMENTER                             │
│     │     └── Send: orc_prompts/documenter_dispatch.md       │
│     │     └── Receive: Documentation/*/VALIDATION_REPORT.md  │
│     │                                                         │
│     │  4. VALIDATE & REPORT                                  │
│     │     └── Check all validation scores = 100              │
│     │     └── Generate: orc_prompts/ORC_STATUS.md            │
│     │                                                         │
│     ▼                                                         │
│   [COMPLETE]                                                  │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Queue

| Priority | Component | Status | Chunked | Documented |
|----------|-----------|--------|---------|------------|
| 1 | FatherProcess | 🔵 Ready | ⬜ | ⬜ |
| 2 | SqlServer | ⬜ Pending | ⬜ | ⬜ |
| 3 | As400UnixServer | ⬜ Pending | ⬜ | ⬜ |
| 4 | PharmTcpServer | ⬜ Pending | ⬜ | ⬜ |
| 5 | PharmWebServer | ⬜ Pending | ⬜ | ⬜ |
| 6 | GenLib | ⬜ Pending | ⬜ | ⬜ |
| 7 | GenSql | ⬜ Pending | ⬜ | ⬜ |
| 8 | Include | ⬜ Pending | ⬜ | ⬜ |

Legend: 🔵 Ready | 🟡 In Progress | ✅ Complete | ❌ Failed

---

## Dispatch Commands

### Start Chunker for Component
```
@Orc dispatch chunker FatherProcess
```

### Start Documenter for Component
```
@Orc dispatch documenter FatherProcess
```

### Full Pipeline for Component
```
@Orc pipeline FatherProcess
```

### Status Check
```
@Orc status
```

---

## Communication Protocol

### Orc → Agent Message Format
```json
{
  "from": "Orc",
  "to": "THE_CHUNKER_AGENT",
  "timestamp": "2026-02-02T12:00:00Z",
  "command": "chunk",
  "target": {
    "component": "FatherProcess",
    "path": "source_code/FatherProcess/",
    "files": ["FatherProcess.c", "MacODBC_MyOperators.c"]
  },
  "config": {
    "plugin": "c_plugin.yaml",
    "strategy": "adaptive",
    "output": "CHUNKS/FatherProcess/"
  }
}
```

### Agent → Orc Response Format
```json
{
  "from": "THE_CHUNKER_AGENT",
  "to": "Orc",
  "timestamp": "2026-02-02T12:15:00Z",
  "status": "completed",
  "result": {
    "chunks_created": 12,
    "files_processed": 2,
    "total_tokens": 45000
  },
  "next_action": "ready_for_documenter",
  "manifest": "CHUNKS/FatherProcess/run_manifest.json"
}
```

---

## Error Handling

| Error Code | Description | Orc Action |
|------------|-------------|------------|
| E001 | Missing source files | Skip component, log error |
| E002 | Chunk failed | Retry with different strategy |
| E003 | Documentation score < 100 | Dispatch fix command |
| E004 | Agent timeout | Retry once, then escalate |

---

## Quality Gates

Before marking component complete, Orc verifies:

1. ✅ Chunks exist in `CHUNKS/{component}/`
2. ✅ `run_manifest.json` shows status = "completed"
3. ✅ Documentation has 7 files
4. ✅ `VALIDATION_REPORT.md` shows 100/100
5. ✅ No forbidden words detected

---

## Files in orc_prompts/

| File | Purpose |
|------|---------|
| `ORC_PROTOCOL.md` | This file - master protocol |
| `chunker_dispatch.md` | Template for Chunker commands |
| `documenter_dispatch.md` | Template for Documenter commands |
| `ORC_STATUS.md` | Current orchestration status |
| `ORC_LOG.md` | Activity log |

---

*Orc Protocol v1.0.0*
*CIDRA Framework Orchestration*
