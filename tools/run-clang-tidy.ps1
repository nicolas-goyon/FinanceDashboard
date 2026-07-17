<#
.SYNOPSIS
    On-demand clang-tidy run over FinanceDashboard's own sources.

.DESCRIPTION
    Reads compile_commands.json from the given build directory and runs
    clang-tidy on every entry whose file lives under src/ (skips AUTOMOC/
    AUTORCC/qml-registration generated files under the build directory,
    which are not useful to lint and are already silenced separately when
    ENABLE_CLANG_TIDY is used at build time - see CMakeLists.txt and
    ARCHITECTURE.MD "Static Analysis" section).

    Plain text output to the console - no HTML report. This script is
    independent of the ENABLE_CLANG_TIDY CMake option; use whichever fits
    your workflow (or both).

.PARAMETER BuildDir
    Path to the CMake build directory containing compile_commands.json.
    Defaults to cmake-build-debug (this project's default CLion build dir).

.PARAMETER Checks
    Optional clang-tidy -checks override. If omitted, clang-tidy uses the
    project's .clang-tidy file.

.EXAMPLE
    ./tools/run-clang-tidy.ps1

.EXAMPLE
    ./tools/run-clang-tidy.ps1 -BuildDir cmake-build-release

.NOTES
    Requires clang-tidy on PATH (install LLVM - see ARCHITECTURE.MD
    "Static Analysis" section for the Windows install steps). Not yet
    verified end-to-end against this project's actual MinGW
    compile_commands.json - if clang-tidy errors out parsing MinGW-specific
    flags, report it so the
    .clang-tidy / this script can be adjusted.
#>

param(
    [string]$BuildDir = "cmake-build-debug",
    [string]$Checks = ""
)

# Deliberately NOT setting $ErrorActionPreference = "Stop" here: clang-tidy
# writes normal informational output (e.g. "N warnings generated.") to
# stderr, and PowerShell promotes stderr lines from native commands into
# error records - under "Stop" that kills the script after the very first
# file instead of continuing through the rest. Each validation check below
# calls exit 1 itself right after Write-Error, so explicit Stop isn't
# needed for those to actually halt the script.
$ErrorActionPreference = "Continue"

$clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $clangTidy) {
    Write-Error "clang-tidy not found on PATH. Install LLVM (https://releases.llvm.org, or 'winget install LLVM.LLVM') and re-open your shell."
    exit 1
}

$compileCommandsPath = Join-Path $BuildDir "compile_commands.json"
if (-not (Test-Path $compileCommandsPath)) {
    Write-Error "No compile_commands.json found at '$compileCommandsPath'. Configure the project first (CMAKE_EXPORT_COMPILE_COMMANDS is on by default via CLion's CMake profile)."
    exit 1
}

$entries = Get-Content $compileCommandsPath -Raw | ConvertFrom-Json

# Only lint our own sources under src/. Excludes anything under the build
# directory itself (AUTOMOC moc_*.cpp, qrc_*.cpp, *_qmltyperegistrations.cpp,
# etc.) without needing to know their exact generated names.
# Project root = parent of this script's own folder (tools/), so this works
# regardless of the caller's current directory.
$projectRoot = Split-Path -Parent $PSScriptRoot
# CMake always writes forward-slash paths into compile_commands.json even on
# Windows, so normalize before comparing.
$srcRootForward = (Join-Path $projectRoot "src") -replace '\\', '/'

$files = $entries |
    Where-Object { ($_.file -replace '\\', '/') -like "$srcRootForward/*" } |
    Select-Object -ExpandProperty file -Unique

if (-not $files) {
    Write-Warning "No src/ files found in $compileCommandsPath - nothing to lint."
    exit 0
}

Write-Host "Running clang-tidy on $($files.Count) file(s) from $BuildDir ..." -ForegroundColor Cyan

$failed = $false
foreach ($file in $files) {
    Write-Host "`n=== $file ===" -ForegroundColor Yellow
    $argList = @("-p", $BuildDir)
    if ($Checks) { $argList += "-checks=$Checks" }
    $argList += $file

    & clang-tidy @argList
    if ($LASTEXITCODE -ne 0) { $failed = $true }
}

if ($failed) {
    Write-Host "`nclang-tidy reported issues (see above)." -ForegroundColor Red
} else {
    Write-Host "`nclang-tidy finished with no issues reported." -ForegroundColor Green
}
