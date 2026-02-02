<#
.SYNOPSIS
    Documentation Quality Validator for The Documenter Agent

.DESCRIPTION
    Validates documentation quality following MACCABI ICM standards.
    Target: 100/100 score (only perfect scores acceptable)

.PARAMETER DocPath
    Path to documentation directory (e.g., WD\SCREENS\11_V_TREATMENT_HISTORY_SCREEN)

.PARAMETER SourcePath
    Path to source code directory (e.g., WD)

.PARAMETER ConfigProfile
    Configuration profile: maccabi_icm, enterprise_client, internal_team, quick_notes
    Default: maccabi_icm

.EXAMPLE
    .\Validate-Documentation.ps1 -DocPath "WD\SCREENS\11_V_TREATMENT_HISTORY_SCREEN" -SourcePath "WD"

.EXAMPLE
    .\Validate-Documentation.ps1 -DocPath "documentation\MyProgram" -SourcePath "source" -ConfigProfile "enterprise_client"

.NOTES
    Version: 1.0.0
    Based on: MACCABI ICM SAP WebDynpro ABAP Project (proven 100% accuracy)
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$DocPath,

    [Parameter(Mandatory=$false)]
    [string]$SourcePath = "WD",

    [Parameter(Mandatory=$false)]
    [ValidateSet("maccabi_icm", "enterprise_client", "internal_team", "quick_notes")]
    [string]$ConfigProfile = "maccabi_icm"
)

# ============================================================================
# CONFIGURATION
# ============================================================================

$config = @{
    maccabi_icm = @{
        Name = "MACCABI Standard"
        ExpectedFileCount = 7
        PassingScore = 100
        AllowPartialPass = $false
        Deductions = @{
            ForbiddenWord = 5
            MissingCarefulLanguage = 10
            EstimateLanguage = 15
            MissingLimitations = 20
            MissingCrossReference = 25
            WrongFileCount = 30
            InaccurateLineCount = 40
        }
    }
    enterprise_client = @{
        Name = "Enterprise Client"
        ExpectedFileCount = 6
        PassingScore = 95
        AllowPartialPass = $true
        Deductions = @{
            ForbiddenWord = 3
            MissingCarefulLanguage = 5
            EstimateLanguage = 10
            MissingLimitations = 15
            MissingCrossReference = 15
            WrongFileCount = 20
            InaccurateLineCount = 30
        }
    }
    internal_team = @{
        Name = "Internal Team"
        ExpectedFileCount = 5
        PassingScore = 90
        AllowPartialPass = $true
        Deductions = @{
            ForbiddenWord = 2
            MissingCarefulLanguage = 5
            EstimateLanguage = 5
            MissingLimitations = 10
            MissingCrossReference = 5
            WrongFileCount = 15
            InaccurateLineCount = 20
        }
    }
    quick_notes = @{
        Name = "Quick Reference"
        ExpectedFileCount = 3
        PassingScore = 100
        AllowPartialPass = $false
        Deductions = @{
            ForbiddenWord = 5
            MissingCarefulLanguage = 0
            EstimateLanguage = 20
            MissingLimitations = 15
            MissingCrossReference = 0
            WrongFileCount = 30
            InaccurateLineCount = 50
        }
    }
}

$selectedConfig = $config[$ConfigProfile]

# ============================================================================
# VALIDATION FUNCTIONS
# ============================================================================

function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host "======================================================================" -ForegroundColor Cyan
    Write-Host $Text -ForegroundColor Cyan
    Write-Host "======================================================================" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Check {
    param(
        [string]$CheckName,
        [int]$CheckNumber,
        [int]$TotalChecks
    )
    Write-Host "[$CheckNumber/$TotalChecks] $CheckName..." -ForegroundColor Green
}

function Write-Pass {
    param([string]$Message)
    Write-Host "  ✓ PASS: $Message" -ForegroundColor White
}

function Write-Fail {
    param([string]$Message)
    Write-Host "  ✗ FAIL: $Message" -ForegroundColor Red
}

function Write-Warning {
    param([string]$Message)
    Write-Host "  ⚠ WARNING: $Message" -ForegroundColor Yellow
}

# ============================================================================
# MAIN VALIDATION
# ============================================================================

Write-Header "DOCUMENTER AGENT - QUALITY VALIDATION"

Write-Host "Configuration: $($selectedConfig.Name)" -ForegroundColor Yellow
Write-Host "Target Score: $($selectedConfig.PassingScore)/100" -ForegroundColor Yellow
Write-Host "Documentation: $DocPath" -ForegroundColor Gray
Write-Host "Source: $SourcePath" -ForegroundColor Gray
Write-Host ""

# Initialize score
$score = 100
$totalChecks = 11

# Check if documentation directory exists
if (-not (Test-Path $DocPath)) {
    Write-Host "✗ ERROR: Documentation directory not found: $DocPath" -ForegroundColor Red
    exit 1
}

# ============================================================================
# CHECK 1: File Count
# ============================================================================

Write-Check "File Count Verification" 1 $totalChecks

$mdFiles = Get-ChildItem "$DocPath\*.md" -ErrorAction SilentlyContinue
$fileCount = ($mdFiles | Measure-Object).Count
$expectedCount = $selectedConfig.ExpectedFileCount

if ($fileCount -eq $expectedCount) {
    Write-Pass "$fileCount files found (expected $expectedCount)"
} else {
    Write-Fail "$fileCount files found (expected $expectedCount)"
    $score -= $selectedConfig.Deductions.WrongFileCount

    if ($mdFiles) {
        Write-Host "  Files found:" -ForegroundColor Gray
        $mdFiles | ForEach-Object { Write-Host "    - $($_.Name)" -ForegroundColor Gray }
    }
}

Write-Host ""

# ============================================================================
# CHECK 2: Forbidden Words
# ============================================================================

Write-Check "Forbidden Words Scan" 2 $totalChecks

$forbiddenPattern = 'מתקדם|חכם|אינטליגנטי|מתוחכם|KPI|advanced|smart|intelligent|sophisticated|state-of-the-art|cutting-edge'
$forbidden = Select-String -Path "$DocPath\*.md" -Pattern $forbiddenPattern -AllMatches -ErrorAction SilentlyContinue

if (!$forbidden) {
    Write-Pass "No forbidden words found"
} else {
    $forbiddenCount = ($forbidden | Measure-Object).Count
    Write-Fail "$forbiddenCount forbidden word occurrences found"
    $score -= ($forbiddenCount * $selectedConfig.Deductions.ForbiddenWord)

    Write-Host "  Examples:" -ForegroundColor Gray
    $forbidden | Select-Object -First 5 | ForEach-Object {
        Write-Host "    - $($_.Filename):$($_.LineNumber) - $($_.Line.Trim())" -ForegroundColor Gray
    }
}

Write-Host ""

# ============================================================================
# CHECK 3: Careful Language
# ============================================================================

Write-Check "Careful Language Verification" 3 $totalChecks

$carefulPattern = 'נראה ש|נראה כי|לפי הקוד|מבוסס על|appears to|seems to|according to|based on'
$careful = Select-String -Path "$DocPath\*.md" -Pattern $carefulPattern -AllMatches -ErrorAction SilentlyContinue
$carefulCount = ($careful | Measure-Object).Count

if ($carefulCount -ge 5) {
    Write-Pass "$carefulCount instances of careful language found"
} elseif ($selectedConfig.Deductions.MissingCarefulLanguage -eq 0) {
    Write-Warning "Only $carefulCount instances found (not required for $($selectedConfig.Name))"
} else {
    Write-Fail "Only $carefulCount instances found (expected ≥5)"
    $score -= $selectedConfig.Deductions.MissingCarefulLanguage
}

Write-Host ""

# ============================================================================
# CHECK 4: Estimate Language
# ============================================================================

Write-Check "Estimate Language Detection" 4 $totalChecks

$estimatePattern = 'בערך|בקירוב|approximately|around|roughly|about\s+\d|~\d|±|several|many\s+\w+|some\s+\w+'
$estimates = Select-String -Path "$DocPath\*.md" -Pattern $estimatePattern -AllMatches -ErrorAction SilentlyContinue

if (!$estimates) {
    Write-Pass "No estimate language found"
} else {
    $estimateCount = ($estimates | Measure-Object).Count
    Write-Fail "$estimateCount estimate language occurrences found"
    $score -= ($estimateCount * $selectedConfig.Deductions.EstimateLanguage)

    Write-Host "  Examples:" -ForegroundColor Gray
    $estimates | Select-Object -First 5 | ForEach-Object {
        Write-Host "    - $($_.Filename):$($_.LineNumber) - $($_.Line.Trim())" -ForegroundColor Gray
    }
}

Write-Host ""

# ============================================================================
# CHECK 5: Limitations Section
# ============================================================================

Write-Check "Limitations Section Presence" 5 $totalChecks

$specFile = Get-ChildItem "$DocPath\01_*.md" -ErrorAction SilentlyContinue | Select-Object -First 1

if ($specFile) {
    $limitationsPattern = 'מגבלות תיעוד|Documentation Limitations'
    $hasLimitations = Select-String -Path $specFile.FullName -Pattern $limitationsPattern -Quiet

    if ($hasLimitations) {
        # Check for both subsections
        $content = Get-Content $specFile.FullName -Raw
        $hasCannotSection = $content -match 'מה שלא|What CANNOT'
        $hasCanSection = $content -match 'מה שכן|What IS'

        if ($hasCannotSection -and $hasCanSection) {
            Write-Pass "Limitations section found with required subsections"
        } else {
            Write-Fail "Limitations section incomplete (missing subsections)"
            $score -= $selectedConfig.Deductions.MissingLimitations
        }
    } else {
        Write-Fail "Limitations section not found in $($specFile.Name)"
        $score -= $selectedConfig.Deductions.MissingLimitations
    }
} else {
    Write-Fail "01_SPECIFICATION.md file not found"
    $score -= $selectedConfig.Deductions.MissingLimitations
}

Write-Host ""

# ============================================================================
# CHECK 6: Cross-Reference Marking
# ============================================================================

Write-Check "Cross-Reference Marking Verification" 6 $totalChecks

if ($specFile) {
    $crossRefPattern = 'משותף מ|shared from|unique to|ייחודי ל'
    $crossRef = Select-String -Path $specFile.FullName -Pattern $crossRefPattern -AllMatches -ErrorAction SilentlyContinue

    if ($crossRef) {
        $crossRefCount = ($crossRef | Measure-Object).Count
        Write-Pass "$crossRefCount shared/unique elements marked"
    } else {
        if ($selectedConfig.Deductions.MissingCrossReference -eq 0) {
            Write-Warning "No cross-reference marking found (not required for $($selectedConfig.Name))"
        } else {
            Write-Warning "No shared/unique elements marked (acceptable if none exist)"
            Write-Host "  Note: Deduction only if cross-references should exist" -ForegroundColor Gray
        }
    }
} else {
    Write-Warning "Cannot check cross-references without specification file"
}

Write-Host ""

# ============================================================================
# CHECK 7: Validation Report
# ============================================================================

Write-Check "Validation Report Presence" 7 $totalChecks

$validationReport = Get-ChildItem "$DocPath\*אימות*.md","$DocPath\*validation*.md","$DocPath\*VALIDATION*.md" -ErrorAction SilentlyContinue | Select-Object -First 1

if ($validationReport) {
    Write-Pass "Validation report found ($($validationReport.Name))"
} else {
    Write-Fail "Validation report not found"
    $score -= 15
}

Write-Host ""

# ============================================================================
# CHECK 8: README Exists
# ============================================================================

Write-Check "README Existence" 8 $totalChecks

$readme = Get-ChildItem "$DocPath\README.md" -ErrorAction SilentlyContinue

if ($readme) {
    Write-Pass "README.md found"
} else {
    Write-Fail "README.md not found"
    $score -= 10
}

Write-Host ""

# ============================================================================
# CHECK 9: Line Count Accuracy (if applicable)
# ============================================================================

Write-Check "Line Count Accuracy" 9 $totalChecks

if ($specFile) {
    # Try to extract documented line count
    $content = Get-Content $specFile.FullName -Raw

    if ($content -match '(\d+,?\d*)\s+שורות|(\d+,?\d*)\s+lines') {
        $documentedCount = $matches[1] -replace ',', ''
        Write-Host "  Documented count found: $documentedCount" -ForegroundColor Gray
        Write-Host "  (Manual verification recommended against actual source)" -ForegroundColor Gray
    } else {
        Write-Host "  No line count found in documentation" -ForegroundColor Gray
    }
} else {
    Write-Host "  Cannot check without specification file" -ForegroundColor Gray
}

Write-Host ""

# ============================================================================
# CHECK 10: Code Snippet Presence (if applicable)
# ============================================================================

Write-Check "Code Artifacts File" 10 $totalChecks

$codeArtifacts = Get-ChildItem "$DocPath\05_*.md","$DocPath\*CODE*.md" -ErrorAction SilentlyContinue | Select-Object -First 1

if ($codeArtifacts) {
    Write-Pass "Code artifacts file found ($($codeArtifacts.Name))"
} else {
    if ($selectedConfig.ExpectedFileCount -le 3) {
        Write-Host "  Not required for $($selectedConfig.Name)" -ForegroundColor Gray
    } else {
        Write-Warning "Code artifacts file not found"
    }
}

Write-Host ""

# ============================================================================
# CHECK 11: File Naming Convention
# ============================================================================

Write-Check "File Naming Convention" 11 $totalChecks

$expectedPatterns = @(
    '01_.*SPECIFICATION\.md'
    '02_.*\.md'
    '03_.*\.md'
    '04_.*\.md'
    '05_.*\.md'
    'README\.md'
)

$namingIssues = 0

if ($mdFiles) {
    $fileNames = $mdFiles | ForEach-Object { $_.Name }

    # Check if files follow numbering convention
    $numberedFiles = $fileNames | Where-Object { $_ -match '^\d{2}_' }

    if ($numberedFiles) {
        Write-Pass "Files follow numbering convention"
    } else {
        Write-Warning "Files don't follow 01_, 02_, etc. numbering"
    }
} else {
    Write-Host "  No files to check" -ForegroundColor Gray
}

Write-Host ""

# ============================================================================
# FINAL SCORE
# ============================================================================

Write-Header "VALIDATION RESULTS"

Write-Host "Configuration: $($selectedConfig.Name)" -ForegroundColor White
Write-Host "Expected Files: $expectedCount" -ForegroundColor White
Write-Host "Actual Files: $fileCount" -ForegroundColor White
Write-Host ""

if ($score -eq 100) {
    Write-Host "FINAL SCORE: 100/100" -ForegroundColor Green -BackgroundColor Black
    Write-Host ""
    Write-Host "✓ READY FOR DELIVERY" -ForegroundColor Green
    Write-Host ""
    Write-Host "Documentation meets all quality standards." -ForegroundColor White
    $exitCode = 0
} elseif ($score -ge $selectedConfig.PassingScore) {
    Write-Host "FINAL SCORE: $score/100" -ForegroundColor Yellow -BackgroundColor Black
    Write-Host ""

    if ($selectedConfig.AllowPartialPass) {
        Write-Host "✓ ACCEPTABLE (≥$($selectedConfig.PassingScore) required for $($selectedConfig.Name))" -ForegroundColor Yellow
        $exitCode = 0
    } else {
        Write-Host "✗ FIXES REQUIRED (100/100 required for $($selectedConfig.Name))" -ForegroundColor Yellow
        $exitCode = 1
    }

    Write-Host ""
    Write-Host "Minor issues found. Review validation output above." -ForegroundColor White
} elseif ($score -ge 85) {
    Write-Host "FINAL SCORE: $score/100" -ForegroundColor Magenta -BackgroundColor Black
    Write-Host ""
    Write-Host "⚠ MODERATE FIXES REQUIRED" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "Several issues need attention. Review validation output above." -ForegroundColor White
    $exitCode = 1
} else {
    Write-Host "FINAL SCORE: $score/100" -ForegroundColor Red -BackgroundColor Black
    Write-Host ""
    Write-Host "✗ MAJOR REWORK NEEDED" -ForegroundColor Red
    Write-Host ""
    Write-Host "Significant issues found. Review validation output and fix before delivery." -ForegroundColor White
    $exitCode = 1
}

Write-Host ""
Write-Header "END OF VALIDATION"

# Return appropriate exit code
exit $exitCode
