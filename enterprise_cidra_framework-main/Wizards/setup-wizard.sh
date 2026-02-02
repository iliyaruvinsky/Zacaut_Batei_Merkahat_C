#!/bin/bash
# CIDRA Framework Setup Wizard
# Version: 1.0.0

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║      CIDRA Framework Configuration Wizard               ║"
echo "║  Chunker + Interpreter + Documenter + Recommender + AI  ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Check if update mode
UPDATE_MODE=false
if [ "$1" == "--update" ]; then
    UPDATE_MODE=true
    echo "🔄 Update mode - will preserve existing settings"
    echo ""
fi

# Step 1: Technology Selection
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "1️⃣  Select Technology"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "  a) SAP (WebDynpro, ABAP, FI/CO, HR, BW)"
echo "  b) AS/400 (RPG, COBOL, CL, DDS)"
echo "  c) React/JavaScript (React, Node.js, TypeScript)"
echo "  d) Python (Django, FastAPI, Flask)"
echo "  e) Custom (I'll configure manually)"
echo ""
read -p "Choose technology [a/b/c/d/e]: " tech_choice

case $tech_choice in
    a|A) TECHNOLOGY="SAP"; PLUGIN="sap_plugin.yaml";;
    b|B) TECHNOLOGY="AS400"; PLUGIN="as400_plugin.yaml";;
    c|C) TECHNOLOGY="React"; PLUGIN="react_plugin.yaml";;
    d|D) TECHNOLOGY="Python"; PLUGIN="python_plugin.yaml";;
    e|E) TECHNOLOGY="Custom"; PLUGIN="custom_plugin.yaml";;
    *) TECHNOLOGY="SAP"; PLUGIN="sap_plugin.yaml";;
esac

echo "✅ Selected: $TECHNOLOGY"
echo ""

# Step 2: Project Name
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "2️⃣  Project Name"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
read -p "Enter project name (e.g., FINANCE_SYSTEM): " project_name

if [ -z "$project_name" ]; then
    project_name="MY_PROJECT"
fi

echo "✅ Project name: $project_name"
echo ""

# Step 3: Code Location
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "3️⃣  Source Code Location"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
read -p "Enter source code directory path: " code_location

if [ -z "$code_location" ]; then
    code_location="./src"
fi

echo "✅ Code location: $code_location"
echo ""

# Step 4: Documentation Language
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "4️⃣  Documentation Language"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "  a) Hebrew (עברית)"
echo "  b) English"
echo ""
read -p "Choose language [a/b]: " lang_choice

case $lang_choice in
    a|A) LANGUAGE="he";;
    b|B) LANGUAGE="en";;
    *) LANGUAGE="he";;
esac

echo "✅ Language: $LANGUAGE"
echo ""

# Step 5: Advanced Settings
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "5️⃣  Advanced Settings (Optional)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
read -p "Enable strict validation mode? [y/N]: " strict_mode

if [ "$strict_mode" == "y" ] || [ "$strict_mode" == "Y" ]; then
    STRICT_MODE=true
else
    STRICT_MODE=false
fi

read -p "Minimum quality score (0-100) [100]: " min_score
if [ -z "$min_score" ]; then
    min_score=100
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📝 Generating Configuration..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Generate timestamp
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# Create .cidra-config.json
cat > .cidra-config.json <<EOF
{
  "project_name": "$project_name",
  "technology": "$TECHNOLOGY",
  "plugin": "Plugins/$PLUGIN",
  "code_location": "$code_location",
  "documentation_language": "$LANGUAGE",
  "created_at": "$TIMESTAMP",
  "updated_at": "$TIMESTAMP",
  
  "agents": {
    "the_chunker": {
      "enabled": true,
      "strategy": "adaptive",
      "chunk_size": "2K-4K tokens",
      "optimization": "llm-friendly"
    },
    "the_documenter": {
      "enabled": true,
      "anti_hallucination": true,
      "file_count": 7,
      "citation_required": true,
      "exact_counting": true
    },
    "the_recommender": {
      "enabled": true,
      "roi_analysis": true,
      "risk_assessment": true,
      "multi_criteria": true
    }
  },
  
  "validation": {
    "strict_mode": $STRICT_MODE,
    "min_quality_score": $min_score,
    "required_sections": "all",
    "code_verification": true
  },
  
  "output": {
    "directory": "./documentation",
    "format": "markdown",
    "include_validation_reports": true
  }
}
EOF

echo "✅ Configuration file created: .cidra-config.json"
echo ""

# Create necessary directories
mkdir -p documentation
mkdir -p chunks
mkdir -p validation

echo "✅ Created directories: documentation/, chunks/, validation/"
echo ""

# Copy IDE templates if requested
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "6️⃣  IDE Integration (Optional)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
read -p "Install VS Code templates? [y/N]: " install_vscode

if [ "$install_vscode" == "y" ] || [ "$install_vscode" == "Y" ]; then
    CIDRA_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
    cp -r "$CIDRA_DIR/Protocols/.vscode" ./.vscode
    echo "✅ VS Code templates installed in .vscode/"
fi

read -p "Install Cursor rules? [y/N]: " install_cursor

if [ "$install_cursor" == "y" ] || [ "$install_cursor" == "Y" ]; then
    CIDRA_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
    cat "$CIDRA_DIR/Protocols/.cursorrules" >> .cursorrules
    echo "✅ Cursor rules appended to .cursorrules"
fi

read -p "Install Claude Code rules? [y/N]: " install_claude

if [ "$install_claude" == "y" ] || [ "$install_claude" == "Y" ]; then
    CIDRA_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
    cp -r "$CIDRA_DIR/Protocols/.claude" ./.claude
    echo "✅ Claude Code rules installed in .claude/"
fi

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║              ✅ CIDRA Setup Complete!                    ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "📋 Configuration Summary:"
echo "  • Project: $project_name"
echo "  • Technology: $TECHNOLOGY"
echo "  • Plugin: $PLUGIN"
echo "  • Language: $LANGUAGE"
echo "  • Strict Mode: $STRICT_MODE"
echo ""
echo "🚀 Next Steps:"
echo "  1. Review .cidra-config.json"
echo "  2. Start chunking: @THE_CHUNKER_AGENT analyze $project_name"
echo "  3. Start documenting: @THE_DOCUMENTER_AGENT document [COMPONENT]"
echo ""
echo "📚 Documentation: See Documentation/USER_GUIDE.md"
echo ""

