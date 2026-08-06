#!/usr/bin/env bash
#
# fetch_deps.sh — download the three pinned build dependencies into the repo root.
#
# Produces (all at repo root, gitignored):
#   juce_8/          JUCE 8 module build   (github.com/juce-framework/JUCE tag 8.0.14)
#   reaper-sdk/sdk/   REAPER plugin headers  (github.com/justinfrankel/reaper-sdk)
#   reaper-sdk/WDL/   WDL + SWELL            (github.com/justinfrankel/WDL)
#   boost_1_91_0/    Boost 1.91.0 headers   (archives.boost.io)
#
# Idempotent: any dependency already present is skipped. Delete its folder to
# re-fetch, or run with --force to re-fetch everything.
#
# Requirements: git, curl, tar. On the build host also install (Debian/Ubuntu):
#   sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev libcurl4-openssl-dev

set -euo pipefail

FORCE=0
case "${1:-}" in
  --force|-f) FORCE=1 ;;
  -h|--help)  sed -n '2,30p' "$0"; exit 0 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# --- helpers -----------------------------------------------------------------

# clone_shallow <url> <dest> [ref]  — shallow clone, then drop its .git
clone_shallow() {
  local url="$1" dest="$2" ref="${3:-}"
  if [ -n "$ref" ]; then
    git clone --depth 1 --branch "$ref" "$url" "$dest"
  else
    git clone --depth 1 "$url" "$dest"
  fi
  rm -rf "$dest/.git"
}

section() { printf '\n=== %s ===\n' "$*"; }
ok()      { printf '  ✓ %s\n' "$*"; }

# sanity: tools this script needs
for t in git curl tar; do
  command -v "$t" >/dev/null 2>&1 || { echo "error: '$t' not found in PATH" >&2; exit 1; }
done

# --- 1. JUCE 8 (module build) → juce_8/ --------------------------------------
section "JUCE 8  →  juce_8/"
if [ "$FORCE" = 1 ]; then rm -rf juce_8; fi
if [ -f juce_8/CMakeLists.txt ] && [ -d juce_8/modules/juce_gui_basics ]; then
  ok "already present, skipping (use --force to re-fetch)"
else
  clone_shallow https://github.com/juce-framework/JUCE juce_8 8.0.14
  ok "fetched"
  [ -f juce_8/CMakeLists.txt ] && [ -d juce_8/modules/juce_gui_basics ] \
    || { echo "error: juce_8/CMakeLists.txt or modules missing after clone" >&2; exit 1; }
  ok "verified: CMakeLists.txt, modules/"
fi

# --- 2. REAPER SDK → reaper-sdk/sdk/  +  WDL/SWELL → reaper-sdk/WDL/ ----------
section "REAPER SDK  →  reaper-sdk/sdk/  +  reaper-sdk/WDL/"
if [ "$FORCE" = 1 ]; then rm -rf reaper-sdk; fi
if [ -d reaper-sdk/sdk ] && [ -d reaper-sdk/WDL/swell ]; then
  ok "already present, skipping (use --force to re-fetch)"
else
  rm -rf reaper-sdk
  clone_shallow https://github.com/justinfrankel/reaper-sdk reaper-sdk
  [ -f reaper-sdk/sdk/reaper_plugin.h ] \
    || { echo "error: reaper-sdk/sdk/reaper_plugin.h missing" >&2; exit 1; }
  ok "sdk/ headers in place"

  tmp="$(mktemp -d)"
  clone_shallow https://github.com/justinfrankel/WDL "$tmp/src"
  mv "$tmp/src/WDL" reaper-sdk/WDL
  rm -rf "$tmp"
  [ -f reaper-sdk/WDL/swell/swell-modstub-generic.cpp ] && [ -f reaper-sdk/WDL/ptrlist.h ] \
    || { echo "error: WDL/swell or WDL/ptrlist.h missing" >&2; exit 1; }
  ok "WDL + SWELL in place (swell-modstub-generic.cpp, ptrlist.h)"
fi

# --- 3. Boost 1.91.0 (headers only) → boost_1_91_0/ --------------------------
section "Boost 1.91.0  →  boost_1_91_0/"
if [ "$FORCE" = 1 ]; then rm -rf boost_1_91_0; fi
if [ -f boost_1_91_0/boost/signals2.hpp ]; then
  ok "already present, skipping (use --force to re-fetch)"
else
  echo "  downloading (~180 MB)..."
  curl -fsSL -o boost_1_91_0.tar.bz2 \
    https://archives.boost.io/release/1.91.0/source/boost_1_91_0.tar.bz2
  tar xjf boost_1_91_0.tar.bz2
  rm -f boost_1_91_0.tar.bz2
  [ -f boost_1_91_0/boost/signals2.hpp ] \
    || { echo "error: boost/signals2.hpp missing after extract" >&2; exit 1; }
  # Boost 1.91.0 compiles cleanly under C++17/Clang/libc++ and needs no patches
  # (unlike the old 1.39.0, which required shared_ptr + signals2 fixes for
  # modern compilers). Only headers are used (signals2, smart_ptr).
  ok "fetched and verified"
fi

# --- done --------------------------------------------------------------------
section "done"

case "$(uname -s)" in
  Darwin)
    cat <<'EOF_macOS'

  All dependencies are in place. Next, on this macOS host:

    # 1. Install build tools (one-time)
    xcode-select --install        # if not already done
    brew install cmake             # or install cmake manually

    # 2. Build, deploy, and run REAPER (one command, no manual cmake needed):
    ./scripts/build-and-run-linux-macos.sh --release

    #    Manual alternative (see README.md):
    #    mkdir build && cd build
    #    cmake .. -DCMAKE_BUILD_TYPE=Release -DMCU_DEBUG_LOG=OFF
    #    cmake --build . -- -j"$(sysctl -n hw.ncpu)"
    #    mkdir -p ~/Library/Application\ Support/REAPER/UserPlugins/
    #    cp build/reaper_csurf_mcu_klinke.dylib ~/Library/Application\ Support/REAPER/UserPlugins/

    Then restart REAPER and add "Mackie Control Protocol (Klinke)"
    in Preferences → Control/OSC/web.

  The plugin is output as build/reaper_csurf_mcu_klinke.dylib
EOF_macOS
    ;;
  MINGW*|MSYS*|CYGWIN*)
    # Native Windows (Git Bash / MSYS2 / Cygwin). The WSL mirror script needs
    # wslpath and /mnt/c, so it cannot run from here — point at WSL instead.
    cat <<'EOF_windows'

  All dependencies are in place. Next, on Windows:

    # Build from WSL (recommended; needs WSL with rsync/unzip and Visual
    # Studio 2019+ Build Tools with the MSVC x64 + Windows SDK components):
    wsl
    ./scripts/build-windows-from-wsl.sh --setup   # one-time mirror to C:\csurf_klinke_mcu
    ./scripts/build-windows-from-wsl.sh           # incremental Release build + deploy

    #    Native MSVC alternative (from a Developer Command Prompt), see README.md:
    #    mkdir build && cd build
    #    cmake .. -DCMAKE_BUILD_TYPE=Release -DMCU_DEBUG_LOG=OFF
    #    cmake --build . --config Release
    #    copy Release\reaper_csurf_mcu_klinke_x64.dll %APPDATA%\REAPER\UserPlugins\

    Then restart REAPER and add "Mackie Control Protocol (Klinke)"
    in Preferences → Control/OSC/web.
EOF_windows
    ;;
  Linux)
    # WSL1/2 reports "Linux" from uname -s. Detect via /proc/version, but
    # skip when inside a container (Docker Desktop's WSL2 backend would match).
    if ! [ -f /.dockerenv ] && grep -qi microsoft /proc/version 2>/dev/null; then
      cat <<'EOF_wsl'

  All dependencies are in place. Next, from WSL:

    # Requires: rsync + unzip (sudo apt install rsync unzip) and Visual
    # Studio 2019+ Build Tools (MSVC x64 + Windows SDK) on the Windows side.
    # 1. One-time: mirror source + deps to native NTFS (C:\csurf_klinke_mcu)
    ./scripts/build-windows-from-wsl.sh --setup

    # 2. Build + deploy (incremental Release build into REAPER's UserPlugins)
    ./scripts/build-windows-from-wsl.sh

    #    Native MSVC alternative (from a Developer Command Prompt), see README.md:
    #    mkdir build && cd build
    #    cmake .. -DCMAKE_BUILD_TYPE=Release -DMCU_DEBUG_LOG=OFF
    #    cmake --build . --config Release
    #    copy Release\reaper_csurf_mcu_klinke_x64.dll %APPDATA%\REAPER\UserPlugins\

    Then restart REAPER and add "Mackie Control Protocol (Klinke)"
    in Preferences → Control/OSC/web.

  The plugin is output as C:\csurf_klinke_mcu\build_win\reaper_csurf_mcu_klinke_x64.dll
EOF_wsl
    else
      cat <<'EOF_linux'

  All dependencies are in place. Next, on the build host (Debian/Ubuntu):

    sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev libcurl4-openssl-dev

    ./scripts/build-and-run-linux-macos.sh  --release # build + deploy + start REAPER

    #    Manual alternative (see README.md):
    #    mkdir build && cd build
    #    cmake .. -DCMAKE_BUILD_TYPE=Release -DMCU_DEBUG_LOG=OFF
    #    cmake --build . -j"$(nproc)"

  The plugin is output as build/reaper_csurf_mcu_klinke.so
EOF_linux
    fi
    ;;
  *)
    cat <<'EOF_other'

  All dependencies are in place. See README.md for build instructions
  for your platform.
EOF_other
    ;;
esac
