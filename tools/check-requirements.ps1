# Check, BEFORE building, that everything the Docker build needs is in place
# and whether THIS computer will be able to run the result - based on the
# options in .env (TARGET, LINK_MODE, QT_VERSION, LLVM_MINGW_VERSION, ...).
# See docs/docker-build.md, section "Requirement check".
#
#   ./tools/check-requirements.ps1                 # report only
#   ./tools/check-requirements.ps1 -LogFile x.log  # also write the report to a file
#   ./tools/check-requirements.ps1 -Offline        # skip the download/version checks
#
# Exit code: 0 = no failure (warnings allowed), 1 = at least one FAIL.
# tools/docker-build.ps1 runs this automatically (CHECK_REQUIREMENTS / FORCE_BUILD in .env).
#
# Written for Windows PowerShell 5.1 (the one shipped with Windows) and PowerShell 7.
# Keep this file ASCII-only: PowerShell 5.1 reads BOM-less scripts as ANSI.
param(
    [string]$LogFile = '',
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest's progress bar is very slow on 5.1
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$root = Split-Path -Parent $PSScriptRoot
# $IsWindows only exists in PowerShell 6+; Windows PowerShell 5.1 ("Desktop") is always Windows.
$onWindows = ($PSVersionTable.PSEdition -eq 'Desktop') -or $IsWindows

# -- Options (.env, falling back to .env.example, then built-in defaults) -----
function Read-DotEnv([string]$Path) {
    $h = @{}
    if (Test-Path $Path) {
        foreach ($line in Get-Content $Path) {
            if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$') {
                $h[$Matches[1]] = $Matches[2].Trim('"').Trim("'")
            }
        }
    }
    return $h
}
$defaults = @{
    TARGET = 'windows'; LINK_MODE = 'shared'; BUILD_TYPE = 'Release'
    QT_VERSION = '6.11.1'; LLVM_MINGW_VERSION = '20231128'
}
$envFile = Join-Path $root '.env'
if (-not (Test-Path $envFile)) { $envFile = Join-Path $root '.env.example' }
$cfg = Read-DotEnv $envFile
foreach ($k in $defaults.Keys) { if (-not $cfg[$k]) { $cfg[$k] = $defaults[$k] } }
$TARGET = $cfg.TARGET; $LINK = $cfg.LINK_MODE; $QTV = $cfg.QT_VERSION; $LLVMV = $cfg.LLVM_MINGW_VERSION

# -- Reporting ----------------------------------------------------------------
$script:results = New-Object System.Collections.ArrayList
$script:logLines = New-Object System.Collections.ArrayList
function Out-Line([string]$Text, [string]$Color = 'Gray') {
    Write-Host $Text -ForegroundColor $Color
    [void]$script:logLines.Add($Text)
}
function Section([string]$Title) { Out-Line ''; Out-Line "== $Title ==" 'Cyan' }
function Report([string]$Status, [string]$Name, [string]$Detail) {
    [void]$script:results.Add([pscustomobject]@{ Status = $Status; Name = $Name; Detail = $Detail })
    $color = @{ OK = 'Green'; WARN = 'Yellow'; FAIL = 'Red'; SKIP = 'DarkGray' }[$Status]
    Out-Line ("  [{0,-4}] {1}: {2}" -f $Status, $Name, $Detail) $color
}

function Get-Text([string]$Url) {
    return (Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 30).Content
}
function Test-Url([string]$Url) {
    try { [void](Invoke-WebRequest -Uri $Url -Method Head -UseBasicParsing -TimeoutSec 30); return $true }
    catch { return $false }
}

Out-Line "FinanceDashboard requirement check - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Out-Line "Options from $(Split-Path -Leaf $envFile): TARGET=$TARGET LINK_MODE=$LINK BUILD_TYPE=$($cfg.BUILD_TYPE) QT_VERSION=$QTV LLVM_MINGW_VERSION=$LLVMV"

# -- 1. Options ---------------------------------------------------------------
Section 'Options (.env)'
if ($TARGET -in @('windows', 'linux')) { Report OK 'TARGET' $TARGET } else { Report FAIL 'TARGET' "'$TARGET' (expected windows or linux)" }
if ($LINK -in @('shared', 'static')) { Report OK 'LINK_MODE' $LINK } else { Report FAIL 'LINK_MODE' "'$LINK' (expected shared or static)" }
if ($cfg.BUILD_TYPE -in @('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')) { Report OK 'BUILD_TYPE' $cfg.BUILD_TYPE }
else { Report FAIL 'BUILD_TYPE' "'$($cfg.BUILD_TYPE)' (expected Release, Debug, RelWithDebInfo or MinSizeRel)" }
$qtOk = $QTV -match '^6\.(\d+)\.(\d+)$'
if ($qtOk -and [int]$Matches[1] -ge 8) { Report OK 'QT_VERSION' $QTV }
elseif ($qtOk) { Report FAIL 'QT_VERSION' "$QTV - the project needs Qt 6.8 or later (qt_standard_project_setup(REQUIRES 6.8))"; $qtOk = $false }
else { Report FAIL 'QT_VERSION' "'$QTV' (expected 6.x.y)" }

# -- 2. Build machine (Docker) ------------------------------------------------
Section 'Build machine (Docker)'
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Report FAIL 'Docker CLI' 'not found - install Docker Desktop: https://docs.docker.com/desktop/'
} else {
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $dockerVer = (docker --version 2>$null) -join ' '
    $osType = (docker info --format '{{.OSType}}' 2>$null) -join ''
    $infoExit = $LASTEXITCODE
    $memTotal = (docker info --format '{{.MemTotal}}' 2>$null) -join ''
    $cpus = (docker info --format '{{.NCPU}}' 2>$null) -join ''
    $composeVer = (docker compose version --short 2>$null) -join ''
    $composeExit = $LASTEXITCODE
    $ErrorActionPreference = $prev
    Report OK 'Docker CLI' $dockerVer
    if ($infoExit -ne 0) {
        Report FAIL 'Docker engine' 'not running - start Docker Desktop and wait until it says "Engine running"'
    } else {
        if ($osType -eq 'linux') { Report OK 'Container mode' 'Linux containers' }
        else { Report FAIL 'Container mode' "$osType containers - switch Docker Desktop to Linux containers" }
        $memGb = [math]::Round([double]$memTotal / 1GB, 1)
        $needGb = if ($LINK -eq 'static') { 8 } else { 4 }
        if ($memGb -ge $needGb) { Report OK 'Docker memory' "$memGb GB, $cpus CPUs" }
        else { Report WARN 'Docker memory' "$memGb GB (recommended >= $needGb GB for $LINK; raise it in Docker Desktop > Settings > Resources, or lower QT_BUILD_JOBS/BUILD_JOBS)" }
    }
    if ($composeExit -eq 0 -and $composeVer) { Report OK 'Docker Compose' "v$composeVer" }
    else { Report FAIL 'Docker Compose' "'docker compose' (v2) not available - update Docker Desktop" }
}
# Disk: build-docker/ + dist/ next to the project, image layers on Docker's disk (usually C:).
$needDisk = if ($LINK -eq 'static') { 30 } else { 8 }
$drives = @()
try { $drives += (Split-Path -Qualifier $root).TrimEnd(':') } catch { }
if ($env:SystemDrive) { $drives += $env:SystemDrive.TrimEnd(':') }
foreach ($d in ($drives | Select-Object -Unique)) {
    $drive = Get-PSDrive -Name $d -ErrorAction SilentlyContinue
    if ($drive) {
        $freeGb = [math]::Round($drive.Free / 1GB, 1)
        if ($freeGb -ge $needDisk) { Report OK "Free disk ${d}:" "$freeGb GB" }
        else { Report WARN "Free disk ${d}:" "$freeGb GB (recommended >= $needDisk GB for a $LINK build)" }
    }
}
if (-not $drives) { Report SKIP 'Free disk' 'no drive letter (not running on Windows)' }

# -- 3. Downloads & versions --------------------------------------------------
Section 'Downloads & versions'
if ($Offline) {
    Report SKIP 'Downloads' '-Offline given'
} elseif (-not $qtOk) {
    Report SKIP 'Downloads' 'QT_VERSION invalid'
} else {
    $nodot = $QTV -replace '\.', ''
    $repo = 'https://download.qt.io/online/qtsdkrepository'
    $qtLlvm = $null

    # Host Qt (Linux, prebuilt) - needed by every variant except linux-static
    # (host tools for cross builds; the target Qt itself for linux-shared).
    if (-not ($TARGET -eq 'linux' -and $LINK -eq 'static')) {
        try {
            $xml = Get-Text "$repo/linux_x64/desktop/qt6_$nodot/qt6_$nodot/Updates.xml"
            if ($xml -match "qt\.qt6\.$nodot\.addons\.qtgraphs\.linux_gcc_64") { Report OK 'Qt (Linux, prebuilt)' "$QTV linux_gcc_64 with Qt Graphs" }
            else { Report FAIL 'Qt (Linux, prebuilt)' "$QTV linux_gcc_64 exists but has no Qt Graphs package" }
        } catch { Report FAIL 'Qt (Linux, prebuilt)' "$QTV linux_gcc_64 not found on download.qt.io ($($_.Exception.Message))" }
    }

    if ($LINK -eq 'shared' -and $TARGET -eq 'windows') {
        # Qt >= 6.11 splits the Windows repo per compiler; older versions don't.
        $xml = $null
        foreach ($sub in @("qt6_${nodot}_llvm_mingw", "qt6_$nodot")) {
            try { $xml = Get-Text "$repo/windows_x86/desktop/qt6_$nodot/$sub/Updates.xml"; break } catch { }
        }
        if ($xml -and $xml -match "qt\.qt6\.$nodot\.addons\.qtgraphs\.win64_llvm_mingw") {
            if ($xml -match 'llvm-mingw\s+(\d+\.\d+\.\d+)') { $qtLlvm = $Matches[1] }
            $suffix = ''
            if ($qtLlvm) { $suffix = ", built with LLVM $qtLlvm" }
            Report OK 'Qt (Windows, prebuilt)' "$QTV win64_llvm_mingw with Qt Graphs$suffix"
        } else { Report FAIL 'Qt (Windows, prebuilt)' "$QTV win64_llvm_mingw (with Qt Graphs) not found on download.qt.io" }
    }

    if ($LINK -eq 'static') {
        $mm = $QTV -replace '\.\d+$', ''
        $found = $null
        foreach ($base in @("https://download.qt.io/official_releases/qt/$mm/$QTV/submodules", "https://download.qt.io/archive/qt/$mm/$QTV/submodules")) {
            if (Test-Url "$base/qtbase-everywhere-src-$QTV.tar.xz") { $found = $base; break }
        }
        if (-not $found) { Report FAIL 'Qt sources' "$QTV source tarballs not found (official_releases/ or archive/)" }
        else {
            $missing = @('qtshadertools', 'qtdeclarative', 'qtgraphs') | Where-Object { -not (Test-Url "$found/$_-everywhere-src-$QTV.tar.xz") }
            if ($missing) { Report FAIL 'Qt sources' ("missing: " + ($missing -join ', ')) }
            else { Report OK 'Qt sources' "qtbase, qtshadertools, qtdeclarative, qtgraphs $QTV" }
        }
    }

    if ($TARGET -eq 'windows') {
        try {
            $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/mstorsjo/llvm-mingw/releases/tags/$LLVMV" -TimeoutSec 30
            $assetName = "llvm-mingw-$LLVMV-ucrt-ubuntu-20.04-x86_64.tar.xz"
            $relLlvm = $null
            if ($rel.name -match 'LLVM\s+(\d+\.\d+\.\d+)') { $relLlvm = $Matches[1] }
            if (-not ($rel.assets | Where-Object { $_.name -eq $assetName })) {
                Report FAIL 'llvm-mingw' "release $LLVMV has no $assetName"
            } else {
                Report OK 'llvm-mingw' "release $LLVMV ($($rel.name))"
                if ($LINK -eq 'static') {
                    Report SKIP 'LLVM version match' 'not needed for static: Qt is compiled from source with this same toolchain'
                } elseif (-not $qtLlvm -or -not $relLlvm) {
                    Report WARN 'LLVM version match' "couldn't read both versions (Qt: $qtLlvm, llvm-mingw: $relLlvm) - check by hand"
                } elseif ($qtLlvm -eq $relLlvm) {
                    Report OK 'LLVM version match' "Qt's binaries and llvm-mingw $LLVMV are both LLVM $relLlvm"
                } else {
                    Report FAIL 'LLVM version match' "Qt $QTV was built with LLVM $qtLlvm but llvm-mingw $LLVMV is LLVM $relLlvm - set LLVM_MINGW_VERSION to the llvm-mingw release with LLVM $qtLlvm (https://github.com/mstorsjo/llvm-mingw/releases)"
                }
            }
        } catch { Report FAIL 'llvm-mingw' "release '$LLVMV' not found on GitHub ($($_.Exception.Message))" }
    }

    if (Test-Url 'https://pypi.org/simple/aqtinstall/') { Report OK 'PyPI' 'reachable (aqtinstall dependencies, CMake)' }
    else { Report WARN 'PyPI' 'pypi.org not reachable from this computer - the image build needs it' }
}

# -- 4. Can THIS computer run the result? -------------------------------------
Section "Running the app on this computer ($TARGET, $LINK)"
if (-not $onWindows) {
    Report SKIP 'Run check' 'this script is not running on Windows - use ./tools/check-requirements.sh'
} elseif ($TARGET -ne 'windows') {
    Report SKIP 'Run check' "TARGET=$TARGET can't run on Windows - run ./tools/check-requirements.sh on the Linux machine, or just start the built app there (its launcher checks everything)"
} else {
    # Qt 6.11: Windows 10 1809 (build 17763) or later, x86_64 - https://doc.qt.io/qt-6/supported-platforms.html
    $os = Get-CimInstance Win32_OperatingSystem
    $build = [int]$os.BuildNumber
    if ($build -ge 17763) { Report OK 'Windows version' "$($os.Caption) build $build (needs 10 1809 / build 17763+)" }
    else { Report FAIL 'Windows version' "$($os.Caption) build $build - Qt 6 needs Windows 10 1809 (build 17763) or later" }
    $arch = $env:PROCESSOR_ARCHITECTURE
    if ($env:PROCESSOR_ARCHITEW6432) { $arch = $env:PROCESSOR_ARCHITEW6432 }
    if ($arch -eq 'AMD64') { Report OK 'CPU architecture' 'x86_64' }
    elseif ($arch -eq 'ARM64') { Report WARN 'CPU architecture' 'ARM64 - the x86_64 app runs under Windows 11 emulation (slower), not on Windows 10 ARM' }
    else { Report FAIL 'CPU architecture' "$arch - the app is x86_64 only" }
    if ($LINK -eq 'shared') {
        Report OK 'Libraries' 'nothing to install: the Qt DLLs and C++ runtime ship next to the exe'
    } else {
        Report OK 'Libraries' 'nothing to install: everything is linked into the exe'
    }
}

# -- Summary ------------------------------------------------------------------
$fails = @($script:results | Where-Object { $_.Status -eq 'FAIL' }).Count
$warns = @($script:results | Where-Object { $_.Status -eq 'WARN' }).Count
Out-Line ''
if ($fails -eq 0) { Out-Line "RESULT: OK ($warns warning(s))" 'Green' }
else { Out-Line "RESULT: $fails requirement(s) not met, $warns warning(s)" 'Red' }

if ($LogFile) {
    $dir = Split-Path -Parent $LogFile
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    [IO.File]::WriteAllLines($LogFile, [string[]]$script:logLines, (New-Object System.Text.UTF8Encoding($false)))
}
if ($fails -gt 0) { exit 1 } else { exit 0 }
