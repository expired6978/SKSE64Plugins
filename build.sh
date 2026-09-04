#!/usr/bin/env bash
# ============================================================================
# skee64 — machine-agnostic, repeatable Windows (MSVC) build driver
#
# Usage:
#   ./build.sh [release|debug] [--static | --dynamic] [--full] [--install] [--portable-msvc]
#
# --install: after a successful build, run `cmake --install`, which places
# skee64.dll and all plugin shaders (sources + compiled .cso) under
# $Skyrim64Path/Data/SKSE/Plugins (the CMake install prefix follows
# Skyrim64Path - see CMakeLists.txt). Skyrim64Path must point at the Skyrim
# root directory (the folder containing Data/); it may be a native path or a
# Windows-style C:\... path under WSL.
#
# Default mode is incremental: if the project is already configured
# (build/<preset>/build.ninja exists), ninja runs directly against it — only
# changed files are recompiled and relinked, so iteration is fast. HLSL
# shaders in skee64/Shaders are recompiled by fxc when their sources change.
#
# Full mode (--full, or automatically on first run) does the complete setup.
# Every step is idempotent — already-downloaded pieces are detected and
# skipped:
#   1. Initializes the CommonLibSSE-NG git submodule if missing.
#   2. Downloads CMake + Ninja for Windows into ./toolchain (gitignored).
#   3. Fetches vcpkg at the exact commit pinned in vcpkg.json
#      (builtin-baseline) into ./toolchain/vcpkg and bootstraps it.
#   4. Resolves an MSVC compiler environment, in order:
#        a. $SKEE_VCVARS (explicit path to a vcvars64.bat / setup bat)
#        b. vswhere (if present)
#        c. the Visual Studio SxS registry keys
#        d. standard install locations on fixed drives
#        e. relocated installs (top-level *studio* folders, bounded scan)
#        f. cl.exe already on PATH (developer prompt)
#      If none is found it downloads a portable MSVC + Windows SDK into
#      ./toolchain/msvc via tools/portable-msvc.py — nothing is installed
#      system-wide. Pass --portable-msvc to force that path.
#   5. Configures and builds with the CMake preset <type>-msvc-vcpkg-<flatrim|static>
#      (dynamic CRT by default; --static / SKEE_RUNTIME=static selects the fully
#       static-CRT variant, which statically links the MSVC runtimes).
#
# Environment overrides:
#   SKEE_VCVARS=/path/to/vcvars64.bat   use this compiler environment
#   SKEE_JOBS=N                         ninja parallelism (default 8)
#   SKEE_RUNTIME=static|dynamic         statically (/MT) vs dynamically (/MD) link the MSVC CRT (default dynamic)
#   Skyrim64Path=/path/to/Skyrim        game root used by --install
#
# Requires: cmd.exe (Windows/WSL/Git Bash), curl or wget, git.
# The portable-MSVC fallback additionally needs a Python on the Windows PATH
# or python3 + msiextract (msitools) in this shell.
# ============================================================================
set -euo pipefail
export MSYS_NO_PATHCONV=1 # Git Bash: don't rewrite /C or Windows-style args

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)"
TOOLCHAIN="$SCRIPT_DIR/toolchain"

# Pinned versions (match the known-good reference environment).
CMAKE_VERSION="3.31.12"
NINJA_VERSION="1.12.1"
VCPKG_COMMIT="ee12231b20c95013c6638d845d04c91559a1d1ff" # == vcpkg.json builtin-baseline

BUILD_TYPE="release"
RUNTIME="${SKEE_RUNTIME:-dynamic}" # static|dynamic -> MSVC CRT linkage (default dynamic)
FORCE_FULL=0
DO_INSTALL=0
FORCE_PORTABLE=0
for arg in "$@"; do
  case "$arg" in
  release | debug) BUILD_TYPE="$arg" ;;
  --static) RUNTIME="static" ;;
  --dynamic) RUNTIME="dynamic" ;;
  --full) FORCE_FULL=1 ;;
  --install) DO_INSTALL=1 ;;
  --portable-msvc) FORCE_PORTABLE=1 ;;
  -h | --help)
    grep '^#' "$0" | sed 's/^# \{0,1\}//'
    exit 0
    ;;
  *)
    echo "Unknown argument: $arg (expected [release|debug] [--static|--dynamic] [--full] [--install] [--portable-msvc])" >&2
    exit 2
    ;;
  esac
done
case "$RUNTIME" in
static) RUNTIME_TAG="static" ;;
dynamic) RUNTIME_TAG="flatrim" ;;
*)
  echo "Unknown runtime: $RUNTIME (expected static|dynamic)" >&2
  exit 2
  ;;
esac
PRESET="${BUILD_TYPE}-msvc-vcpkg-${RUNTIME_TAG}"
BUILD_DIR="$SCRIPT_DIR/build/$PRESET"
CFG_NAME="$(tr '[:lower:]' '[:upper:]' <<<"${BUILD_TYPE:0:1}")${BUILD_TYPE:1}"

CMAKE_ROOT="$TOOLCHAIN/cmake-${CMAKE_VERSION}-windows-x86_64"
CMAKE_BIN="$CMAKE_ROOT/bin/cmake.exe"
NINJA_DIR="$TOOLCHAIN/ninja"
VCPKG_DIR="$TOOLCHAIN/vcpkg"
PORTABLE_SETUP="$TOOLCHAIN/msvc/setup_x64.bat"

# ----------------------------------------------------------------------------
info() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m    ! %s\033[0m\n' "$*"; }
die() {
  printf '\n\033[1;31mERROR: %s\033[0m\n' "$*" >&2
  exit 1
}

# Convert a path in this shell to a Windows path.
to_win() {
  local p="$1"
  if [[ -n ${WSL_DISTRO_NAME:-} ]] && command -v wslpath >/dev/null 2>&1; then
    wslpath -w "$p"
  elif [[ $p =~ ^/([A-Za-z])/(.*)$ ]]; then
    printf '%s:\\%s' "${BASH_REMATCH[1]^^}" "${BASH_REMATCH[2]//\//\\}"
  else
    printf '%s' "${p//\//\\}"
  fi
}

# Write a batch file into the toolchain dir and run it with cmd.exe.
# The .bat path is passed as a single argument (no embedded quotes): WSL/Git
# Bash re-quote it for CreateProcess, which handles spaces correctly. All
# Windows-specific quoting lives *inside* the .bat, not on this invocation.
run_batch() { # $1 = batch contents
  mkdir -p "$TOOLCHAIN"
  local bat="$TOOLCHAIN/.build-step.bat" rc=0
  printf '%s\n' "$1" >"$bat"
  cmd.exe /c "$(to_win "$bat")" || rc=$?
  return $rc
}

download() { # url dest
  [[ -s "$2" ]] && return 0
  info "Downloading $(basename "$2")"
  local tmp="$2.part"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 --retry-delay 2 --connect-timeout 15 -o "$tmp" "$1" ||
      {
        rm -f "$tmp"
        die "Download failed: $1 (network required)"
      }
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$tmp" "$1" ||
      {
        rm -f "$tmp"
        die "Download failed: $1 (network required)"
      }
  else
    rm -f "$tmp"
    die "curl or wget is required to download build dependencies."
  fi
  mv "$tmp" "$2"
}

unzip_win() { # zipwin destwin
  run_batch "@echo off
powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '$1' -DestinationPath '$2' -Force\"
exit /b %errorlevel%" || die "Failed to extract $1"
}

# Download + extract CMake if missing or broken (shared by the full path and
# --install, which needs it even when building incrementally).
ensure_cmake() {
  if ! run_batch "\"$(to_win "$CMAKE_BIN")\" --version >nul 2>&1" >/dev/null; then
    download "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-windows-x86_64.zip" \
      "$TOOLCHAIN/cmake.zip"
    unzip_win "$(to_win "$TOOLCHAIN/cmake.zip")" "$(to_win "$TOOLCHAIN")"
  fi
  [[ -f "$CMAKE_BIN" ]] || die "CMake setup failed (expected $(to_win "$CMAKE_BIN"))"
}

# Download + extract ninja if missing (shared by incremental and full paths).
ensure_ninja() {
  if [[ ! -f "$NINJA_DIR/ninja.exe" ]]; then
    download "https://github.com/ninja-build/ninja/releases/download/v${NINJA_VERSION}/ninja-win.zip" \
      "$TOOLCHAIN/ninja.zip"
    mkdir -p "$NINJA_DIR" "$TOOLCHAIN/.ninja-extract"
    unzip_win "$(to_win "$TOOLCHAIN/ninja.zip")" "$(to_win "$TOOLCHAIN/.ninja-extract")"
    mv "$TOOLCHAIN/.ninja-extract/ninja.exe" "$NINJA_DIR/ninja.exe"
    rm -rf "$TOOLCHAIN/.ninja-extract"
  fi
  info "Ninja: $NINJA_DIR/ninja.exe"
}

# Run `cmake --install` so artifacts land in the game directory. The CMake
# install prefix follows Skyrim64Path (see CMakeLists.txt): skee64.dll +
# shader sources + compiled .cso under <root>/Data/SKSE/Plugins.
install_into_game() {
  [[ $DO_INSTALL -eq 1 ]] || return 0
  local root="${Skyrim64Path:-}"
  if [[ -z "$root" ]]; then
    die "--install requires Skyrim64Path to point at the Skyrim root directory (the folder containing Data/)."
  fi
  # Accept Windows-style paths under WSL (convert to the /mnt/x/... form).
  if [[ "$root" =~ ^[A-Za-z]: ]] && command -v wslpath >/dev/null 2>&1; then
    root="$(wslpath -u "$root")"
  fi
  [[ -d "$root" ]] || die "--install: Skyrim64Path=$root does not exist."

  ensure_cmake
  run_batch "\"$(to_win "$CMAKE_BIN")\" --install \"$(to_win "$BUILD_DIR")\" --config $CFG_NAME || exit /b 1" ||
    die "Install failed (see output above)"
  info "Installed into $root/Data/SKSE/Plugins (skee64.dll + shaders)"
}

# ----------------------------------------------------------------------------
command -v cmd.exe >/dev/null 2>&1 ||
  die "cmd.exe not found — this project builds with MSVC on Windows. Run this script from Windows, WSL, or Git Bash."

info "skee64 build: $BUILD_TYPE (preset: $PRESET)"
mkdir -p "$TOOLCHAIN"

# --- 1. MSVC compiler environment --------------------------------------------
# Needed by both the incremental and full paths (vcvars also puts fxc.exe on
# PATH, which the shader compile step needs).
PORTABLE_DIR="$TOOLCHAIN/msvc"
MSVC_SETUP="" # bat that establishes the compiler env ("" = already in env)

detect_vcvars() {
  local ps="$TOOLCHAIN/.detect-vcvars.ps1" out
  cat >"$ps" <<'PSEOF'
$ErrorActionPreference = 'SilentlyContinue'
$found = $null

# 0) explicit override
if ($env:SKEE_VCVARS -and (Test-Path $env:SKEE_VCVARS)) { Write-Output $env:SKEE_VCVARS; exit 0 }

# 1) vswhere
foreach ($vsw in @("$env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe",
                   "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe")) {
    if (Test-Path $vsw) {
        $installs = & $vsw -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        foreach ($x in @($installs)) {
            $c = Join-Path $x 'VC\Auxiliary\Build\vcvars64.bat'
            if (-not $found -and (Test-Path $c)) { $found = $c }
        }
    }
}

# 2) registry SxS\VS7 (both views)
foreach ($base in @('HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7',
                    'HKLM:\SOFTWARE\Microsoft\VisualStudio\SxS\VS7')) {
    $k = Get-Item $base
    if ($k) {
        foreach ($name in $k.GetPropertyNames()) {
            $c = Join-Path ($k.GetValue($name)) 'VC\Auxiliary\Build\vcvars64.bat'
            if (-not $found -and (Test-Path $c)) { $found = $c }
        }
    }
}

# 3) environment variables
foreach ($e in @($env:VSINSTALLDIR, $env:VCToolsInstallDir)) {
    if ($e) {
        $c = Join-Path $e 'VC\Auxiliary\Build\vcvars64.bat'
        if (-not $found -and (Test-Path $c)) { $found = $c }
    }
}

# 4) standard install locations on fixed drives
# (DriveInfo instead of Get-Volume: the latter returns nothing when
# PowerShell is launched from WSL)
$editions = @('Community','Professional','Enterprise','BuildTools')
$years = @('2026','2025','2022','2019')
# Fixed + Removable (USB) drives. NOTE: use "$d\" below — a bare 'P:' means
# the *current* directory on that drive in PowerShell, not its root.
$drives = ([System.IO.DriveInfo]::GetDrives() | Where-Object { $_.IsReady -and @('Fixed','Removable') -contains $_.DriveType } | ForEach-Object { $_.Name.Substring(0, 1) + ':' })
foreach ($d in $drives) {
    foreach ($y in $years) { foreach ($e in $editions) {
        $c = "$d\Microsoft Visual Studio\$y\$e\VC\Auxiliary\Build\vcvars64.bat"
        if (-not $found -and (Test-Path $c)) { $found = $c }
    }}
}

# 5) relocated installs: top-level *studio* folders on fixed drives, bounded depth
if (-not $found) {
    foreach ($d in $drives) {
        $roots = Get-ChildItem -Path "$d\" -Directory | Where-Object { $_.Name -match '(?i)visual\s*studio|^vs$' }
        foreach ($r in $roots) {
            $hit = Get-ChildItem -Path $r.FullName -Recurse -Depth 4 -Filter vcvars64.bat | Select-Object -First 1
            if ($hit) { $found = $hit.FullName; break }
        }
        if ($found) { break }
    }
}

if ($found) { Write-Output $found; exit 0 }

# 6) cl.exe already on PATH (developer prompt)?
if (Get-Command cl -ErrorAction SilentlyContinue) { Write-Output 'ONPATH'; exit 0 }
exit 1
PSEOF
  out="$(powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(to_win "$ps")" 2>/dev/null | tr -d '\r' | tail -1 || true)"
  printf '%s' "$out"
}

env_ok() { # $1 = setup bat (may be empty)
  local callline=""
  [[ -n "$1" ]] && callline="call \"$1\" >nul 2>&1"
  run_batch "@echo off
$callline
where cl >nul 2>nul && exit /b 0
exit /b 1" >/dev/null
}

# VS channel for the portable download: latest | 2026 | 2022 | 2019
MSVS_CHANNEL="${SKEE_MSVS_CHANNEL:-latest}"

fetch_portable_msvc() {
  info "No Visual Studio installation found — downloading portable MSVC + Windows SDK (channel: $MSVS_CHANNEL) into toolchain/msvc (~500 MB, first run only)"
  local py_win="" wpy
  # Prefer a Windows-side python (uses msiexec, no extra dependencies).
  wpy="$(cmd.exe /C "where python 2>nul" | tr -d '\r' | head -1 || true)"
  if [[ -n "$wpy" ]] && cmd.exe /C "\"$wpy\" --version" >/dev/null 2>&1; then
    py_win="$wpy"
  elif command -v python3 >/dev/null 2>&1 && command -v msiextract >/dev/null 2>&1; then
    : # fall through to host python3 + msiextract
  else
    die "No usable Python found to download the portable MSVC toolchain.
  Options:
    - install Visual Studio Build Tools (or point SKEE_VCVARS at a vcvars64.bat), or
    - put 'python' on the Windows PATH, or
    - install python3 + msitools ('msiextract') in this shell."
  fi

  if [[ -n "$py_win" ]]; then
    run_batch "@echo off
cd /d \"$(to_win "$TOOLCHAIN")\"
\"$py_win\" \"$(to_win "$SCRIPT_DIR/tools/portable-msvc.py")\" --accept-license --vs $MSVS_CHANNEL || exit /b 1" ||
      die "Portable MSVC download failed (network required)"
  else
    (cd "$TOOLCHAIN" && python3 "$SCRIPT_DIR/tools/portable-msvc.py" --accept-license --vs "$MSVS_CHANNEL") ||
      die "Portable MSVC download failed (network required)"
  fi
}

if [[ $FORCE_PORTABLE -eq 1 ]]; then
  info "Forcing portable MSVC toolchain"
else
  detected="$(detect_vcvars)"
  if [[ "$detected" == "ONPATH" ]]; then
    MSVC_SETUP="" # cl is already on PATH
    info "MSVC: cl.exe found on PATH (developer environment)"
  elif [[ -n "$detected" ]]; then
    if env_ok "$detected"; then
      MSVC_SETUP="$detected"
      info "MSVC: $detected"
    else
      warn "Found $detected but it does not provide a working cl.exe — ignoring."
    fi
  fi
fi

# Fall back to the portable toolchain if no working compiler env was found.
if [[ -z "$MSVC_SETUP" ]] || ! env_ok "$MSVC_SETUP"; then
  if [[ -f "$PORTABLE_SETUP" ]] && env_ok "$PORTABLE_SETUP"; then
    MSVC_SETUP="$PORTABLE_SETUP"
    info "MSVC: portable toolchain at $PORTABLE_DIR"
  else
    fetch_portable_msvc
    if [[ -f "$PORTABLE_SETUP" ]] && env_ok "$PORTABLE_SETUP"; then
      MSVC_SETUP="$PORTABLE_SETUP"
      info "MSVC: portable toolchain at $PORTABLE_DIR"
    else
      die "Portable MSVC setup failed (expected $(to_win "$PORTABLE_SETUP") with a working cl.exe)."
    fi
  fi
fi

# --- 2. incremental build (default) -------------------------------------------
# If the project is already configured, run ninja directly against the existing
# build.ninja: only changed files are recompiled + relinked (shaders included).
if [[ $FORCE_FULL -eq 0 && -f "$BUILD_DIR/build.ninja" ]]; then
  ensure_ninja
  JOBS="${SKEE_JOBS:-8}"
  info "Incremental build (ninja, existing configuration)"
  callline=""
  [[ -n "$MSVC_SETUP" ]] && callline="call \"$MSVC_SETUP\" >nul 2>&1"
  run_batch "@echo off
setlocal
$callline
set \"PATH=$(to_win "$NINJA_DIR"));%PATH%\"
cd /d \"$(to_win "$BUILD_DIR")\"
ninja -j $JOBS || exit /b 1" ||
    die "Incremental build failed (see output above)"

  DLL="$BUILD_DIR/skee64.dll"
  if [[ -f "$DLL" ]]; then
    info "Success: $DLL"
  else
    warn "Build finished but skee64.dll was not found at $DLL"
  fi
  install_into_game
  exit 0
fi

# --- 3. full setup (first run or --full) ---------------------------------------
# --- 3a. CommonLibSSE-NG submodule ---------------------------------------------
if [[ ! -f "$SCRIPT_DIR/CommonLibSSE-NG/CMakeLists.txt" ]]; then
  command -v git >/dev/null 2>&1 ||
    die "CommonLibSSE-NG/ is missing and git is not available. Run 'git submodule update --init' manually."
  info "Initializing CommonLibSSE-NG git submodule"
  git -C "$SCRIPT_DIR" submodule update --init --recursive ||
    die "git submodule update failed (network required)"
fi

# --- 3b. CMake ------------------------------------------------------------------
ensure_cmake
cmake_ver="$(run_batch "@echo off
\"$(to_win "$CMAKE_BIN")\" --version" 2>/dev/null | grep -m1 'cmake version')"
info "CMake: ${cmake_ver:-unknown}"

# --- 3c. Ninja -------------------------------------------------------------------
ensure_ninja

# --- 3d. vcpkg source at the pinned commit ----------------------------------------
# Full clone (NOT --depth 1): manifest mode checks out port files from
# historical commits inside the vcpkg repo, so the full history is required.
vcpkg_head="$(git -C "$VCPKG_DIR" rev-parse HEAD 2>/dev/null || true)"
if [[ "$vcpkg_head" != "$VCPKG_COMMIT" ]]; then
  command -v git >/dev/null 2>&1 ||
    die "toolchain/vcpkg is missing and git is not available."
  info "Cloning vcpkg @ ${VCPKG_COMMIT:0:12} (full history, a few hundred MB — required by manifest mode)"
  rm -rf "$VCPKG_DIR"
  (git clone --progress https://github.com/microsoft/vcpkg.git "$VCPKG_DIR" &&
    git -C "$VCPKG_DIR" checkout -q "$VCPKG_COMMIT") ||
    die "Failed to clone vcpkg from GitHub (network required)"
fi
info "vcpkg: $VCPKG_DIR @ ${VCPKG_COMMIT:0:12}"

# --- 3e. vcpkg bootstrap (needs the compiler env) ----------------------------------
if [[ ! -f "$VCPKG_DIR/vcpkg.exe" ]]; then
  info "Bootstrapping vcpkg (first run only)"
  callline=""
  [[ -n "$MSVC_SETUP" ]] && callline="call \"$MSVC_SETUP\" >nul 2>&1"
  run_batch "@echo off
setlocal
$callline
cd /d \"$(to_win "$VCPKG_DIR")\"
bootstrap-vcpkg.bat -disableMetrics || exit /b 1" ||
    die "vcpkg bootstrap failed (needs the MSVC compiler environment)"
fi

# --- 3f. configure + build -----------------------------------------------------------
EXPECTED_TC="$(to_win "$VCPKG_DIR")/scripts/buildsystems/vcpkg.cmake"
EXPECTED_TC="${EXPECTED_TC//\\//}"
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  cached_tc="$(grep -m1 '^CMAKE_TOOLCHAIN_FILE:' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2- || true)"
  if [[ "$cached_tc" != "$EXPECTED_TC" ]]; then
    warn "Existing build dir was configured with a different vcpkg root — wiping $BUILD_DIR"
    rm -rf "$BUILD_DIR"
  fi
fi

# vcpkg requires the binary cache directory to already exist.
mkdir -p "$TOOLCHAIN/vcpkg-cache"

info "Configuring and building ($PRESET) — first run also builds the vcpkg ports, this can take a while"
callline=""
[[ -n "$MSVC_SETUP" ]] && callline="call \"$MSVC_SETUP\" >nul 2>&1"
run_batch "@echo off
setlocal
$callline
set \"VCPKG_ROOT=$(to_win "$VCPKG_DIR")\"
set \"VCPKG_DEFAULT_BINARY_CACHE=$(to_win "$TOOLCHAIN")\\vcpkg-cache\"
set \"PATH=$(to_win "$CMAKE_ROOT/bin");$(to_win "$NINJA_DIR");%PATH%\"
cd /d \"$(to_win "$SCRIPT_DIR")\"
\"$(to_win "$CMAKE_BIN")\" --preset $PRESET || exit /b 1
\"$(to_win "$CMAKE_BIN")\" --build \"$(to_win "$BUILD_DIR")\" --config $CFG_NAME || exit /b 1" ||
  die "Build failed (see output above)"

# --- 4. result ------------------------------------------------------------------------
DLL="$BUILD_DIR/skee64.dll"
if [[ -f "$DLL" ]]; then
  info "Success: $DLL"
else
  warn "Build finished but skee64.dll was not found at $DLL"
fi
install_into_game
