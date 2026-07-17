<#
.SYNOPSIS
    Runs every static analyzer set up for FinanceDashboard and logs each
    one's output to its own plain text file.

.DESCRIPTION
    Wraps the individual analyzer runs (currently: clang-tidy, qmllint -
    see ARCHITECTURE.MD "Static Analysis" section; clazy is not set up,
    skipped by design since it's unsupported on this project's MinGW
    toolchain) and tees each analyzer's full text output to logs/<analyzer>.log, while
    still printing to the console as it runs. Prints a short pass/fail
    summary at the end.

.PARAMETER BuildDir
    CMake build directory (needs an existing configure - compile_commands.json
    for clang-tidy, the FinanceDashboard_qmllint target for qmllint).
    Defaults to cmake-build-debug.

.PARAMETER LogDir
    Where to write the per-analyzer log files. Defaults to logs/ (gitignored).

.EXAMPLE
    ./tools/run-static-analysis.ps1

.EXAMPLE
    ./tools/run-static-analysis.ps1 -BuildDir cmake-build-debug -LogDir logs

.NOTES
    Not yet run end-to-end on the real dev machine - report back anything
    that breaks (see the notes in run-clang-tidy.ps1 about MinGW compile
    flags).
#>

param(
    [string]$BuildDir = "cmake-build-debug",
    [string]$LogDir = "logs"
)

# Deliberately NOT "Stop": clang-tidy and cmake both write normal
# informational output to stderr (e.g. clang-tidy's "N warnings generated."),
# and PowerShell promotes stderr lines from native commands into error
# records - under "Stop" that would abort the whole run after the first
# stderr line instead of finishing all analyzers. See run-clang-tidy.ps1 for
# the same note.
$ErrorActionPreference = "Continue"
$scriptDir = $PSScriptRoot
$projectRoot = Split-Path -Parent $scriptDir

# Run from the project root regardless of the caller's cwd, so relative
# paths ($BuildDir, $LogDir, compile_commands.json lookups inside
# run-clang-tidy.ps1) resolve consistently.
Push-Location $projectRoot
try {
    if (-not (Test-Path $LogDir)) {
        New-Item -ItemType Directory -Path $LogDir | Out-Null
    }
    $LogDir = (Resolve-Path $LogDir).Path

    $results = @()

    function Run-Analyzer {
        param(
            [string]$Name,
            [scriptblock]$Command,
            [string]$LogFile
        )

        Write-Host "`n==================== $Name ====================" -ForegroundColor Cyan
        Write-Host "Log: $LogFile" -ForegroundColor DarkGray

        # Run the command, tee its combined stdout+stderr to the console and
        # to the log file, and capture the wrapped command's own exit code
        # (not Tee-Object's, which is always 0).
        & $Command *>&1 | Tee-Object -FilePath $LogFile
        $exitCode = $LASTEXITCODE

        $hasIssues = (Select-String -Path $LogFile -Pattern 'warning:|error:' -Quiet -ErrorAction SilentlyContinue)

        $script:results += [PSCustomObject]@{
            Analyzer  = $Name
            ExitCode  = $exitCode
            HasIssues = [bool]$hasIssues
            LogFile   = $LogFile
        }
    }

    # -- clang-tidy ----------------------------------------------------------
    $clangTidyLog = Join-Path $LogDir "clang-tidy.log"
    $clangTidyScript = Join-Path $scriptDir "run-clang-tidy.ps1"
    Run-Analyzer -Name "clang-tidy" -LogFile $clangTidyLog -Command {
        & $clangTidyScript -BuildDir $BuildDir
    }

    # -- qmllint (already wired via qt_add_qml_module - see ARCHITECTURE.MD) -
    $qmllintLog = Join-Path $LogDir "qmllint.log"
    Run-Analyzer -Name "qmllint" -LogFile $qmllintLog -Command {
        cmake --build $BuildDir --target FinanceDashboard_qmllint
    }

    # -- clazy: intentionally not run - see ARCHITECTURE.MD "Static Analysis" section
    # (unsupported on this project's MinGW toolchain; not set up)

    # -- Summary ---------------------------------------------------------
    Write-Host "`n==================== Summary ====================" -ForegroundColor Cyan
    foreach ($r in $results) {
        $status = if ($r.ExitCode -ne 0 -or $r.HasIssues) { "ISSUES FOUND" } else { "clean" }
        $color = if ($status -eq "clean") { "Green" } else { "Yellow" }
        Write-Host ("{0,-12} {1,-14} {2}" -f $r.Analyzer, $status, $r.LogFile) -ForegroundColor $color
    }
    Write-Host ""
}
finally {
    Pop-Location
}
