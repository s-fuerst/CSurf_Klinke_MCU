#!/usr/bin/env bash
#
# scripts/build-windows-fast.sh — fast incremental Windows build via /mnt/c
#
# Unlike build-windows.sh (which builds in-place on WSL ext4 via the UNC
# mount), this script mirrors the source tree onto native NTFS (/mnt/c) and
# builds there with the Ninja generator.  On NTFS, both Ninja's stat() and
# MSVC's file tracker work correctly, giving Linux-like incremental speed
# (~10-15 s for a one-file change, ~1-2 s for a no-op build).
#
# Cost: a one-time --setup rsync of the deps (~400 MB, several minutes over
# drvfs).  Per-build rsync of source files only (~5 MB, a few seconds).
# The WSL repo (where you edit and use git) is untouched.
#
# Usage:
#   scripts/build-windows-fast.sh --setup     # one-time: rsync deps + source
#   scripts/build-windows-fast.sh             # incremental: rsync source, build, deploy
#   scripts/build-windows-fast.sh --clean     # wipe build dir, reconfigure + build
#   scripts/build-windows-fast.sh --reconfigure  # re-run CMake configure, then build
#   scripts/build-windows-fast.sh --debug     # Debug config (needs --clean first)
#   scripts/build-windows-fast.sh --no-deploy
#
#   Switching release <-> debug requires --clean (Ninja is single-config).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# --- options ------------------------------------------------------------------
SETUP=0
CLEAN=0
RECONFIGURE=0
BUILD_TYPE=Release
DEPLOY=1
JOBS="$(nproc)"
while [ $# -gt 0 ]; do
  case "$1" in
    --setup)       SETUP=1 ;;
    --clean)       CLEAN=1 ;;
    --reconfigure) RECONFIGURE=1 ;;
    --debug)       BUILD_TYPE=Debug; RECONFIGURE=1 ;;
    --no-deploy)   DEPLOY=0 ;;
    -j)            shift; JOBS="$1" ;;
    -j*)           JOBS="${1#-j}" ;;
    -h|--help)     sed -n '2,34p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1 (try --help)" >&2; exit 2 ;;
  esac
  shift
done

# --- sanity: rsync must be available -----------------------------------------
command -v rsync >/dev/null 2>&1 || { echo "ERROR: rsync not found in PATH." >&2; exit 1; }

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

win_exists() { local u; u=$(wslpath -u "$1" 2>/dev/null) || return 1; [ -e "$u" ]; }
if [ -z "${VSVARS:-}" ] || ! win_exists "$VSVARS"; then
  echo "ERROR: could not locate vcvars64.bat." >&2
  exit 1
fi
if [ -z "${NINJA_EXE:-}" ] || ! win_exists "$NINJA_EXE"; then
  echo "ERROR: could not locate the Ninja bundled with Visual Studio." >&2
  exit 1
fi
echo "MSVC env:   $VSVARS"
echo "Ninja:      $NINJA_EXE"

# --- ensure portable CMake (>= 3.22) -----------------------------------------
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

# --- deps check (in WSL repo, for --setup) -----------------------------------
for d in juce_8 reaper-sdk boost_1_91_0; do
  if [ ! -d "$ROOT/$d" ]; then
    echo "ERROR: dependency '$d/' is missing. Run ./fetch_deps.sh first." >&2
    exit 1
  fi
done

# --- paths -------------------------------------------------------------------
MIRROR="/mnt/c/csurf_klinke_mcu"
BUILD_DIR="$MIRROR/build_win"
MIRROR_WIN=$(wslpath -w "$MIRROR")    # C:\\csurf_klinke_mcu (bat uses this)
REPO_WIN=$(wslpath -w "$ROOT")          # \\\\wsl.localhost\\... (robocopy source)
CMAKE_WIN=$(wslpath -w "$CMAKE_EXE")    # UNC -> .cache/.../cmake.exe

# =============================================================================
#  --setup: one-time sync of source + deps to native NTFS (via robocopy)
#  Using robocopy from the Windows side avoids the drvfs small-file bottleneck.
# =============================================================================
if [ "$SETUP" = 1 ]; then
  mkdir -p "$MIRROR"
  echo "=== one-time setup: mirror source + deps to $MIRROR (robocopy, ~1 min) ==="
  cat > /tmp/_setup_robo.bat <<EOF
@echo off
pushd "$REPO_WIN"
robocopy . "$MIRROR_WIN" /E /NFL /NDL /NJH /NJS /XD build build_win .git scripts
popd
exit /b 0
EOF
  sed -i 's/$/\r/' /tmp/_setup_robo.bat
  ( cd /mnt/c && cmd.exe /c "$(wslpath -w /tmp/_setup_robo.bat)" )
  rm -f /tmp/_setup_robo.bat
  echo "Setup complete. You can now run: scripts/build-windows-fast.sh"
  exit 0
fi

# --- guard: --setup must be run first ----------------------------------------
if [ ! -d "$MIRROR/juce_8" ]; then
  echo "ERROR: mirror not set up. Run: $0 --setup" >&2
  exit 1
fi

# --- per-build source sync (quick, deps excluded) ----------------------------
echo "=== rsync source (WSL -> NTFS) ==="
rsync -a --delete \
  --exclude build/ --exclude build_win/ --exclude .git/ \
  --exclude juce_8/ --exclude boost_1_91_0/ --exclude reaper-sdk/ \
  --exclude VERSION.txt \
  --exclude scripts/ \
  --exclude '*.log' --exclude '*.binlog' \
  --exclude '*.obj' --exclude '*.dll' --exclude '*.exe' --exclude '*.so' \
  "$ROOT/" "$MIRROR/"

# --- clean / reconfigure -----------------------------------------------------
if [ "$CLEAN" = 1 ]; then
  rm -rf "$BUILD_DIR"
  echo "Cleaned $BUILD_DIR/"
fi

NEED_CONFIGURE=0
if [ "$CLEAN" = 1 ] || [ "$RECONFIGURE" = 1 ] \
   || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  NEED_CONFIGURE=1
fi

# --- generate the .bat (no pushd — source is on C:, native NTFS) -------------
BAT="$(mktemp --suffix=.bat)"
trap 'rm -f "$BAT"' EXIT
if [ "$NEED_CONFIGURE" = 1 ]; then
  echo ">> CMake configure (Ninja, $BUILD_TYPE)"
  cat > "$BAT" <<EOF
@echo off
setlocal
cd /d "$MIRROR_WIN"
if errorlevel 1 ( echo ERROR: cd to $MIRROR_WIN failed & goto :fail )
call "$VSVARS"
if errorlevel 1 ( echo ERROR: vcvars64.bat failed & goto :fail )
echo === CMake configure (Ninja, $BUILD_TYPE) ===
"$CMAKE_WIN" -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_MAKE_PROGRAM="$NINJA_EXE" -S . -B build_win
if errorlevel 1 ( echo ERROR: CMake configure failed & goto :fail )
echo === Ninja build ===
"$CMAKE_WIN" --build build_win --parallel $JOBS
if errorlevel 1 ( echo ERROR: build failed & goto :fail )
endlocal
exit /b 0
:fail
endlocal
exit /b 1
EOF
else
  echo ">> incremental build (configure skipped; pass --reconfigure to force)"
  cat > "$BAT" <<EOF
@echo off
setlocal
cd /d "$MIRROR_WIN"
if errorlevel 1 ( echo ERROR: cd to $MIRROR_WIN failed & goto :fail )
call "$VSVARS"
if errorlevel 1 ( echo ERROR: vcvars64.bat failed & goto :fail )
echo === Ninja build ===
"$CMAKE_WIN" --build build_win --parallel $JOBS
if errorlevel 1 ( echo ERROR: build failed & goto :fail )
endlocal
exit /b 0
:fail
endlocal
exit /b 1
EOF
fi
sed -i 's/$/\r/' "$BAT"

# --- run it (from /mnt/c so cmd's cwd is a real drive) -----------------------
echo "=== building (Ninja on NTFS) ==="
( cd /mnt/c && cmd.exe /c "$(wslpath -w "$BAT")" )
RC=$?
if [ "$RC" -ne 0 ]; then
  echo "ERROR: build failed (cmd.exe exit $RC)." >&2
  exit "$RC"
fi

# --- locate artifact (Ninja single-config -> directly in build dir) -----------
DLL="$BUILD_DIR/reaper_csurf_mcu_klinke_x64.dll"
if [ ! -f "$DLL" ]; then
  echo "ERROR: build reported success but $DLL is missing." >&2
  exit 1
fi
echo
echo "=== built: $DLL ($(du -h "$DLL" | cut -f1)) ==="

# --- deploy ------------------------------------------------------------------
if [ "$DEPLOY" = 1 ]; then
  DEST="${MCU_USERPLUGINS:-}"
  if [ -z "$DEST" ]; then
    DEST="$(ls -d /mnt/c/Users/*/AppData/Roaming/REAPER/UserPlugins 2>/dev/null | head -1)"
  fi
  if [ -n "$DEST" ] && [ -d "$DEST" ]; then
    cp -f "$DLL" "$DEST/"
    echo "=== deployed to: $(wslpath -w "$DEST")\\reaper_csurf_mcu_klinke_x64.dll ==="
    echo "Restart REAPER fully to load the new .dll."
  else
    echo "WARNING: no UserPlugins found under /mnt/c/Users/*" >&2
  fi
fi
