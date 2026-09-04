# ============================================================================
# skee64 - machine-agnostic, repeatable Windows (MSVC) build driver
# PowerShell edition (native Windows; no WSL/Git Bash required)
#
# Usage:
#   .\build.ps1 [release|debug] [--static | --dynamic] [--full] [--install] [--portable-msvc]
#
# From WSL, invoke the Windows PowerShell explicitly (not pwsh - a Linux pwsh
# in the same shell would run this on Linux, where cmd.exe does not exist):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./build.ps1 [args]
# Note: WSL does not pass its environment variables to Windows processes,
# so env overrides (Skyrim64Path, SKEE_*) must be set on the Windows side;
# CLI arguments do pass through. Any /mnt/<drive>/... value of Skyrim64Path
# or SKEE_VCVARS is converted to a Windows path automatically.
#
# --install: after a successful build, run `cmake --install`, which places
# skee64.dll and all plugin shaders (sources + compiled .cso) under
# $Skyrim64Path\Data\SKSE\Plugins (the CMake install prefix follows
# Skyrim64Path - see CMakeLists.txt). Skyrim64Path must point at the Skyrim
# root directory (the folder containing Data\).
#
# Default mode is incremental: if the project is already configured
# (build/<preset>/build.ninja exists), ninja runs directly against it - only
# changed files are recompiled and relinked, so iteration is fast. HLSL
# shaders in skee64/Shaders are recompiled by fxc when their sources change.
#
# Full mode (--full, or automatically on first run) does the complete setup.
# Every step is idempotent - already-downloaded pieces are detected and
# skipped:
#   1. Initializes the CommonLibSSE-NG git submodule if missing.
#   2. Downloads CMake + Ninja for Windows into ./toolchain (gitignored).
#   3. Fetches vcpkg at the exact commit pinned in vcpkg.json
#      (builtin-baseline) into ./toolchain/vcpkg and bootstraps it.
#   4. Resolves an MSVC compiler environment, in order:
#        a. $env:SKEE_VCVARS (explicit path to a vcvars64.bat / setup bat)
#        b. vswhere (if present)
#        c. the Visual Studio SxS registry keys
#        d. standard install locations on fixed drives
#        e. relocated installs (top-level *studio* folders, bounded scan)
#        f. cl.exe already on PATH (developer prompt)
#      If none is found it downloads a portable MSVC + Windows SDK into
#      ./toolchain/msvc via tools/portable-msvc.py - nothing is installed
#      system-wide. Pass --portable-msvc to force that path.
#   5. Configures and builds with the CMake preset <type>-msvc-vcpkg-<flatrim|static>
#      (dynamic CRT by default; --static / SKEE_RUNTIME=static selects the fully
#       static-CRT variant, which statically links the MSVC runtimes).
#
# Environment overrides:
#   $env:SKEE_VCVARS = 'C:\path\to\vcvars64.bat'  use this compiler environment
#   $env:SKEE_JOBS   = 'N'                        ninja parallelism (default 8)
#   $env:SKEE_RUNTIME= 'static'|'dynamic'         statically (/MT) vs dynamically (/MD) link the MSVC CRT (default dynamic)
#   $env:Skyrim64Path= 'C:\path\to\Skyrim'        game root used by --install
#
# Requires: Windows PowerShell 5.1+ or pwsh on Windows, git.
# The portable-MSVC fallback additionally needs a Python on the Windows PATH.
# ============================================================================

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$ScriptDir = $PSScriptRoot
$Toolchain = Join-Path $ScriptDir 'toolchain'

# Pinned versions (match the known-good reference environment).
$CMakeVersion = '3.31.12'
$NinjaVersion = '1.12.1'
$VcpkgCommit  = 'ee12231b20c95013c6638d845d04c91559a1d1ff' # == vcpkg.json builtin-baseline

# ----------------------------------------------------------------------------
function Info { param([string]$Msg) Write-Host "`n==> $Msg" -ForegroundColor Cyan }
function Warn { param([string]$Msg) Write-Host "    ! $Msg" -ForegroundColor Yellow }
function Die  { param([string]$Msg) Write-Host "`nERROR: $Msg" -ForegroundColor Red; exit 1 }

function Show-Help {
    $inBlock = $false
    foreach ($line in (Get-Content -LiteralPath $PSCommandPath)) {
        if ($line -match '^#') {
            $inBlock = $true
            Write-Host ($line -replace '^# ?', '')
        } elseif ($inBlock) { break }
    }
}

# Convert a WSL-style path (e.g., /mnt/u/foo) to a Windows path (U:\foo).
# Note: WSL strips its internal env vars (WSL_DISTRO_NAME, ...) before
# launching Windows executables, so detect by pattern instead - the form is
# unambiguous and harmless on native Windows. Anything else passes through.
function ConvertTo-WindowsPath {
    param([string]$Path)
    if ($Path -match '^/mnt/([A-Za-z])(?:/(.*))?$') {
        $drive = $matches[1].ToUpper() + ':'
        if ($matches[2]) { return "$drive\$($matches[2] -replace '/', '\')" }
        return "$drive\"
    }
    return $Path
}

# Write a batch file to %TEMP% and run it with cmd.exe. A unique name per
# call avoids collisions between concurrent runs (and access-denied when the
# shared file is still held by a previous cmd); nothing is left in the repo.
# All Windows-specific quoting lives *inside* the .bat, not on the invocation.
function Invoke-Batch {
    param([string[]]$Lines)
    $bat = Join-Path $env:TEMP ("skee64-" + [guid]::NewGuid().ToString('N') + ".bat")
    try {
        Set-Content -LiteralPath $bat -Value (($Lines -join "`r`n") + "`r`n") -Encoding ASCII
        & $bat
    } finally {
        Remove-Item -LiteralPath $bat -Force -ErrorAction SilentlyContinue
    }
}

# Does the given setup bat (or the current environment, if empty) provide a
# working cl.exe?
function Test-MsvcSetup {
    param([string]$VcvarsBat)
    $lines = @('@echo off')
    if ($VcvarsBat) { $lines += 'call "' + $VcvarsBat + '" >nul 2>&1' }
    $lines += 'where cl >nul 2>nul && exit /b 0'
    $lines += 'exit /b 1'
    Invoke-Batch -Lines $lines | Out-Null
    return ($LASTEXITCODE -eq 0)
}

# Load the environment established by a setup bat into this process (via a
# fresh cmd + `set`), so ninja/cmake can be invoked directly from PowerShell.
function Enter-MsvcEnvironment {
    param([string]$VcvarsBat)
    if (-not $VcvarsBat) { return } # cl already on PATH (developer environment)
    # NOTE: build the line list incrementally - an @('a', 'b' + $var + 'c')
    # literal is misparsed by PowerShell (the @() body is parsed as a
    # command, so the concatenation fragments into array-append operands).
    $lines = @('@echo off')
    $lines += 'call "' + $VcvarsBat + '" >nul 2>&1'
    $lines += 'set'
    $out = Invoke-Batch -Lines $lines
    foreach ($line in $out) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2]
        }
    }
}

function Download {
    param([string]$Url, [string]$Dest)
    if ((Test-Path -LiteralPath $Dest) -and ((Get-Item -LiteralPath $Dest).Length -gt 0)) { return }
    Info "Downloading $(Split-Path -Leaf $Dest)"
    $tmp = "$Dest.part"
    try {
        if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
            & curl.exe -fL --retry 3 --retry-delay 2 --connect-timeout 15 -o $tmp $Url
            if ($LASTEXITCODE -ne 0) { throw "curl failed" }
        } else {
            Invoke-WebRequest -Uri $Url -OutFile $tmp -UseBasicParsing
        }
    } catch {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
        Die "Download failed: $Url (network required)"
    }
    Move-Item -LiteralPath $tmp -Destination $Dest -Force
}

# Download + extract CMake if missing or broken (shared by the full path and
# --install, which needs it even when building incrementally).
function Ensure-Cmake {
    $cmakeWorks = $false
    if (Test-Path -LiteralPath $CMakeBin) {
        & $CMakeBin --version | Out-Null
        $cmakeWorks = ($LASTEXITCODE -eq 0)
    }
    if (-not $cmakeWorks) {
        Download "https://github.com/Kitware/CMake/releases/download/v$CMakeVersion/cmake-$CMakeVersion-windows-x86_64.zip" (Join-Path $Toolchain 'cmake.zip')
        Expand-Archive -LiteralPath (Join-Path $Toolchain 'cmake.zip') -DestinationPath $Toolchain -Force
    }
    if (-not (Test-Path -LiteralPath $CMakeBin)) { Die "CMake setup failed (expected $CMakeBin)" }
}

# Download + extract ninja if missing (shared by incremental and full paths).
function Ensure-Ninja {
    if (-not (Test-Path -LiteralPath $NinjaExe)) {
        Download "https://github.com/ninja-build/ninja/releases/download/v$NinjaVersion/ninja-win.zip" (Join-Path $Toolchain 'ninja.zip')
        $extract = Join-Path $Toolchain '.ninja-extract'
        New-Item -ItemType Directory -Force -Path $NinjaDir, $extract | Out-Null
        Expand-Archive -LiteralPath (Join-Path $Toolchain 'ninja.zip') -DestinationPath $extract -Force
        Move-Item -LiteralPath (Join-Path $extract 'ninja.exe') -Destination $NinjaExe -Force
        Remove-Item -LiteralPath $extract -Recurse -Force
    }
    Info "Ninja: $NinjaExe"
}

# Run `cmake --install` so artifacts land in the game directory. The CMake
# install prefix follows Skyrim64Path (see CMakeLists.txt): skee64.dll +
# shader sources + compiled .cso under <root>\Data\SKSE\Plugins.
function Install-IntoGame {
    if (-not $DoInstall) { return }
    # Accept WSL-style paths (e.g., /mnt/u/...) when run from WSL.
    $root = ConvertTo-WindowsPath $env:Skyrim64Path
    if (-not $root) {
        Die "--install requires Skyrim64Path to point at the Skyrim root directory (the folder containing Data/)."
    }
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        Die "--install: Skyrim64Path=$root does not exist."
    }

    Ensure-Cmake
    & $CMakeBin --install $BuildDir --config $CfgName
    if ($LASTEXITCODE -ne 0) { Die "Install failed (see output above)" }
    Info "Installed into $(Join-Path $root 'Data\SKSE\Plugins') (skee64.dll + shaders)"
}

# Locate a vcvars64.bat that establishes an MSVC compiler environment.
# Returns the path, or the sentinel 'ONPATH' if cl.exe is already on PATH.
function Find-Vcvars {
    # 0) explicit override
    if ($env:SKEE_VCVARS) {
        $override = ConvertTo-WindowsPath $env:SKEE_VCVARS
        if (Test-Path -LiteralPath $override) { return $override }
    }

    # 1) vswhere
    foreach ($vsw in @("$env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe",
                       "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe")) {
        if (Test-Path -LiteralPath $vsw) {
            $installs = & $vsw -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
            foreach ($x in @($installs)) {
                $c = Join-Path $x 'VC\Auxiliary\Build\vcvars64.bat'
                if (Test-Path -LiteralPath $c) { return $c }
            }
        }
    }

    # 2) registry SxS\VS7 (both views)
    foreach ($base in @('HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7',
                        'HKLM:\SOFTWARE\Microsoft\VisualStudio\SxS\VS7')) {
        $k = Get-Item $base -ErrorAction SilentlyContinue
        if ($k) {
            foreach ($name in $k.GetPropertyNames()) {
                $c = Join-Path ($k.GetValue($name)) 'VC\Auxiliary\Build\vcvars64.bat'
                if (Test-Path -LiteralPath $c) { return $c }
            }
        }
    }

    # 3) environment variables
    foreach ($e in @($env:VSINSTALLDIR, $env:VCToolsInstallDir)) {
        if ($e) {
            $c = Join-Path $e 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path -LiteralPath $c) { return $c }
        }
    }

    # 4) standard install locations on fixed drives
    # Fixed + Removable (USB) drives. NOTE: use "$d\" below - a bare 'P:' means
    # the *current* directory on that drive in PowerShell, not its root.
    $editions = @('Community', 'Professional', 'Enterprise', 'BuildTools')
    $years    = @('2026', '2025', '2022', '2019')
    $drives   = ([System.IO.DriveInfo]::GetDrives() |
        Where-Object { $_.IsReady -and @('Fixed', 'Removable') -contains $_.DriveType } |
        ForEach-Object { $_.Name.Substring(0, 1) + ':' })
    foreach ($d in $drives) {
        foreach ($y in $years) { foreach ($e in $editions) {
            $c = "$d\Microsoft Visual Studio\$y\$e\VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path -LiteralPath $c) { return $c }
        }}
    }

    # 5) relocated installs: top-level *studio* folders on fixed drives, bounded depth
    foreach ($d in $drives) {
        $roots = Get-ChildItem -Path "$d\" -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '(?i)visual\s*studio|^vs$' }
        foreach ($r in $roots) {
            $hit = Get-ChildItem -Path $r.FullName -Recurse -Depth 4 -Filter vcvars64.bat -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($hit) { return $hit.FullName }
        }
    }

    # 6) cl.exe already on PATH (developer prompt)?
    if (Get-Command cl -ErrorAction SilentlyContinue) { return 'ONPATH' }
    return $null
}

function Fetch-PortableMsvc {
    $channel = if ($env:SKEE_MSVS_CHANNEL) { $env:SKEE_MSVS_CHANNEL } else { 'latest' }
    Info "No Visual Studio installation found - downloading portable MSVC + Windows SDK (channel: $channel) into toolchain/msvc (~500 MB, first run only)"
    $py = Get-Command python -ErrorAction SilentlyContinue
    if (-not $py) {
        Die @"
No usable Python found to download the portable MSVC toolchain.
  Options:
    - install Visual Studio Build Tools (or point SKEE_VCVARS at a vcvars64.bat), or
    - put 'python' on the Windows PATH, or
    - use build.sh from WSL/Git Bash (falls back to python3 + msiextract).
"@
    }
    Push-Location $Toolchain
    try {
        & $py.Source (Join-Path $ScriptDir 'tools\portable-msvc.py') --accept-license --vs $channel
        if ($LASTEXITCODE -ne 0) { Die "Portable MSVC download failed (network required)" }
    } finally { Pop-Location }
}

# ----------------------------------------------------------------------------
$BuildType     = 'release'
$Runtime       = if ($env:SKEE_RUNTIME) { $env:SKEE_RUNTIME } else { 'dynamic' } # static|dynamic -> MSVC CRT linkage (default dynamic)
$ForceFull     = $false
$DoInstall     = $false
$ForcePortable = $false
foreach ($arg in $args) {
    switch -Regex ($arg) {
        '^(release|debug)$' { $BuildType = $arg }
        '^--static$'        { $Runtime = 'static' }
        '^--dynamic$'       { $Runtime = 'dynamic' }
        '^--full$'          { $ForceFull = $true }
        '^--install$'       { $DoInstall = $true }
        '^--portable-msvc$' { $ForcePortable = $true }
        '^(?:-h|--help)$'   { Show-Help; exit 0 }
        default {
            Write-Host "Unknown argument: $arg (expected [release|debug] [--static|--dynamic] [--full] [--install] [--portable-msvc])" -ForegroundColor Red
            exit 2
        }
    }
}

switch ($Runtime) {
    'static'  { $RuntimeTag = 'static' }
    'dynamic' { $RuntimeTag = 'flatrim' }
    default   { Write-Host "Unknown runtime: $Runtime (expected static|dynamic)" -ForegroundColor Red; exit 2 }
}
$Preset   = "$BuildType-msvc-vcpkg-$RuntimeTag"
$BuildDir = Join-Path $ScriptDir "build/$Preset"
$CfgName  = $BuildType.Substring(0, 1).ToUpper() + $BuildType.Substring(1)

$CMakeRoot     = Join-Path $Toolchain "cmake-$CMakeVersion-windows-x86_64"
$CMakeBin      = Join-Path $CMakeRoot 'bin\cmake.exe'
$NinjaDir      = Join-Path $Toolchain 'ninja'
$NinjaExe      = Join-Path $NinjaDir 'ninja.exe'
$VcpkgDir      = Join-Path $Toolchain 'vcpkg'
$PortableDir   = Join-Path $Toolchain 'msvc'
$PortableSetup = Join-Path $PortableDir 'setup_x64.bat'

# ----------------------------------------------------------------------------
if (-not (Get-Command cmd.exe -ErrorAction SilentlyContinue)) {
    Die "cmd.exe not found - this project builds with MSVC on Windows. Run this script from a Windows shell."
}

Info "skee64 build: $BuildType (preset: $Preset)"
New-Item -ItemType Directory -Force -Path $Toolchain | Out-Null

# --- 1. MSVC compiler environment --------------------------------------------
# Needed by both the incremental and full paths (vcvars also puts fxc.exe on
# PATH, which the shader compile step needs).
$MsvcSetup = '' # setup bat that establishes the compiler env ("" = already in env)
if ($ForcePortable) {
    Info "Forcing portable MSVC toolchain"
} else {
    $detected = Find-Vcvars
    if ($detected -eq 'ONPATH') {
        $MsvcSetup = '' # cl is already on PATH
        Info "MSVC: cl.exe found on PATH (developer environment)"
    } elseif ($detected) {
        if (Test-MsvcSetup -VcvarsBat $detected) {
            $MsvcSetup = $detected
            Info "MSVC: $detected"
        } else {
            Warn "Found $detected but it does not provide a working cl.exe - ignoring."
        }
    }
}

# Fall back to the portable toolchain if no working compiler env was found.
if (-not (Test-MsvcSetup -VcvarsBat $MsvcSetup)) {
    if ((Test-Path -LiteralPath $PortableSetup) -and (Test-MsvcSetup -VcvarsBat $PortableSetup)) {
        $MsvcSetup = $PortableSetup
        Info "MSVC: portable toolchain at $PortableDir"
    } else {
        Fetch-PortableMsvc
        if ((Test-Path -LiteralPath $PortableSetup) -and (Test-MsvcSetup -VcvarsBat $PortableSetup)) {
            $MsvcSetup = $PortableSetup
            Info "MSVC: portable toolchain at $PortableDir"
        } else {
            Die "Portable MSVC setup failed (expected $PortableSetup with a working cl.exe)."
        }
    }
}

# --- 2. incremental build (default) -------------------------------------------
# If the project is already configured, run ninja directly against the existing
# build.ninja: only changed files are recompiled + relinked (shaders included).
if (-not $ForceFull -and (Test-Path -LiteralPath (Join-Path $BuildDir 'build.ninja'))) {
    Ensure-Ninja
    $Jobs = if ($env:SKEE_JOBS) { [int]$env:SKEE_JOBS } else { 8 }
    Info "Incremental build (ninja, existing configuration)"
    Enter-MsvcEnvironment -VcvarsBat $MsvcSetup
    $env:PATH = "$NinjaDir;$env:PATH"
    Push-Location $BuildDir
    try {
        & $NinjaExe -j $Jobs
        if ($LASTEXITCODE -ne 0) { Die "Incremental build failed (see output above)" }
    } finally { Pop-Location }

    $Dll = Join-Path $BuildDir 'skee64.dll'
    if (Test-Path -LiteralPath $Dll) {
        Info "Success: $Dll"
    } else {
        Warn "Build finished but skee64.dll was not found at $Dll"
    }
    Install-IntoGame
    exit 0
}

# --- 3. full setup (first run or --full) ---------------------------------------
# --- 3a. CommonLibSSE-NG submodule ---------------------------------------------
if (-not (Test-Path -LiteralPath (Join-Path $ScriptDir 'CommonLibSSE-NG\CMakeLists.txt'))) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Die "CommonLibSSE-NG/ is missing and git is not available. Run 'git submodule update --init' manually."
    }
    Info "Initializing CommonLibSSE-NG git submodule"
    Push-Location $ScriptDir
    try {
        & git submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) { Die "git submodule update failed (network required)" }
    } finally { Pop-Location }
}

# --- 3b. CMake ------------------------------------------------------------------
Ensure-Cmake
$cmakeVerLine = (& $CMakeBin --version | Select-Object -First 1)
Info "CMake: $(if ($cmakeVerLine) { $cmakeVerLine } else { 'unknown' })"

# --- 3c. Ninja -------------------------------------------------------------------
Ensure-Ninja

# --- 3d. vcpkg source at the pinned commit ----------------------------------------
# Full clone (NOT --depth 1): manifest mode checks out port files from
# historical commits inside the vcpkg repo, so the full history is required.
$vcpkgHead = ''
if (Test-Path -LiteralPath (Join-Path $VcpkgDir '.git')) {
    $vcpkgHead = (& git -C $VcpkgDir rev-parse HEAD 2>$null | Select-Object -First 1)
}
if ($vcpkgHead -ne $VcpkgCommit) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Die "toolchain/vcpkg is missing and git is not available."
    }
    Info "Cloning vcpkg @ $($VcpkgCommit.Substring(0, 12)) (full history, a few hundred MB - required by manifest mode)"
    Remove-Item -LiteralPath $VcpkgDir -Recurse -Force -ErrorAction SilentlyContinue
    & git clone --progress https://github.com/microsoft/vcpkg.git $VcpkgDir
    if ($LASTEXITCODE -ne 0) { Die "Failed to clone vcpkg from GitHub (network required)" }
    & git -C $VcpkgDir checkout -q $VcpkgCommit
    if ($LASTEXITCODE -ne 0) { Die "Failed to check out vcpkg commit $VcpkgCommit" }
}
Info "vcpkg: $VcpkgDir @ $($VcpkgCommit.Substring(0, 12))"

# --- 3e. vcpkg bootstrap (needs the compiler env) ----------------------------------
if (-not (Test-Path -LiteralPath (Join-Path $VcpkgDir 'vcpkg.exe'))) {
    Info "Bootstrapping vcpkg (first run only)"
    Enter-MsvcEnvironment -VcvarsBat $MsvcSetup
    Push-Location $VcpkgDir
    try {
        & .\bootstrap-vcpkg.bat -disableMetrics
        if ($LASTEXITCODE -ne 0) { Die "vcpkg bootstrap failed (needs the MSVC compiler environment)" }
    } finally { Pop-Location }
}

# --- 3f. configure + build -----------------------------------------------------------
$expectedTc = "$VcpkgDir\scripts\buildsystems\vcpkg.cmake"
$cacheFile  = Join-Path $BuildDir 'CMakeCache.txt'
if (Test-Path -LiteralPath $cacheFile) {
    $cachedLine = (Select-String -LiteralPath $cacheFile -Pattern '^CMAKE_TOOLCHAIN_FILE:' | Select-Object -First 1).Line
    if ($cachedLine) {
        $cachedTc = $cachedLine.Substring($cachedLine.IndexOf('=') + 1)
        if (($cachedTc -replace '\\', '/') -ne ($expectedTc -replace '\\', '/')) {
            Warn "Existing build dir was configured with a different vcpkg root - wiping $BuildDir"
            Remove-Item -LiteralPath $BuildDir -Recurse -Force
        }
    }
}

# vcpkg requires the binary cache directory to already exist.
New-Item -ItemType Directory -Force -Path (Join-Path $Toolchain 'vcpkg-cache') | Out-Null

Info "Configuring and building ($Preset) - first run also builds the vcpkg ports, this can take a while"
Enter-MsvcEnvironment -VcvarsBat $MsvcSetup
$env:VCPKG_ROOT                 = $VcpkgDir
$env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $Toolchain 'vcpkg-cache'
$env:PATH = "$CMakeRoot\bin;$NinjaDir;$env:PATH"
Push-Location $ScriptDir
try {
    & $CMakeBin --preset $Preset
    if ($LASTEXITCODE -ne 0) { Die "Build failed (see output above)" }
    & $CMakeBin --build $BuildDir --config $CfgName
    if ($LASTEXITCODE -ne 0) { Die "Build failed (see output above)" }
} finally { Pop-Location }

# --- 4. result ------------------------------------------------------------------------
$Dll = Join-Path $BuildDir 'skee64.dll'
if (Test-Path -LiteralPath $Dll) {
    Info "Success: $Dll"
} else {
    Warn "Build finished but skee64.dll was not found at $Dll"
}
Install-IntoGame
