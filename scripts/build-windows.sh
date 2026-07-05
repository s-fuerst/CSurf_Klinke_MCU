#!/usr/bin/env bash
#
# scripts/build-windows.sh — build the Windows .dll from WSL using the native
# MSVC toolchain.
#
# Why this exists: the CMakeLists.txt Windows branch (res.rc, winmm.lib,
# native Win32 dialogs) must be compiled by MSVC (cl.exe + rc.exe), which only
# runs on the Windows side. This script drives that toolchain from WSL.
#
# How it works:
#   1. Locates Visual Studio + vcvars64.bat + the bundled Ninja via vswhere.
#   2. Ensures a portable CMake >= 3.22 is cached (JUCE 8 requires 3.22; the
#      VS2019-bundled CMake is only 3.20). Downloaded once, reused after.
#   3. Generates a temp .bat that: pushd's into the repo via its UNC path
#      (\\wsl.localhost\<distro>\... which pushd maps to a drive letter, e.g.
#      Z:), calls vcvars64.bat, then runs cmake -G Ninja configure + build.
#      Source stays on WSL ext4 (read-only, stable stat). Build outputs go to
#      /mnt/c (native NTFS) so the try-compile source is stat-able and Ninja's
#      stat-based incremental tracking works reliably — the MSVC file tracker
#      (used for MSBuild .tlog) does NOT hook file I/O on the \\wsl.localhost
#      /9P mount, which is why the VS-generator path always rebuilt everything.
#   4. Runs that .bat through cmd.exe (invoked from /mnt/c so cmd does not
#      complain about a UNC current directory).
#
# Source + deps stay on the WSL filesystem — no duplication. Only build outputs
# (.obj, .dll, .ninja_deps) land on /mnt/c (native NTFS, fast I/O).
#
# Output:  /mnt/c/klinke_build_win/reaper_csurf_mcu_klinke_x64.dll
# Deploy:  copied to %APPDATA%\REAPER\UserPlugins\ (auto-detected) unless
#          --no-deploy is given.
#
# Usage:
#   scripts/build-windows.sh [--clean] [--reconfigure] [--debug] [--no-deploy] [-j N]
#
#   Switching between release/debug requires --clean; with Ninja (single-config)
#   the build type is baked at configure time.  Pass MCU_BUILD_DIR=/path to
#   override the default /mnt/c/klinke_build_win.
#
# Requirements on the Windows host:
#   - Visual Studio 2019+ with the "MSVC v142 - x64" and "Windows 10/11 SDK"
#     components (Community/BuildTools both fine).
#   - vswhere.exe (ships with the VS installer).
#   - Internet access on first run (one-time portable-CMake download, ~50 MB).
#
# NOTE on line endings: the repo is checked out with core.autocrlf=true, so the
# source files have CRLF. That is harmless for MSVC (cl/rc handle CRLF). This
# script itself is LF (shell scripts must be LF).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# --- options ------------------------------------------------------------------
CLEAN=0
RECONFIGURE=0
BUILD_TYPE=Release
DEPLOY=1
JOBS="$(nproc)"
while [ $# -gt 0 ]; do
  case "$1" in
    --clean)        CLEAN=1 ;;
    --reconfigure)  RECONFIGURE=1 ;;
    --debug)        BUILD_TYPE=Debug; RECONFIGURE=1 ;;
    --no-deploy)    DEPLOY=0 ;;
    -j)          shift; JOBS="$1" ;;
    -j*)         JOBS="${1#-j}" ;;
    -h|--help)   sed -n '2,52p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1 (try --help)" >&2; exit 2 ;;
  esac
  shift
done

# --- locate the MSVC environment via vswhere ---------------------------------
VSWHERE="/mnt/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
if [ ! -x "$VSWHERE" ]; then
  echo "ERROR: vswhere.exe not found at '$VSWHERE'." >&2
  echo "       Install Visual Studio 2019+ (or the Build Tools) with the MSVC" >&2
  echo "       x64 + Windows SDK components." >&2
  exit 1
fi

read -r VSVARS < <("$VSWHERE" -latest -products '*' \
    -find 'VC/Auxiliary/Build/vcvars64.bat' | tr -d '\r')
read -r NINJA_EXE < <("$VSWHERE" -latest -products '*' \
    -find 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe' | tr -d '\r')

# vswhere returns Windows paths (C:\...). wslpath -u converts them to WSL paths
# so bash's [ -e ] can actually test them (a bare 'C:\...' is not a WSL path).
# Do NOT run these through wslpath -w — that would strip the backslashes.
win_exists() { local u; u=$(wslpath -u "$1" 2>/dev/null) || return 1; [ -e "$u" ]; }

if [ -z "${VSVARS:-}" ] || ! win_exists "$VSVARS"; then
  echo "ERROR: could not locate vcvars64.bat. Is the 'MSVC v142/v143 - x64'" >&2
  echo "       component installed in Visual Studio?" >&2
  exit 1
fi
if [ -z "${NINJA_EXE:-}" ] || ! win_exists "$NINJA_EXE"; then
  echo "ERROR: could not locate the Ninja bundled with Visual Studio." >&2
  exit 1
fi
echo "MSVC env:   $VSVARS"
echo "Ninja:      $NINJA_EXE"

# --- ensure portable CMake (>= 3.22, what JUCE 8 requires) -------------------
# VS2019 bundles CMake 3.20, which is too old for JUCE 8.0.14 (needs >= 3.22).
# Download a portable Windows CMake once and cache it under ~/.cache.
CMAKE_VERSION="3.31.6"
CMAKE_CACHE="$HOME/.cache/csurf-klinke-mcu"
CMAKE_DIR="$CMAKE_CACHE/cmake-$CMAKE_VERSION-windows-x86_64"
CMAKE_EXE="$CMAKE_DIR/bin/cmake.exe"
if [ ! -x "$CMAKE_EXE" ]; then
  url="https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-windows-x86_64.zip"
  zip="$CMAKE_CACHE/cmake-$CMAKE_VERSION.zip"
  mkdir -p "$CMAKE_CACHE"
  echo "Downloading portable CMake $CMAKE_VERSION (~50 MB, one-time)..."
  curl -fL --retry 3 -o "$zip" "$url"
  rm -rf "$CMAKE_DIR"
  ( cd "$CMAKE_CACHE" && unzip -q "$(basename "$zip")" )
  rm -f "$zip"
fi
[ -x "$CMAKE_EXE" ] || { echo "ERROR: $CMAKE_EXE missing after extract." >&2; exit 1; }
echo "CMake:       $("$CMAKE_EXE" --version | head -1 | tr -d '\r')"

# --- guard: deps must be fetched first ---------------------------------------
for d in juce_8 reaper-sdk boost_1_91_0; do
  if [ ! -d "$ROOT/$d" ]; then
    echo "ERROR: dependency '$d/' is missing. Run ./fetch_deps.sh first." >&2
    exit 1
  fi
done

# --- Windows-form paths ------------------------------------------------------
# Source + deps stay on WSL ext4 (ROOT).  Only the build directory goes to
# native NTFS (/mnt/c) so that the try-compile source is stat-able by Ninja.
# Override with MCU_BUILD_DIR.
REPO_WIN=$(wslpath -w "$ROOT")          # \\wsl.localhost\<distro>\<repo>
CMAKE_WIN=$(wslpath -w "$CMAKE_EXE")    # UNC -> .cache/.../cmake.exe
BUILD_DIR="${MCU_BUILD_DIR:-/mnt/c/klinke_build_win}"
BUILD_DIR_WIN=$(wslpath -w "$BUILD_DIR")    # C:\klinke_build_win (bat arg)

if [ "$CLEAN" = 1 ]; then
  rm -rf "$BUILD_DIR"
  echo "Cleaned $BUILD_DIR/"
fi

# --- decide whether to re-run CMake configure --------------------------------
# With Ninja (single-config), CMake configure is the slow step (~30-40s: it
# re-runs the JUCE/juceide setup and bumps the VERSION.txt build counter). For
# a fast dev loop we configure ONCE and then just build incrementally.
# Re-run configure only when: --clean wiped the build dir, --reconfigure was
# given, or the build dir isn't configured yet.
NEED_CONFIGURE=0
if [ "$CLEAN" = 1 ] || [ "$RECONFIGURE" = 1 ] \
   || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  NEED_CONFIGURE=1
fi

# --- generate the .bat that runs under cmd.exe with the MSVC env active -------
# pushd handles the UNC repo path by mapping it to a temp drive letter (Z:),
# which is what lets MSVC/rc.exe/juceaide work on the WSL filesystem.
BAT="$(mktemp --suffix=.bat)"
trap 'rm -f "$BAT"' EXIT
if [ "$NEED_CONFIGURE" = 1 ]; then
  echo ">> CMake configure (Ninja, $BUILD_TYPE) -> $BUILD_DIR"
  cat > "$BAT" <<EOF
@echo off
setlocal
pushd "$REPO_WIN"
if errorlevel 1 ( echo ERROR: pushd to repo failed & goto :fail )
call "$VSVARS"
if errorlevel 1 ( echo ERROR: vcvars64.bat failed & goto :fail )
echo === CMake configure (Ninja, $BUILD_TYPE) ===
"$CMAKE_WIN" -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_MAKE_PROGRAM="$NINJA_EXE" -S . -B "$BUILD_DIR_WIN"
if errorlevel 1 ( echo ERROR: CMake configure failed & goto :fail )
echo === Ninja build ===
"$CMAKE_WIN" --build "$BUILD_DIR_WIN" --parallel $JOBS
if errorlevel 1 ( echo ERROR: build failed & goto :fail )
popd
endlocal
exit /b 0
:fail
popd 2>nul
endlocal
exit /b 1
EOF
else
  echo ">> incremental build (Ninja) — configure skipped; pass --reconfigure after CMakeLists.txt edits"
  cat > "$BAT" <<EOF
@echo off
setlocal
pushd "$REPO_WIN"
if errorlevel 1 ( echo ERROR: pushd to repo failed & goto :fail )
call "$VSVARS"
if errorlevel 1 ( echo ERROR: vcvars64.bat failed & goto :fail )
echo === Ninja build ===
"$CMAKE_WIN" --build "$BUILD_DIR_WIN" --parallel $JOBS
if errorlevel 1 ( echo ERROR: build failed & goto :fail )
popd
endlocal
exit /b 0
:fail
popd 2>nul
endlocal
exit /b 1
EOF
fi
# CRLF so cmd.exe is happy with labels/goto.
sed -i 's/$/\r/' "$BAT"

# --- run it (from /mnt/c so cmd's cwd is a real drive, not a UNC path) -------
echo "=== building (MSVC via cmd.exe) ==="
( cd /mnt/c && cmd.exe /c "$(wslpath -w "$BAT")" )
RC=$?
# cmd.exe writes progress to stdout as it goes; only its exit code is captured.
if [ "$RC" -ne 0 ]; then
  echo "ERROR: build failed (cmd.exe exit $RC)." >&2
  exit "$RC"
fi

# --- locate the artifact (Ninja single-config -> directly in build dir) ------
DLL="$BUILD_DIR/reaper_csurf_mcu_klinke_x64.dll"
if [ ! -f "$DLL" ]; then
  echo "ERROR: build reported success but $DLL is missing." >&2
  exit 1
fi
echo
echo "=== built: $DLL ($(du -h "$DLL" | cut -f1)) ==="

# --- deploy to REAPER UserPlugins (auto-detect; override with MCU_USERPLUGINS) -
if [ "$DEPLOY" = 1 ]; then
  DEST="${MCU_USERPLUGINS:-}"
  if [ -z "$DEST" ]; then
    DEST="$(ls -d /mnt/c/Users/*/AppData/Roaming/REAPER/UserPlugins 2>/dev/null | head -1)"
  fi
  if [ -n "$DEST" ] && [ -d "$DEST" ]; then
    cp -f "$DLL" "$DEST/"
    echo "=== deployed to: $(wslpath -w "$DEST")\\reaper_csurf_mcu_klinke_x64.dll ==="
    echo "Restart REAPER fully (not just reload) to load the new .dll, then add"
    echo "\"Mackie Control Protocol (Klinke)\" in Preferences -> Control/OSC/web."
  else
    echo "WARNING: no %APPDATA%\\REAPER\\UserPlugins found under /mnt/c/Users/*" >&2
    echo "         (set MCU_USERPLUGINS=/path or pass --no-deploy)." >&2
  fi
fi
