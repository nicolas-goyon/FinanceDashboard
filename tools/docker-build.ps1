# Build FinanceDashboard in Docker using the options in .env.
# See docs/docker-build.md.
#
#   ./tools/docker-build.ps1              # check requirements, build the image if needed, then the app
#   ./tools/docker-build.ps1 -NoImage     # skip the image step (faster when only code changed)
#   ./tools/docker-build.ps1 -SkipChecks  # don't run the requirement check (overrides CHECK_REQUIREMENTS)
#   ./tools/docker-build.ps1 -Force       # build even if the check fails (overrides FORCE_BUILD)
#
# Result: dist/<TARGET>-<LINK_MODE>-<build type>/
# Logs:   logs/docker-build/<yyyy-MM-dd_HH-mm-ss>_<TARGET>-<LINK_MODE>-<build type>/
#           summary.txt  options used, each step's result and duration
#           env.txt      copy of .env
#           requirements.log, image.log, container.log (everything the container printed),
#           configure.log, build.log, deploy.log (written by the container, one per step)
#
# Keep this file ASCII-only: Windows PowerShell 5.1 reads BOM-less scripts as ANSI.
param(
    [switch]$NoImage,
    [switch]$SkipChecks,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
# Docker prints UTF-8; PowerShell decodes native output with the console's
# code page (often 437/850), which garbles characters like box-drawing
# symbols. Read it as UTF-8 for this run, restore afterwards.
$prevOutputEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
Push-Location $root
try {
    if (-not (Test-Path '.env')) {
        Copy-Item '.env.example' '.env'
        Write-Host 'Created .env from .env.example - edit it to change target / link mode.'
    }

    # -- Options needed here (the rest is read by compose / the checker) ------
    $cfg = @{}
    foreach ($line in Get-Content '.env') {
        if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$') { $cfg[$Matches[1]] = $Matches[2].Trim('"').Trim("'") }
    }
    function Opt([string]$Key, [string]$Default) { if ($cfg[$Key]) { return $cfg[$Key] } else { return $Default } }
    $target = Opt 'TARGET' 'windows'
    $link = Opt 'LINK_MODE' 'shared'
    $buildType = Opt 'BUILD_TYPE' 'Release'
    # NB: PowerShell variable names are case-insensitive - $forceBuild, not
    # $force, or it would overwrite the -Force switch.
    $doCheck = (Opt 'CHECK_REQUIREMENTS' 'true') -notin @('false', '0', 'no', 'off')
    $forceBuild = (Opt 'FORCE_BUILD' 'false') -in @('true', '1', 'yes', 'on')
    if ($SkipChecks) { $doCheck = $false }
    if ($Force) { $forceBuild = $true }

    # -- Run log folder ---------------------------------------------------------
    $variant = "$target-$link-$($buildType.ToLower())"
    $runName = "$(Get-Date -Format 'yyyy-MM-dd_HH-mm-ss')_$variant"
    $runDir = Join-Path $root "logs\docker-build\$runName"
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    Copy-Item '.env' (Join-Path $runDir 'env.txt')
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $summary = Join-Path $runDir 'summary.txt'
    [IO.File]::WriteAllText($summary, "FinanceDashboard Docker build $runName`r`nTARGET=$target LINK_MODE=$link BUILD_TYPE=$buildType CHECK_REQUIREMENTS=$doCheck FORCE_BUILD=$forceBuild`r`n`r`n", $utf8)
    function Add-Summary([string]$Text) { [IO.File]::AppendAllText($summary, "$Text`r`n", $utf8) }
    Write-Host "Logs: logs\docker-build\$runName" -ForegroundColor Cyan

    # Run a native command, echo its output and append it (UTF-8) to a log file.
    # Native stderr must not become a terminating error under 'Stop'.
    function Invoke-Logged([string]$LogFile, [scriptblock]$Command) {
        $writer = New-Object System.IO.StreamWriter($LogFile, $true, $utf8)
        $prev = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & $Command 2>&1 | ForEach-Object {
                # Windows PowerShell wraps each native stderr line in an
                # ErrorRecord; an EMPTY stderr line then prints as the type
                # name "System.Management.Automation.RemoteException".
                $l = "$_"
                if ($_ -is [System.Management.Automation.ErrorRecord]) {
                    $l = $_.Exception.Message
                    if ($l -eq 'System.Management.Automation.RemoteException') { $l = '' }
                }
                $writer.WriteLine($l)
                Write-Host $l
            }
            return $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $prev
            $writer.Close()
        }
    }
    function Step([string]$Name, [scriptblock]$Body) {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $code = & $Body
        Add-Summary ("{0,-14} {1,-6} {2}" -f $Name, $(if ($code -eq 0) { 'OK' } else { "FAIL ($code)" }), $sw.Elapsed.ToString('hh\:mm\:ss'))
        return $code
    }

    # -- 1. Requirement check ---------------------------------------------------
    if ($doCheck) {
        $code = Step 'requirements' {
            & (Join-Path $PSScriptRoot 'check-requirements.ps1') -LogFile (Join-Path $runDir 'requirements.log')
            return $LASTEXITCODE
        }
        if ($code -ne 0) {
            if ($forceBuild) {
                Write-Host 'Requirement check failed - continuing anyway (FORCE_BUILD / -Force).' -ForegroundColor Yellow
                Add-Summary '               (continued: FORCE_BUILD)'
            } else {
                Add-Summary 'Stopped: requirement check failed. Fix the FAIL lines, or set FORCE_BUILD=true / use -Force.'
                throw 'Requirement check failed - see above (or logs). Set FORCE_BUILD=true in .env or pass -Force to build anyway.'
            }
        }
    } else {
        Add-Summary 'requirements   SKIPPED (CHECK_REQUIREMENTS=false / -SkipChecks)'
    }

    # -- 2. Toolchain image ---------------------------------------------------
    if (-not $NoImage) {
        $code = Step 'image' { Invoke-Logged (Join-Path $runDir 'image.log') { docker compose build builder } }
        if ($code -ne 0) { throw "docker compose build failed ($code) - see logs\docker-build\$runName\image.log" }
    }

    # -- 3. App build (configure / build / deploy inside the container) -------
    # RUN_LOG_DIR tells build-app.sh where to put its per-step logs (logs/ is
    # mounted at /work/logs). -T: no pseudo-TTY, so the output is plain lines.
    $env:RUN_LOG_DIR = "docker-build/$runName"
    $code = Step 'app' { Invoke-Logged (Join-Path $runDir 'container.log') { docker compose run --rm -T builder } }
    if ($code -ne 0) { throw "build failed ($code) - see logs\docker-build\$runName\" }
    Add-Summary "`r`nResult: dist\$variant\"
    Write-Host "Done: dist\$variant\  (logs: logs\docker-build\$runName)" -ForegroundColor Green
}
finally {
    Remove-Item Env:\RUN_LOG_DIR -ErrorAction SilentlyContinue
    [Console]::OutputEncoding = $prevOutputEncoding
    Pop-Location
}
