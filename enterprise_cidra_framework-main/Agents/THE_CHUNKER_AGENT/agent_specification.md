# The Chunker Agent - Specification
**Version**: 1.0.0
**Date**: November 3, 2025
**Purpose**: Optimize code chunks for LLM digestion and RAG operations

---

## 🎯 Mission Statement

**Create semantically meaningful, optimally-sized code chunks that maximize LLM understanding and RAG retrieval effectiveness across enterprise codebases.**

---

## 🧠 Core Capabilities

### 1. **Intelligent Code Segmentation**
Break code into semantically coherent chunks that:
- Respect logical boundaries (functions, classes, modules)
- Maintain context relationships
- Optimize for token limits (2K-4K tokens per chunk)
- Preserve cross-references and dependencies

### 2. **Multi-Language Analysis**
Support enterprise technology stack:
- **SAP ABAP**: Classes, methods, function modules, includes
- **AS/400**: RPG programs, procedures, copy members, DDS
- **JavaScript/TypeScript**: Functions, components, modules
- **Python**: Classes, functions, modules
- **React**: Components, hooks, contexts
- **SQL**: Procedures, triggers, views

### 3. **Relationship Mapping**
Track and document:
- Dependencies (imports, includes, calls)
- Hierarchies (class inheritance, module nesting)
- Cross-references (shared vs unique components)
- Data flow (input/output relationships)
- Business logic chains

### 4. **Metadata Generation**
For each chunk, create:
- **Semantic summary** (what does this code do?)
- **Purpose tags** (validation, data retrieval, UI, etc.)
- **Dependency list** (what does it need?)
- **Used-by list** (what uses it?)
- **Complexity score** (simple, medium, complex)
- **Embedding-friendly description** (for vector search)

### 5. **RAG Optimization**
Ensure chunks are:
- **Self-contained** (understandable in isolation)
- **Context-rich** (include necessary context)
- **Query-friendly** (match common search patterns)
- **Relationship-aware** (link to related chunks)
- **Embedding-optimized** (clear, concise descriptions)

---

## 📊 Chunking Strategies

### Strategy 1: **File-Level Chunking**
**When to use**: Small files (<4K tokens)

**Process**:
1. Keep entire file as one chunk
2. Add file-level metadata
3. Extract key elements (functions, classes)
4. Generate summary
5. Tag with purpose and type

**Example** (SAP ABAP):
```yaml
chunk_id: "chunk_001_method_check_patient_id"
type: "file_complete"
file: "0001ZZ_CL_DOCTORS_CONTROLLING=====CM001.txt"
language: "ABAP"
tokens: 350
content: "[Full method implementation]"
summary: "Validates Israeli patient ID using check digit algorithm"
tags: ["validation", "patient", "healthcare", "id_verification"]
dependencies: ["/ATL/L_CHECK_DIGIT"]
used_by: ["check_treatment", "check_visit"]
complexity: "low"
```

---

### Strategy 2: **Function/Method-Level Chunking**
**When to use**: Medium files (4K-20K tokens)

**Process**:
1. Split by function/method boundaries
2. Include function signature + implementation
3. Add local context (relevant data structures)
4. Link to calling/called functions
5. Generate method-specific summary

**Example** (AS/400 RPG):
```yaml
chunk_id: "chunk_042_proc_calculate_balance"
type: "procedure"
file: "ACCTPROC.RPGLE"
language: "RPG_ILE"
tokens: 1200
content: "[Procedure with local variables and logic]"
summary: "Calculates account balance with interest and fees"
tags: ["calculation", "accounting", "financial", "balance"]
dependencies: ["GET_ACCOUNT_DATA", "APPLY_INTEREST", "DS_ACCOUNT"]
used_by: ["PROCESS_STATEMENT", "CHECK_OVERDRAFT"]
complexity: "medium"
related_chunks: ["chunk_015_ds_account", "chunk_028_apply_interest"]
```

---

### Strategy 3: **Class-Level Chunking**
**When to use**: Object-oriented code with clear class boundaries

**Process**:
1. One chunk per class (if <8K tokens)
2. Include class definition + all methods
3. Extract method summaries separately
4. Link to parent/child classes
5. Document interfaces implemented

**Example** (SAP ABAP):
```yaml
chunk_id: "chunk_100_class_zz_cl_doctors_controlling"
type: "class_complete"
file: "Multiple files (0001-0023)"
language: "ABAP"
tokens: 6500
content: "[Inferred class structure + 23 methods]"
summary: "Central business logic helper for Maccabi ICM Control App"
tags: ["business_logic", "validation", "authorization", "helper_class"]
methods: [
  "check_patient_id",
  "check_treatment",
  "get_patient_details",
  # ... 20 more
]
dependencies: [
  "/ATL/L_CHECK_DIGIT",
  "ZBW_RFC_DESTINATION",
  "GET_PATIENT_DATA"
]
used_by: ["All 33 WebDynpro controllers"]
complexity: "high"
related_chunks: [
  "chunk_200_webdynpro_component_controller",
  "chunk_050_patient_structures"
]
```

---

### Strategy 4: **Semantic Grouping**
**When to use**: Related functions that form a business process

**Process**:
1. Group related functions (e.g., all validation methods)
2. Create super-chunk with context
3. Link to individual method chunks
4. Document the business flow
5. Tag with business process name

**Example** (JavaScript):
```yaml
chunk_id: "chunk_250_auth_flow"
type: "semantic_group"
files: ["auth.js", "token.js", "session.js"]
language: "JavaScript"
tokens: 3800
content: "[Login, token generation, session management]"
summary: "Complete authentication flow from login to session"
tags: ["authentication", "security", "session", "jwt"]
business_process: "user_authentication"
includes_functions: [
  "validateCredentials",
  "generateJWT",
  "createSession",
  "validateToken"
]
dependencies: ["bcrypt", "jsonwebtoken", "redis"]
complexity: "medium"
flow: "Login → Token → Session → Validation"
```

---

### Strategy 5: **Cross-Reference Chunking**
**When to use**: Shared components referenced by multiple modules

**Process**:
1. Identify shared components (Component Controller, copy members)
2. Create separate chunk for shared element
3. Link all users to this chunk
4. Mark as "shared" in metadata
5. Include usage statistics

**Example** (SAP WebDynpro):
```yaml
chunk_id: "chunk_008_component_controller"
type: "shared_component"
file: "0008_1BCWDY_S_0O2THA8S1FYRM8B3AW18.txt"
language: "ABAP_WebDynpro"
tokens: 2100
content: "[Component Controller interface with shared nodes]"
summary: "Shared context nodes and methods for all 33 screens"
tags: ["shared", "component_controller", "context_nodes"]
shared_nodes: [
  "GT_TAB",
  "GT_TREATMENTS",
  "SELECTION",
  # ... more
]
used_by: [
  "V_SELECT",
  "V_MAIN",
  "V_DETAIL",
  # ... all 33 screens
]
usage_count: 33
complexity: "medium"
importance: "critical"
```

---

## 🔍 Metadata Schema

### Core Metadata (Required for All Chunks)

```yaml
chunk_id: string              # Unique identifier
type: enum                    # file_complete, procedure, class, semantic_group, shared
file: string                  # Source file path
language: string              # Programming language
tokens: integer               # Token count (for context window planning)
content: string               # Actual code content
summary: string               # Human-readable summary (1-2 sentences)
tags: array[string]           # Semantic tags for search
dependencies: array[string]   # What this chunk needs
used_by: array[string]        # What uses this chunk
complexity: enum              # low, medium, high
created_at: datetime          # Chunk creation timestamp
version: string               # Codebase version
```

### Extended Metadata (Optional but Recommended)

```yaml
# Relationships
related_chunks: array[string]     # Semantically related chunks
parent_chunk: string              # Parent in hierarchy
child_chunks: array[string]       # Children in hierarchy
calls: array[string]              # Functions/methods called
called_by: array[string]          # Functions/methods that call this

# Business Context
business_process: string          # Business process name
business_rules: array[string]     # Business rules implemented
data_entities: array[string]      # Data entities involved
user_roles: array[string]         # Affected user roles

# Technical Details
database_tables: array[string]    # Tables accessed
external_systems: array[string]   # External integrations
performance_notes: string         # Performance considerations
security_notes: string            # Security considerations

# Search Optimization
embedding_description: string     # Optimized for vector embeddings
search_keywords: array[string]    # Additional search keywords
common_queries: array[string]     # Queries this chunk answers

# Quality Metrics
test_coverage: float             # Test coverage percentage
last_modified: datetime          # Last code modification
author: string                   # Primary author
review_status: enum              # unreviewed, reviewed, approved
```

---

## 🎯 Chunking Workflow

### Phase 1: **Discovery & Analysis** (10% of time)

**Input**: Codebase directory
**Output**: File inventory with metadata

```yaml
Steps:
1. Scan all files in codebase
2. Identify file types and languages
3. Count lines and estimate tokens
4. Detect dependencies (imports, includes)
5. Build file relationship graph
6. Identify shared components
7. Categorize by complexity
8. Generate file inventory report
```

**Example Output**:
```
Codebase: MACCABI_ICM_CONTROL_APP
Total Files: 789
Languages: ABAP (656), WebDynpro (133)
Total Tokens: ~2.5M
Shared Components: 8 identified
Average File Size: 3,200 tokens
Complexity Distribution: 20% low, 60% medium, 20% high
```

---

### Phase 2: **Strategic Planning** (10% of time)

**Input**: File inventory + analysis
**Output**: Chunking strategy per file/component

```yaml
For each file:
  if tokens < 4K:
    strategy: "file_complete"
  else if has_clear_functions:
    strategy: "function_level"
  else if is_class:
    strategy: "class_level"
  else:
    strategy: "semantic_grouping"

For shared components:
  strategy: "cross_reference"
  priority: "high"

For complex files (>20K tokens):
  strategy: "multi_level"
  split_by: ["class", "function", "section"]
```

---

### Phase 3: **Chunking Execution** (60% of time)

**Input**: Chunking strategy
**Output**: Optimized chunks with metadata

```yaml
For each file:
  1. Parse code (AST if possible, regex if needed)
  2. Identify boundaries (functions, classes, sections)
  3. Extract code segments
  4. Generate summaries using LLM
  5. Extract dependencies
  6. Build relationship links
  7. Tag with semantic labels
  8. Calculate complexity
  9. Create metadata record
  10. Validate chunk quality
  11. Save to chunk repository
```

**Quality Checks**:
- ✓ Chunk size within limits (500-8K tokens)
- ✓ Summary is clear and accurate
- ✓ Dependencies are complete
- ✓ Tags are relevant
- ✓ Relationships are mapped
- ✓ Content is parseable
- ✓ Metadata is complete

---

### Phase 4: **Relationship Mapping** (15% of time)

**Input**: All chunks
**Output**: Relationship graph

```yaml
1. Build dependency graph (who calls whom)
2. Create hierarchy tree (parent-child relationships)
3. Identify clusters (related chunks)
4. Map data flows (input → processing → output)
5. Link business processes
6. Generate relationship visualization
7. Create navigation indexes
```

**Output Formats**:
- JSON graph for programmatic access
- Markdown documentation for humans
- Cypher queries for Neo4j (optional)
- DOT file for Graphviz (optional)

---

### Phase 5: **Optimization & Validation** (5% of time)

**Input**: Chunks + relationships
**Output**: Validated, optimized chunk repository

```yaml
Optimizations:
1. Merge tiny chunks (<500 tokens) if semantically related
2. Split huge chunks (>8K tokens) if boundaries exist
3. Deduplicate identical chunks
4. Enrich metadata with cross-references
5. Generate embedding descriptions
6. Create search indexes
7. Build chunk navigation maps

Validation:
1. Check all dependencies resolve
2. Verify all relationships are bidirectional
3. Ensure no orphan chunks
4. Validate metadata completeness
5. Test RAG retrieval quality
6. Measure search effectiveness
```

---

## 🛠️ Technical Implementation

### Architecture

```
┌─────────────────────────────────────────────────────┐
│                  THE CHUNKER AGENT                  │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐│
│  │   Parser     │  │  Analyzer    │  │ Chunker  ││
│  │   Engine     │→ │   Engine     │→ │  Engine  ││
│  └──────────────┘  └──────────────┘  └──────────┘│
│         ↓                  ↓                ↓      │
│  ┌──────────────────────────────────────────────┐ │
│  │         Metadata Generator                   │ │
│  └──────────────────────────────────────────────┘ │
│         ↓                                          │
│  ┌──────────────────────────────────────────────┐ │
│  │         Relationship Mapper                  │ │
│  └──────────────────────────────────────────────┘ │
│         ↓                                          │
│  ┌──────────────────────────────────────────────┐ │
│  │       RAG Optimization Engine                │ │
│  └──────────────────────────────────────────────┘ │
│         ↓                                          │
│  ┌──────────────────────────────────────────────┐ │
│  │         Output Formatters                    │ │
│  │  (JSON, Markdown, Vector DB, Graph DB)      │ │
│  └──────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

### Parser Engine

**Responsibilities**:
- Language detection
- AST generation (if supported)
- Token counting
- Boundary detection
- Syntax validation

**Supported Parsers**:
- **JavaScript/TypeScript**: Babel, TypeScript Compiler API
- **Python**: ast module
- **ABAP**: Regex-based (no standard AST parser)
- **AS/400**: Regex-based (RPG/COBOL patterns)
- **SQL**: sqlparse library

---

### Analyzer Engine

**Responsibilities**:
- Dependency extraction
- Relationship identification
- Complexity calculation
- Pattern detection
- Shared component identification

**Analysis Techniques**:
- **Static analysis**: Code structure, imports, calls
- **Pattern matching**: Common coding patterns
- **Graph analysis**: Call graphs, dependency graphs
- **Heuristics**: File size, cyclomatic complexity

---

### Chunker Engine

**Responsibilities**:
- Boundary identification
- Code extraction
- Context preservation
- Size optimization
- Quality validation

**Chunking Algorithms**:
1. **Greedy splitter**: Split at next boundary when size limit reached
2. **Semantic splitter**: Split at semantic boundaries (function/class)
3. **Balanced splitter**: Minimize variance in chunk sizes
4. **Context-aware splitter**: Maximize context preservation

---

### Metadata Generator

**Responsibilities**:
- Summary generation (using LLM)
- Tag extraction (heuristics + LLM)
- Dependency listing
- Relationship mapping
- Embedding description creation

**LLM Integration**:
```yaml
Prompt Template:
"Analyze this code and provide:
1. One-sentence summary of purpose
2. 3-5 semantic tags
3. Key dependencies
4. What code uses this
5. Complexity level (low/medium/high)
6. A 2-3 sentence description optimized for semantic search

Code:
[CHUNK_CONTENT]"
```

---

### Relationship Mapper

**Responsibilities**:
- Build call graph
- Create hierarchy tree
- Map data flows
- Link business processes
- Generate navigation indexes

**Output**:
```json
{
  "chunk_id": "chunk_042",
  "relationships": {
    "calls": ["chunk_015", "chunk_028"],
    "called_by": ["chunk_055", "chunk_103"],
    "depends_on": ["chunk_008"],
    "used_by": ["chunk_200", "chunk_201"],
    "related_to": ["chunk_043", "chunk_044"],
    "parent": "chunk_040",
    "children": ["chunk_042_sub1", "chunk_042_sub2"]
  }
}
```

---

### RAG Optimization Engine

**Responsibilities**:
- Chunk size balancing
- Context enrichment
- Query pattern matching
- Embedding optimization
- Search index generation

**Optimization Techniques**:
1. **Context injection**: Add just-enough context for understanding
2. **Summary placement**: Put summary at top for better retrieval
3. **Keyword enrichment**: Add relevant keywords to metadata
4. **Chunk linking**: Strong bidirectional links
5. **Deduplication**: Remove redundant chunks

---

## 📦 Output Formats

### Format 1: **JSON Repository**
```json
{
  "metadata": {
    "project": "MACCABI_ICM_CONTROL_APP",
    "version": "1.0.0",
    "chunked_at": "2025-11-03T10:00:00Z",
    "total_chunks": 1247,
    "total_tokens": 2500000,
    "languages": ["ABAP", "WebDynpro"]
  },
  "chunks": [
    {
      "chunk_id": "chunk_001",
      "type": "file_complete",
      "file": "0001ZZ_CL_DOCTORS_CONTROLLING=====CM001.txt",
      "language": "ABAP",
      "tokens": 350,
      "content": "[CODE]",
      "summary": "Validates Israeli patient ID",
      "tags": ["validation", "patient", "id"],
      "dependencies": ["/ATL/L_CHECK_DIGIT"],
      "used_by": ["check_treatment", "check_visit"],
      "complexity": "low",
      "relationships": {
        "calls": [],
        "called_by": ["chunk_002", "chunk_011"]
      }
    }
  ],
  "relationships": {
    "dependency_graph": {},
    "call_graph": {},
    "hierarchy": {}
  },
  "indexes": {
    "by_file": {},
    "by_language": {},
    "by_tag": {},
    "by_complexity": {}
  }
}
```

---

### Format 2: **Markdown Documentation**
```markdown
# Code Chunk Repository

## Overview
- Total Chunks: 1,247
- Total Tokens: 2.5M
- Languages: ABAP (89%), WebDynpro (11%)

## Chunk Index

### chunk_001: check_patient_id
**File**: 0001ZZ_CL_DOCTORS_CONTROLLING=====CM001.txt
**Language**: ABAP
**Tokens**: 350
**Summary**: Validates Israeli patient ID using check digit algorithm

**Dependencies**:
- /ATL/L_CHECK_DIGIT

**Used By**:
- check_treatment (chunk_002)
- check_visit (chunk_011)

**Tags**: validation, patient, healthcare, id_verification

[View Code](#chunk_001_code)
```

---

### Format 3: **Vector Database Format** (for Pinecone, Chroma, etc.)
```python
{
    "id": "chunk_001",
    "values": [0.1, 0.2, ...],  # Embedding vector
    "metadata": {
        "file": "0001ZZ_CL_DOCTORS_CONTROLLING=====CM001.txt",
        "summary": "Validates Israeli patient ID using check digit algorithm",
        "tags": ["validation", "patient", "healthcare"],
        "language": "ABAP",
        "tokens": 350,
        "complexity": "low",
        "dependencies": ["/ATL/L_CHECK_DIGIT"],
        "content": "[FULL CODE]"
    }
}
```

---

### Format 4: **Graph Database Format** (for Neo4j)
```cypher
// Chunk node
CREATE (c:Chunk {
  id: 'chunk_001',
  file: '0001ZZ_CL_DOCTORS_CONTROLLING=====CM001.txt',
  summary: 'Validates Israeli patient ID',
  language: 'ABAP',
  tokens: 350,
  complexity: 'low'
})

// Relationships
CREATE (c)-[:CALLS]->(dep:Dependency {name: '/ATL/L_CHECK_DIGIT'})
CREATE (caller:Chunk {id: 'chunk_002'})-[:CALLS]->(c)
CREATE (c)-[:TAGGED_WITH]->(t:Tag {name: 'validation'})
```

---

## 🎯 Use Cases

### Use Case 1: **AI-Assisted Code Understanding**

**Scenario**: Developer asks "How does patient validation work?"

**RAG Flow**:
1. Query: "patient validation"
2. Vector search returns top 5 chunks:
   - chunk_001 (check_patient_id) - 0.95 similarity
   - chunk_002 (check_treatment) - 0.88 similarity
   - chunk_011 (check_visit) - 0.87 similarity
   - chunk_007 (get_patient_details) - 0.82 similarity
   - chunk_050 (patient structures) - 0.78 similarity
3. LLM receives chunks + relationships
4. LLM generates answer with code examples

**Result**: Comprehensive answer citing specific code locations

---

### Use Case 2: **Impact Analysis**

**Scenario**: "What would break if I change /ATL/L_CHECK_DIGIT?"

**RAG Flow**:
1. Query: "/ATL/L_CHECK_DIGIT dependencies"
2. Graph traversal finds all chunks that depend on it
3. Returns: chunk_001, chunk_002, chunk_011
4. Follows "used_by" relationships
5. Finds affected screens: V_DETAIL, V_MAIN, V_APPROVE, etc.

**Result**: Complete impact map with affected components

---

### Use Case 3: **Documentation Generation**

**Scenario**: Generate API documentation from code

**RAG Flow**:
1. Retrieve all public methods chunks
2. Group by class/module
3. Extract summaries, parameters, return types
4. Link related methods
5. Generate markdown documentation

**Result**: Auto-generated, always-up-to-date API docs

---

### Use Case 4: **Code Search**

**Scenario**: "Find all methods that access zctle_ccntrl_trn table"

**RAG Flow**:
1. Query: "zctle_ccntrl_trn table access"
2. Filter chunks by database_tables metadata
3. Returns: chunk_012 (fix_table_zctle_ccntrl_trn)
4. Follow "called_by" relationships
5. Find all callers

**Result**: Complete list of methods accessing the table

---

## 🔧 IDE Integration

The Chunker is a **prompt-driven agent** designed for activation via AI-assisted IDEs.

### Cursor Integration (Recommended)

**Full guide**: [cursor_integration.md](cursor_integration.md)

**Quick Start**:
```
@THE_CHUNKER_AGENT/agent_specification.md
@THE_CHUNKER_AGENT/chunking_strategies.yaml

Analyze my codebase and create optimized chunks for RAG:
- Directory: [YOUR_CODE_PATH]
- Languages: ABAP, WebDynpro
- Strategy: adaptive
- Output: JSON repository + markdown docs

Follow the 5-phase workflow.
```

### Claude Code Integration

```
Read THE_CHUNKER_AGENT/agent_specification.md and chunking_strategies.yaml

Chunk my codebase following the 5-phase workflow:
1. Analyze - Scan [path] and identify structure
2. Chunk - Break into semantic pieces (2K-4K tokens)
3. Metadata - Generate summaries and tags
4. Relationships - Map dependencies
5. Export - Output to CHUNKS/
```

### VS Code Integration

Use CIDRA snippets for quick activation. See `Protocols/.vscode/cidra.code-snippets`.

> **Note**: Python automation scripts are planned for future implementation.
> Current execution model is prompt-driven via Cursor, Claude Code, or VS Code with AI extension.

---

## 📈 Success Metrics

### Chunking Quality

**Metrics to track**:
- **Chunk size distribution**: Target 2K-4K tokens/chunk
- **Coverage**: % of codebase successfully chunked
- **Boundary accuracy**: % of chunks respecting logical boundaries
- **Relationship completeness**: % of dependencies mapped
- **Metadata completeness**: % of required fields populated

**Quality Thresholds**:
- ✓ 90%+ chunks within size limits
- ✓ 100% codebase coverage
- ✓ 95%+ boundary accuracy
- ✓ 90%+ relationship completeness
- ✓ 100% core metadata complete

---

### RAG Effectiveness

**Metrics to track**:
- **Retrieval accuracy**: % of relevant chunks retrieved
- **Answer quality**: Human evaluation of LLM responses
- **Query latency**: Time to retrieve chunks
- **Context sufficiency**: % of queries needing only retrieved chunks

**Quality Thresholds**:
- ✓ 90%+ retrieval accuracy
- ✓ 85%+ answer quality rating
- ✓ <500ms query latency
- ✓ 95%+ context sufficiency

---

## 🎯 Next Steps

1. **Review this specification**
2. **Build Python scripts** for each phase
3. **Integrate with VS Code** (tasks.json)
4. **Test on MACCABI codebase** (789 files)
5. **Validate chunk quality** (metrics above)
6. **Export to vector database** (Pinecone/Chroma)
7. **Test RAG queries** with Claude/GPT-4
8. **Iterate and optimize** based on results

---

*End of Chunker Agent Specification*
*Ready for implementation*
