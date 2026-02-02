# The Chunker Agent - Cursor Integration Guide
**Version**: 1.0.0
**Date**: November 3, 2025
**Purpose**: Activate and use The Chunker in Cursor IDE

---

## 🚀 Quick Start (30 seconds)

**In Cursor chat, type:**

```
@THE_CHUNKER_AGENT/agent_specification.md
@THE_CHUNKER_AGENT/chunking_strategies.yaml

Analyze my MACCABI ICM codebase and create optimized chunks for RAG:
- Directories: WD/ (133 files), ABAP/ (656 files)
- Languages: ABAP, WebDynpro
- Strategy: adaptive
- Output: JSON repository + markdown docs

Follow the 5-phase workflow from the agent specification.
```

**That's it!** The agent will guide you through the process.

---

## 📋 What The Chunker Does

###  **Input**: Your codebase
- 789 files (MACCABI example)
- Multiple languages
- Complex dependencies
- Shared components

### **Process**: Intelligent chunking
1. **Analyze** - Understand code structure
2. **Chunk** - Break into optimal pieces
3. **Metadata** - Generate rich descriptions
4. **Relationships** - Map dependencies
5. **Optimize** - Prepare for RAG

### **Output**: RAG-optimized repository
- 1,200+ semantic chunks
- Complete metadata
- Relationship graph
- Vector DB ready
- Markdown docs

---

## 🎯 Three Ways to Use

### Method 1: **Full Automation** (Recommended)

**For**: Complete codebase chunking

**Command**:
```
@THE_CHUNKER_AGENT/agent_specification.md
@THE_CHUNKER_AGENT/chunking_strategies.yaml

Run complete chunking pipeline on MACCABI ICM codebase:

Phase 1 - Analyze:
- Scan: WD/*.txt (133 files) + ABAP/*.txt (656 files)
- Languages: ABAP, WebDynpro
- Output: CHUNKS/analysis.json

Phase 2 - Chunk:
- Strategy: adaptive (per chunking_strategies.yaml)
- ABAP: method_level
- WebDynpro: handler_level + cross_reference
- Check Component Controller (file 0008) for shared nodes
- Output: CHUNKS/repository.json

Phase 3 - Metadata:
- Generate summaries using LLM
- Extract tags, dependencies, relationships
- Calculate complexity scores
- Output: Enhanced repository.json

Phase 4 - Relationships:
- Build dependency graph
- Map call relationships
- Identify shared components
- Create navigation indexes
- Output: CHUNKS/graph.json

Phase 5 - Export:
- Format: Pinecone-compatible vectors
- Include embeddings
- Output: CHUNKS/vectors.jsonl

Report progress after each phase.
```

**Expected time**: 10-15 minutes for MACCABI codebase
**Output**: Complete chunk repository ready for RAG

---

### Method 2: **Interactive Chunking**

**For**: Step-by-step with user control

**Step 1: Analysis**
```
@THE_CHUNKER_AGENT/agent_specification.md

Analyze the ABAP directory:
- Path: ABAP/
- Count files by type
- Estimate total tokens
- Identify shared components
- Suggest chunking strategies

Wait for my approval before proceeding.
```

**Step 2: Chunk Generation** (after review)
```
Using the analysis above, generate chunks:
- Apply method_level strategy for classes
- Apply function_level for function modules
- Create cross_reference chunks for shared components
- Target 2K tokens per chunk
```

**Step 3: Validation** (after chunking)
```
Validate generated chunks:
- Check size distribution
- Verify metadata completeness
- Test relationship links
- Report any issues
```

---

### Method 3: **Targeted Chunking**

**For**: Specific files or components

**Example 1 - Chunk a specific class:**
```
@THE_CHUNKER_AGENT/chunking_strategies.yaml

Chunk the ZZ_CL_DOCTORS_CONTROLLING class:
- Files: ABAP/0001-0023 (method implementations)
- Strategy: method_level
- Include all 23 methods as separate chunks
- Add class-level chunk with overview
- Link methods to class
- Generate metadata for each

Output to: CHUNKS/class_zz_cl_doctors_controlling.json
```

**Example 2 - Chunk WebDynpro screens:**
```
@THE_CHUNKER_AGENT/agent_specification.md

Chunk screens 01-10:
- Path: WD/0*_*.txt
- Strategy: handler_level for implementations
- Strategy: node_definition_level for interfaces
- Check file 0008 for shared nodes
- Mark shared vs unique in metadata

Output to: CHUNKS/screens_01-10.json
```

---

## 🛠️ Using VS Code with AI Extensions

**If using VS Code with Claude extension or GitHub Copilot Chat:**

### Quick Activation
Use CIDRA snippets (if installed) or type directly:

```
@THE_CHUNKER_AGENT/agent_specification.md
Chunk my codebase using the 5-phase workflow.
```

### Slash Commands (if skills.yaml is loaded)
```
/chunk [path]           - Main chunking command
/chunk:analyze [path]   - Preview what will be chunked
/chunk:status           - Check progress
```

**Benefits**:
- Prompt-driven flexibility
- Context-aware chunking
- Interactive refinement
- No script dependencies

---

## 📊 Understanding the Output

### CHUNKS/repository.json

**Structure**:
```json
{
  "metadata": {
    "total_chunks": 1247,
    "total_tokens": 2500000,
    "languages": {"ABAP": 1100, "WebDynpro": 147}
  },
  "chunks": [
    {
      "chunk_id": "chunk_001",
      "type": "method_level",
      "file": "0001ZZ_CL_DOCTORS_CONTROLLING=====CM001.txt",
      "summary": "Validates Israeli patient ID",
      "tags": ["validation", "patient", "healthcare"],
      "tokens": 350,
      "dependencies": ["/ATL/L_CHECK_DIGIT"],
      "used_by": ["chunk_002", "chunk_011"],
      "complexity": "low"
    }
  ]
}
```

**Key fields**:
- `chunk_id`: Unique identifier for retrieval
- `summary`: Human-readable description
- `tags`: Semantic labels for search
- `dependencies`: What this needs
- `used_by`: What uses this
- `complexity`: Cognitive load estimate

---

### CHUNKS/graph.json

**Structure**:
```json
{
  "nodes": [
    {
      "id": "chunk_001",
      "label": "check_patient_id",
      "type": "method"
    }
  ],
  "edges": [
    {
      "from": "chunk_002",
      "to": "chunk_001",
      "type": "calls"
    }
  ]
}
```

**Use cases**:
- Visualize dependencies
- Impact analysis
- Find related code
- Navigate codebase

---

### CHUNKS/vectors.jsonl

**Structure** (Pinecone format):
```json
{"id": "chunk_001", "values": [0.1, 0.2, ...], "metadata": {}}
{"id": "chunk_002", "values": [0.3, 0.1, ...], "metadata": {}}
```

**Ready for**:
- Pinecone upload
- Chroma import
- Weaviate ingestion
- Custom vector DB

---

## 🔍 Querying Chunks

### In Cursor Chat

**After chunking, query the repository:**

```
@CHUNKS/repository.json

How does patient validation work in MACCABI ICM?
```

**Cursor will**:
1. Search chunks for "patient validation"
2. Retrieve relevant chunks (e.g., chunk_001, chunk_002)
3. Follow relationships to related code
4. Generate comprehensive answer with code citations

---

### Advanced Queries

**Find all database operations:**
```
@CHUNKS/repository.json

List all chunks that access the zctle_ccntrl_trn table.
For each, show:
- What it does
- Who calls it
- Related chunks
```

**Impact analysis:**
```
@CHUNKS/repository.json
@CHUNKS/graph.json

What would break if I change the check_patient_id method?
Show:
- Direct callers
- Indirect dependencies
- Affected screens
- Risk level
```

**Code navigation:**
```
@CHUNKS/repository.json

Find the code path from V_SELECT screen to patient data retrieval.
Show each chunk in the call chain.
```

---

## 🎯 Use Cases

### Use Case 1: **Documentation Generation**

**Scenario**: Generate API docs for backend classes

**Steps**:
1. Chunk all class methods
2. Extract method signatures + summaries
3. Group by class
4. Generate markdown documentation

**Command**:
```
@CHUNKS/repository.json

Generate API documentation for ZZ_CL_DOCTORS_CONTROLLING:
- Include all 23 methods
- Show parameters and return types
- Add usage examples from calling code
- Link to related methods

Format as markdown.
```

---

### Use Case 2: **Code Search**

**Scenario**: Find all authorization checks

**Command**:
```
@CHUNKS/repository.json

Find all chunks with tag "authorization".
For each:
- Show summary
- List related chunks
- Identify which screens use it
```

---

### Use Case 3: **Impact Analysis**

**Scenario**: Understand impact of changing shared component

**Command**:
```
@CHUNKS/repository.json
@CHUNKS/graph.json

Analyze impact of changing Component Controller (chunk_008):
- List all dependent screens
- Show affected context nodes
- Estimate refactoring effort
- Suggest test cases
```

---

### Use Case 4: **Learning Path**

**Scenario**: New developer needs to understand patient validation

**Command**:
```
@CHUNKS/repository.json

Create a learning path for understanding patient validation:
1. Start with overview chunks
2. Show validation methods in order
3. Explain dependencies
4. Provide examples from actual screens
5. Suggest related topics

Make it beginner-friendly.
```

---

## 📈 Quality Validation

### Check Chunking Quality

**After chunking, validate:**

```
@CHUNKS/repository.json

Validate chunk quality:
- Check size distribution (target: 2K-4K tokens)
- Verify metadata completeness
- Test relationship links
- Identify issues

Report:
- Total chunks
- Size distribution chart
- Metadata completeness %
- Broken links count
- Quality score (0-100)
```

**Expected output**:
```
Chunk Quality Report
====================
Total chunks: 1,247
Size distribution:
  < 500 tokens: 45 chunks (3.6%)
  500-2000: 412 chunks (33%)
  2000-4000: 687 chunks (55%)  ← Target range
  4000-8000: 98 chunks (7.8%)
  > 8000: 5 chunks (0.4%)

Metadata completeness: 98.5%
Broken links: 0
Quality score: 96/100

Issues:
- 5 chunks exceed 8K tokens (recommend splitting)
- 45 chunks below 500 tokens (consider merging)
```

---

### Optimize Chunks

**If issues found:**

```
@CHUNKS/repository.json

Optimize chunks:
- Split 5 large chunks (>8K tokens)
- Merge 45 small chunks (<500 tokens)
- Enrich metadata where incomplete
- Fix broken links

Re-validate after optimization.
```

---

## 🔧 Troubleshooting

### Issue 1: Chunking takes too long

**Solution**:
```
@THE_CHUNKER_AGENT/chunking_strategies.yaml

Reduce scope:
- Chunk only ABAP/ first (656 files)
- Then chunk WD/ separately (133 files)
- Merge results afterward

OR

Enable parallel processing:
- Set max_workers: 8 in config
- Process multiple files simultaneously
```

---

### Issue 2: Chunks too large/small

**Solution**:
```
@THE_CHUNKER_AGENT/chunking_strategies.yaml

Adjust thresholds:
- For too large: Reduce max_tokens_per_chunk to 3000
- For too small: Increase min_tokens_per_chunk to 800
- Re-run chunking with new settings
```

---

### Issue 3: Poor metadata quality

**Solution**:
```
@THE_CHUNKER_AGENT/agent_specification.md

Enhance metadata generation:
- Use better LLM (GPT-4 instead of GPT-3.5)
- Add more context to prompts
- Include code comments in analysis
- Increase tag count (5-7 instead of 3-5)
```

---

### Issue 4: Missing relationships

**Solution**:
```
@CHUNKS/repository.json

Re-analyze relationships:
- Scan for additional call patterns
- Check for implicit dependencies
- Verify cross-references
- Rebuild relationship graph

Focus on: [specific file or component]
```

---

## 🎓 Best Practices

### 1. **Start Small**
```
First run: Chunk 1 file/component
Validate: Check quality
Adjust: Tune strategies
Scale: Chunk entire codebase
```

### 2. **Use Adaptive Strategy**
```
Let the agent choose:
- File-level for small files
- Function-level for medium files
- Semantic grouping for large files
- Cross-reference for shared components
```

### 3. **Validate Often**
```
After each phase:
- Check output quality
- Verify metadata
- Test relationships
- Fix issues before proceeding
```

### 4. **Iterate and Refine**
```
First pass: Get 80% quality
Review: Identify gaps
Refine: Adjust strategies
Final pass: Achieve 95%+ quality
```

---

## 📊 Success Metrics

### For MACCABI ICM Codebase

**Expected Results**:
- **Total chunks**: ~1,200-1,500
- **Average chunk size**: 2,000-2,500 tokens
- **Metadata completeness**: 95%+
- **Relationship coverage**: 90%+
- **Processing time**: 10-15 minutes
- **Quality score**: 95+/100

**RAG Performance**:
- **Retrieval accuracy**: 90%+ relevant chunks
- **Query latency**: <500ms
- **Context sufficiency**: 95%+ of queries
- **Answer quality**: 85%+ rated "good" or "excellent"

---

## 🚀 Next Steps

### After Chunking

1. **Export to Vector DB**
   ```
   Ask Cursor:
   "Export CHUNKS/repository.json to Pinecone-compatible format (vectors.jsonl)"
   ```

2. **Set Up RAG System**
   - Upload vectors to your chosen vector DB (Pinecone, Chroma, etc.)
   - Configure retrieval parameters
   - Test queries
   - Optimize as needed

3. **Document Results**
   - Save chunking strategy used
   - Record quality metrics
   - Document any custom rules
   - Share with team

4. **Maintain Repository**
   - Re-chunk when code changes significantly
   - Update metadata as business logic evolves
   - Rebuild relationships periodically
   - Keep embeddings fresh

---

## 💡 Pro Tips

### Tip 1: **Use Tags Effectively**
```
Good tags: "validation", "database", "authorization"
Bad tags: "code", "method", "function" (too generic)

Generate tags that match:
- Business domains
- Technical functions
- Data entities
- User actions
```

### Tip 2: **Optimize for Search**
```
In metadata, include:
- Common search terms
- Business terminology
- Technical keywords
- Problem descriptions

Example: "patient ID validation check digit algorithm Israeli healthcare"
```

### Tip 3: **Link Related Chunks**
```
Strong relationships:
- Caller → Called
- Parent → Child
- Shared component → Users

Weak relationships:
- Similar functionality
- Same business process
- Related data entities
```

### Tip 4: **Maintain Context**
```
Each chunk should:
- Be understandable alone
- Have enough context
- Link to dependencies
- Reference source location
```

---

## 📞 Getting Help

### If Stuck

**Ask Cursor**:
```
@THE_CHUNKER_AGENT/agent_specification.md

I'm stuck at [phase/step]. Here's what happened:
[describe issue]

What should I do?
```

### Common Issues Solved

1. **"Chunking failed on file X"**
   - Check file encoding
   - Verify file syntax
   - Try different strategy
   - Skip and continue

2. **"Metadata incomplete"**
   - Re-run with better LLM
   - Add manual tags
   - Enrich with domain knowledge

3. **"Relationships missing"**
   - Check dependency patterns
   - Verify file paths
   - Rebuild graph

---

## ✅ Activation Checklist

**Before starting:**
- [ ] Create CHUNKS/ directory
- [ ] Install required Python packages (if using scripts)
- [ ] Configure API keys (for LLM/embeddings)
- [ ] Review chunking_strategies.yaml
- [ ] Understand your codebase structure

**During chunking:**
- [ ] Monitor progress
- [ ] Check intermediate outputs
- [ ] Validate quality
- [ ] Fix issues immediately
- [ ] Save configurations

**After chunking:**
- [ ] Validate final output
- [ ] Test RAG queries
- [ ] Document strategies used
- [ ] Share results
- [ ] Plan maintenance

---

## 🎯 Ready to Start?

**Simplest Command**:

```
@THE_CHUNKER_AGENT/agent_specification.md

Chunk my MACCABI ICM codebase using adaptive strategy.
Output to CHUNKS/ directory.
Report progress and quality metrics.
```

**The Chunker will handle the rest!** 🚀

---

*End of Cursor Integration Guide*
*The Chunker Agent v1.0.0*
