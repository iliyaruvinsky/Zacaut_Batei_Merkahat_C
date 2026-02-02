# CIDRA Protocols - IDE Configuration Templates

**Purpose**: Ready-to-use configurations for popular IDEs  
**Included**: Cursor, Claude Code, VS Code, Windsurf

---

## 📁 What's Included

This directory contains protocol templates for 4 popular IDEs:

| IDE | Directory | What's Included |
|-----|-----------|----------------|
| **Cursor** | `.cursorrules` | Universal behavior rules |
| **Claude Code** | `.claude/` | Rules + settings |
| **VS Code + Copilot** | `.vscode/` | Instructions + settings + tasks + snippets |
| **Windsurf** | `.windsurf/` | Rules for AI assistant |
| **Optional** | `.snapshots/` | Snapshot configuration |

---

## 🚀 Quick Setup

### **Choose Your IDE and Copy:**

#### **For Cursor:**
```bash
# Copy to your project root
cp Protocols/.cursorrules .
```

#### **For Claude Code:**
```bash
# Copy to your project root
cp -r Protocols/.claude .
```

#### **For VS Code + GitHub Copilot:**
```bash
# Copy to your project root
cp -r Protocols/.vscode .

# Restart VS Code
```

#### **For Windsurf:**
```bash
# Copy to your project root
cp -r Protocols/.windsurf .
```

#### **Optional - Snapshots:**
```bash
# Only if your IDE uses snapshots feature
cp -r Protocols/.snapshots .
```

---

## 📖 What Each Protocol Does

### **1. .cursorrules**

**Size**: ~150 lines  
**Contains**:
- Mandatory verification rules
- Anti-hallucination guidelines
- CIDRA agent invocation patterns
- Universal documentation principles

**Works with**: Cursor IDE

---

### **2. .claude/**

**Files**: 
- `rules/anti-hallucination-core.md`
- `rules/documentation-standards.md`
- `settings.local.json`
- `README.md`

**Contains**:
- Core anti-hallucination rules
- 7-file documentation standard
- Permission settings
- Setup instructions

**Works with**: Claude Code

---

### **3. .vscode/**

**Files**:
- `copilot-instructions.md` - GitHub Copilot instructions
- `settings.json` - VS Code settings
- `tasks.json` - Automated tasks
- `documenter.code-snippets` - Documentation snippets
- `README.md` - Setup guide

**Contains**:
- Copilot-specific rules
- Optimized editor settings
- 6 automation tasks
- 7 documentation snippets

**Works with**: VS Code + GitHub Copilot

**Tasks available** (Ctrl+Shift+P → "Tasks: Run Task"):
- 📊 Scan Source Code Files
- 🔍 Find Component Files
- 📏 Count File Lines Exactly
- ✅ Validate Component Documentation
- 🔍 Check for Hallucinations
- 📊 Generate Project Statistics

---

### **4. .windsurf/**

**Files**:
- `rules/anti-hallucination-core.md`
- `rules/documentation-standards.md`
- `README.md`

**Contains**:
- Same core rules as Claude
- 7-file documentation standard
- Setup instructions

**Works with**: Windsurf IDE

---

### **5. .snapshots/** (Optional)

**Files**:
- `config.json` - Snapshot configuration
- `README.md` - Usage guide

**Contains**:
- Excluded file patterns
- Included file patterns
- Default behavior

**Works with**: Cursor, Windsurf (IDE feature)

---

## 🎯 Which IDE Should You Use?

### **Recommended for CIDRA:**

| IDE | Best For | Pros |
|-----|----------|------|
| **Cursor** | Quick setup | Simple .cursorrules file, fast |
| **Claude Code** | Advanced control | Granular permissions, rule organization |
| **VS Code + Copilot** | Teams | Familiar IDE, tasks automation, snippets |
| **Windsurf** | Alternative | Similar to Cursor |

**All work equally well with CIDRA!**

---

## 📋 Setup Checklist

After copying protocols:

- [ ] Copied protocol for your IDE
- [ ] Restarted IDE to load configuration
- [ ] Verified rules are active (ask AI to confirm)
- [ ] Tested with simple documentation task
- [ ] Checked validation report shows 100/100

---

## 🔧 Customization

### **Add Project-Specific Rules:**

Each IDE template can be customized:

**Cursor**: Edit `.cursorrules` - add rules at bottom  
**Claude**: Create `.claude/rules/project-specific.md`  
**VS Code**: Edit `copilot-instructions.md` - add section  
**Windsurf**: Create `.windsurf/rules/project-specific.md`

### **Keep Universal Rules:**

Don't modify the core CIDRA rules - they ensure quality across all projects.

---

## 🆘 Troubleshooting

### **Rules Not Working?**

1. Verify files copied correctly
2. Restart your IDE
3. Check IDE version (some features require updates)
4. Try asking AI: "Are you following CIDRA rules?"

### **Want to Update Protocols?**

```bash
# Get latest from CIDRA
cd enterprise_cidra_framework
git pull

# Copy updated protocols to your project
cp -r Protocols/[YOUR_IDE] /your/project/
```

---

## 📚 More Information

- **CIDRA Quick Start**: `../QUICK_START.md`
- **Full User Guide**: `../Documentation/USER_GUIDE.md`
- **Architecture**: `../Documentation/ARCHITECTURE.md`

---

**Choose your IDE, copy the protocol, and start documenting with CIDRA!**

