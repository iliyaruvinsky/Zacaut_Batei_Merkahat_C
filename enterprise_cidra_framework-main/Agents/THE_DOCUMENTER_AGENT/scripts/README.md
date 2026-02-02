# Documenter Agent Scripts

Validation and automation scripts for The Documenter Agent.

## Available Scripts

### Validate-Documentation.ps1

PowerShell script for validating documentation quality.

**Usage:**

```powershell
# Basic validation (MACCABI standard):
.\Validate-Documentation.ps1 -DocPath "WD\SCREENS\11_V_TREATMENT_HISTORY_SCREEN" -SourcePath "WD"

# With different configuration profile:
.\Validate-Documentation.ps1 -DocPath "documentation\MyProgram" -SourcePath "source" -ConfigProfile "enterprise_client"

# From project root:
.\THE_DOCUMENTER_AGENT\scripts\Validate-Documentation.ps1 -DocPath "WD\SCREENS\11_V_TREATMENT_HISTORY_SCREEN"
```

**Configuration Profiles:**

- `maccabi_icm` (default) - 100/100 required, 7 files
- `enterprise_client` - 95/100 required, 6 files
- `internal_team` - 90/100 required, 5 files
- `quick_notes` - 100/100 required, 3 files

**Exit Codes:**

- `0` - Validation passed (score meets threshold)
- `1` - Validation failed (score below threshold)

**What It Checks:**

1. ✓ File count (expected number of files)
2. ✓ Forbidden words (should be 0)
3. ✓ Careful language (≥5 instances expected)
4. ✓ Estimate language (should be 0)
5. ✓ Limitations section (required)
6. ✓ Cross-reference marking (shared/unique)
7. ✓ Validation report exists
8. ✓ README exists
9. ✓ Line count accuracy (informational)
10. ✓ Code artifacts file
11. ✓ File naming convention

**Example Output:**

```
======================================================================
DOCUMENTER AGENT - QUALITY VALIDATION
======================================================================

Configuration: MACCABI Standard
Target Score: 100/100
Documentation: WD\SCREENS\11_V_TREATMENT_HISTORY_SCREEN
Source: WD

[1/11] File Count Verification...
  ✓ PASS: 7 files found (expected 7)

[2/11] Forbidden Words Scan...
  ✓ PASS: No forbidden words found

[3/11] Careful Language Verification...
  ✓ PASS: 15 instances of careful language found

...

======================================================================
VALIDATION RESULTS
======================================================================

FINAL SCORE: 100/100

✓ READY FOR DELIVERY

Documentation meets all quality standards.
```

## Integration with VS Code

The validation script is integrated into VS Code tasks.

**Run from VS Code:**

1. Press `Ctrl+Shift+P`
2. Type "Tasks: Run Task"
3. Select "🤖 Documenter: Validate Quality (100/100)"
4. Enter screen path when prompted

## Integration with CI/CD

**Example GitLab CI:**

```yaml
validate_documentation:
  script:
    - pwsh THE_DOCUMENTER_AGENT/scripts/Validate-Documentation.ps1 -DocPath "documentation" -SourcePath "source"
  only:
    - merge_requests
```

**Example GitHub Actions:**

```yaml
- name: Validate Documentation
  run: |
    pwsh THE_DOCUMENTER_AGENT/scripts/Validate-Documentation.ps1 -DocPath "documentation" -SourcePath "source"
  shell: pwsh
```

## Future Scripts (Planned)

- `Generate-Documentation.ps1` - Automated documentation generation
- `Fix-Common-Issues.ps1` - Automated fixing of common violations
- `Generate-Quality-Report.ps1` - Generate HTML quality report
- `Validate-All-Screens.ps1` - Batch validation for all screens

---

*Version*: 1.0.0
*Based on*: MACCABI ICM Project (proven 100% accuracy)
