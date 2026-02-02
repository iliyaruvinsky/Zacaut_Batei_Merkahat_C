# CIDRA Framework - Add New Agent Script
# Usage: .\add-agent.ps1 -AgentName "THE_NEW_AGENT"
#
# This script scaffolds a new agent using templates

param(
    [Parameter(Mandatory=$true)]
    [string]$AgentName,
    [switch]$Help
)

$Green = "Green"
$Yellow = "Yellow"
$Cyan = "Cyan"
$Red = "Red"

$ScriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

function Show-Help {
    Write-Host ""
    Write-Host "CIDRA Framework - Add New Agent" -ForegroundColor $Cyan
    Write-Host ""
    Write-Host "Usage: .\add-agent.ps1 -AgentName <name>" -ForegroundColor $Green
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\add-agent.ps1 -AgentName 'THE_INTERPRETER_AGENT'"
    Write-Host "  .\add-agent.ps1 -AgentName 'THE_APPLICATOR_AGENT'"
    Write-Host "  .\add-agent.ps1 -AgentName 'TESTING'"
    Write-Host ""
    Write-Host "The script will normalize the name to THE_*_AGENT format."
    Write-Host ""
}

if ($Help) {
    Show-Help
    exit 0
}

# Normalize agent name
$AgentName = $AgentName.ToUpper()
if (-not $AgentName.StartsWith("THE_")) {
    $AgentName = "THE_" + $AgentName
}
if (-not $AgentName.EndsWith("_AGENT")) {
    $AgentName = $AgentName + "_AGENT"
}

Write-Host ""
Write-Host "Creating new agent: $AgentName" -ForegroundColor $Cyan
Write-Host ""

# Create agent directory
$agentDir = Join-Path $ScriptDir "Agents\$AgentName"
if (Test-Path $agentDir) {
    Write-Host "ERROR: Agent directory already exists: $agentDir" -ForegroundColor $Red
    exit 1
}

New-Item -ItemType Directory -Path $agentDir -Force | Out-Null
Write-Host "  Created: Agents\$AgentName\" -ForegroundColor $Green

# Copy templates
$specTemplate = Join-Path $ScriptDir "Agents\shared\templates\agent_specification_template.md"
$skillsTemplate = Join-Path $ScriptDir "Agents\shared\templates\skills_template.yaml"

$specTarget = Join-Path $agentDir "agent_specification.md"
$skillsTarget = Join-Path $agentDir "skills.yaml"

# Extract agent short name (e.g., THE_INTERPRETER_AGENT -> INTERPRETER)
$shortName = $AgentName -replace 'THE_|_AGENT', ''

# Copy and customize specification
if (Test-Path $specTemplate) {
    $specContent = Get-Content $specTemplate -Raw
    $specContent = $specContent -replace '\[AGENT_NAME\]', $shortName
    $specContent = $specContent -replace '\[agent_name\]', $shortName.ToLower()
    $specContent = $specContent -replace '\[YYYY-MM-DD\]', (Get-Date -Format "yyyy-MM-dd")
    Set-Content -Path $specTarget -Value $specContent
    Write-Host "  Created: agent_specification.md" -ForegroundColor $Green
} else {
    Write-Host "  WARNING: Specification template not found" -ForegroundColor $Yellow
}

# Copy and customize skills
if (Test-Path $skillsTemplate) {
    $skillsContent = Get-Content $skillsTemplate -Raw
    $skillsContent = $skillsContent -replace '\[AGENT_NAME\]', $shortName
    $agentPrefix = $shortName.Substring(0, [Math]::Min(3, $shortName.Length)).ToUpper()
    $skillsContent = $skillsContent -replace '\[AGENT_PREFIX\]', $agentPrefix
    $skillsContent = $skillsContent -replace '\[YYYY-MM\]', (Get-Date -Format "yyyy-MM")
    Set-Content -Path $skillsTarget -Value $skillsContent
    Write-Host "  Created: skills.yaml" -ForegroundColor $Green
} else {
    Write-Host "  WARNING: Skills template not found" -ForegroundColor $Yellow
}

# Update instructions
Write-Host ""
Write-Host "Next steps:" -ForegroundColor $Yellow
Write-Host "  1. Edit Agents\$AgentName\agent_specification.md"
Write-Host "  2. Edit Agents\$AgentName\skills.yaml"
Write-Host "  3. Add agent to Agents\registry.yaml"
Write-Host "  4. Update cidra.manifest.yaml"
Write-Host ""
Write-Host "Template created successfully!" -ForegroundColor $Green
