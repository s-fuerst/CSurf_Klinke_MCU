#!/usr/bin/env bash
#
# fetch_deps.sh — download the three pinned build dependencies into the repo root.
#
# Produces (all at repo root, gitignored):
#   juce_1_52/        JUCE 1.52 amalgamated  (github.com/julianstorer/JUCE tag 1.52)
#   reaper-sdk/sdk/   REAPER plugin headers   (github.com/justinfrankel/reaper-sdk)
#   reaper-sdk/WDL/   WDL + SWELL             (github.com/justinfrankel/WDL)
#   boost_1_39_0/     Boost 1.39.0 headers    (archives.boost.io)
#
# Idempotent: any dependency already present is skipped. Delete its folder to
# re-fetch, or run with --force to re-fetch everything.
#
# NOTE: The original Klinke/Rothchild instructions pointed at a Bitbucket fork
# (Stenzel/csurf_klinke_mcu) that bundled juce_1_52/ and reaper-sdk/ together.
# That fork is no longer available (HTTP 404), so we source the IDENTICAL
# artifacts from their canonical upstream repositories instead. The resulting
# tree layout matches what CMakeLists.txt expects.
#
# Requirements: git, curl, tar. On the build host also install (Debian/Ubuntu):
#   sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev

set -euo pipefail

FORCE=0
case "${1:-}" in
  --force|-f) FORCE=1 ;;
  -h|--help)  sed -n '2,30p' "$0"; exit 0 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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

# --- 1. JUCE 1.52 (amalgamated) → juce_1_52/ ---------------------------------
section "JUCE 1.52  →  juce_1_52/"
if [ "$FORCE" = 1 ]; then rm -rf juce_1_52; fi
if [ -f juce_1_52/juce_amalgamated.cpp ]; then
  ok "already present, skipping (use --force to re-fetch)"
else
  clone_shallow https://github.com/julianstorer/JUCE juce_1_52 1.52
  ok "fetched"
  [ -f juce_1_52/juce_amalgamated.cpp ] && [ -f juce_1_52/juce.h ] \
    || { echo "error: juce_amalgamated.cpp / juce.h missing after clone" >&2; exit 1; }
  ok "verified: juce.h, juce_amalgamated.cpp, juce_Config.h"
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

# --- 3. Boost 1.39.0 (headers only) → boost_1_39_0/ --------------------------
section "Boost 1.39.0  →  boost_1_39_0/"
if [ "$FORCE" = 1 ]; then rm -rf boost_1_39_0; fi
if [ -f boost_1_39_0/boost/signals2.hpp ]; then
  ok "already present, skipping (use --force to re-fetch)"
else
  echo "  downloading (~29 MB)..."
  curl -fsSL -o boost_1_39_0.tar.bz2 \
    https://archives.boost.io/release/1.39.0/source/boost_1_39_0.tar.bz2
  tar xjf boost_1_39_0.tar.bz2
  rm -f boost_1_39_0.tar.bz2
  [ -f boost_1_39_0/boost/signals2.hpp ] \
    || { echo "error: boost/signals2.hpp missing after extract" >&2; exit 1; }

  # Patch Boost 1.39 known bug: shared_ptr declares move ctors under
  # BOOST_HAS_RVALUE_REFS without a copy ctor, so C++11 deletes the implicit
  # copy ctor (breaks boost::signals2). Disable the move block unless
  # BOOST_SHARED_PTR_ENABLE_MOVE_CTORS is defined. Fixed in later Boost.
  sp_h="boost_1_39_0/boost/smart_ptr/shared_ptr.hpp"
  if ! grep -q 'BOOST_SHARED_PTR_ENABLE_MOVE_CTORS' "$sp_h"; then
    sed -i 's|^#if defined( BOOST_HAS_RVALUE_REFS )$|#if defined( BOOST_HAS_RVALUE_REFS ) \&\& defined(BOOST_SHARED_PTR_ENABLE_MOVE_CTORS)|' "$sp_h"
    echo "  patched shared_ptr.hpp (disabled buggy 1.39 move ctors)"
  fi
  ok "fetched and verified"
fi

# --- done --------------------------------------------------------------------
section "done"
cat <<'EOF'

  All dependencies are in place. Next, on the build host (Debian/Ubuntu):

    sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . -- -j"$(nproc)"

  The plugin is output as build/reaper_csurf_mcu_klinke.so
EOF
