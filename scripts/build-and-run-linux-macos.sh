#!/bin/bash
# build-and-run-linux-macos.sh — Build the extension, deploy it, launch Reaper
#
# Works on Linux (GCC, X11, JACK) and macOS (Apple Clang, CoreAudio/MIDI).
#
# Usage:
#   ./scripts/build-and-run-linux-macos.sh              # quick (incremental) build + deploy + run
#                                                         # auto-configures on first run (no build/ yet)
#   ./scripts/build-and-run-linux-macos.sh --release    # full configure + build (Release) + deploy + run
#   ./scripts/build-and-run-linux-macos.sh --debug      # full configure + build (Debug) + deploy + run
#   ./scripts/build-and-run-linux-macos.sh --clean      # wipe build dir, configure + build + deploy + run
#   ./scripts/build-and-run-linux-macos.sh --reconfigure # rerun CMake, then build + deploy + run
#   ./scripts/build-and-run-linux-macos.sh --no-deploy  # build only; do not deploy or start REAPER
#   ./scripts/build-and-run-linux-macos.sh -j8          # use eight parallel build jobs
#   ./scripts/build-and-run-linux-macos.sh --klinke     # full configure + build (Release) with KLINKE features
#   ./scripts/build-and-run-linux-macos.sh --help       # show this message

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="Release"
QUICK=true
KLINKE=0
CLEAN=0
DEPLOY=1
JOBS_OVERRIDE=""

# --- Options ----------------------------------------------------------------

while [ $# -gt 0 ]; do
    case "$1" in
        --clean)       CLEAN=1; QUICK=false ;;
        --reconfigure) QUICK=false ;;
        --debug)       BUILD_TYPE="Debug"; QUICK=false ;;
        --release)     BUILD_TYPE="Release"; QUICK=false ;;
        --klinke)      KLINKE=1; QUICK=false ;;
        --no-deploy)   DEPLOY=0 ;;
        -j)
            if [ $# -lt 2 ]; then
                echo "ERROR: -j requires a job count." >&2
                exit 2
            fi
            shift
            JOBS_OVERRIDE="$1"
            ;;
        -j*)           JOBS_OVERRIDE="${1#-j}" ;;
        -h|--help)     sed -n '2,16p' "$0"; exit 0 ;;
        *)             echo "unknown argument: $1 (try --help)" >&2; exit 2 ;;
    esac
    shift
done

# --- Platform detection -----------------------------------------------------

UNAME="$(uname -s)"
case "$UNAME" in
    Linux)
        JOBS="$(nproc)"
        ARTIFACT="reaper_csurf_mcu_klinke.so"
        PLUGIN_DIR="$HOME/.config/REAPER/UserPlugins"
        REAPER_EXE="$HOME/opt/REAPER/reaper"
        REAPER_ENV="GDK_BACKEND=x11"
        AUDIO_INFO="JACK → PipeWire → Focusrite Saffire Pro 40"
        MIDI_INFO="direct (no a2jmidid)"
        LAUNCH_METHOD="direct"
        ;;
    Darwin)
        JOBS="$(sysctl -n hw.ncpu)"
        ARTIFACT="reaper_csurf_mcu_klinke.dylib"
        PLUGIN_DIR="$HOME/Library/Application Support/REAPER/UserPlugins"
        REAPER_EXE="/Applications/REAPER.app/Contents/MacOS/REAPER"
        REAPER_ENV=""          # macOS REAPER uses Cocoa natively, no GDK needed
        AUDIO_INFO="CoreAudio"
        MIDI_INFO="CoreMIDI"
        LAUNCH_METHOD="open"   # open -a REAPER &; REAPER_EXE used for the info line
        ;;
    *)
        echo "Unsupported platform: $UNAME"; exit 1 ;;
esac

if [ -n "$JOBS_OVERRIDE" ]; then
    JOBS="$JOBS_OVERRIDE"
fi

# --- Build ------------------------------------------------------------------

echo ""
echo "=== Building $BUILD_TYPE ($UNAME) ==="
if [ "$CLEAN" = 1 ] && [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
    echo "  Cleaned $BUILD_DIR/"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# configure_cmake — the ONLY place that runs `cmake` configure. The quick
# path calls it too when the build dir is not (or no longer) configured, so
# the script works on a fresh checkout right after fetch_deps.sh — no manual
# `cmake ..` required.
configure_cmake() {
    cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        $( [ "$BUILD_TYPE" = "Debug" ] && echo "-DMCU_DEBUG_LOG=ON" || echo "-DMCU_DEBUG_LOG=OFF" ) \
        $( [ "$KLINKE" = 1 ] && echo "-DMCU_KLINKE_BUILD=ON" || echo "-DMCU_KLINKE_BUILD=OFF" )
}

NEED_CONFIGURE=0
if [ "$QUICK" = false ]; then
    NEED_CONFIGURE=1
elif [ ! -f CMakeCache.txt ]; then
    echo "  (no CMakeCache.txt — configuring before the first build)"
    NEED_CONFIGURE=1
else
    CACHED_TYPE="$(grep -E '^CMAKE_BUILD_TYPE:' CMakeCache.txt | head -1 | cut -d= -f2)"
    if [ "$CACHED_TYPE" != "$BUILD_TYPE" ]; then
        echo "  (cached build type '$CACHED_TYPE' != requested '$BUILD_TYPE' — reconfiguring)"
        NEED_CONFIGURE=1
    fi
fi
if [ "$NEED_CONFIGURE" = 1 ]; then
    configure_cmake
else
    echo "  (incremental — skipping cmake configure)"
fi
cmake --build . -- -j"$JOBS"

# --- Archive a copy in dist/ -------------------------------------------------
# Same convention as scripts/build-portable-linux.sh: always keep a copy of the
# freshly built binary in dist/ (Linux .so, macOS .dylib).
mkdir -p "$SCRIPT_DIR/dist"
cp -f "$BUILD_DIR/$ARTIFACT" "$SCRIPT_DIR/dist/$ARTIFACT"
echo "=== copied $ARTIFACT → $SCRIPT_DIR/dist/ ==="

# --- Deploy -----------------------------------------------------------------

if [ "$DEPLOY" = 0 ]; then
    echo ""
    echo "=== Build complete (deployment and REAPER launch skipped) ==="
    exit 0
fi

echo ""
echo "=== Deploying $ARTIFACT → $PLUGIN_DIR ==="
cp "$BUILD_DIR/$ARTIFACT" "$PLUGIN_DIR/$ARTIFACT"
echo "Done."
echo ""

# --- Run Reaper -------------------------------------------------------------

VERSION_STR="$(grep MCU_VERSION_STRING "$BUILD_DIR/Version.h" 2>/dev/null | cut -d'"' -f2 || echo "?")"

echo "=== Launching Reaper ==="
echo "  Binary:  $REAPER_EXE"
echo "  Plugin:  $ARTIFACT build $VERSION_STR"
echo "  Audio:   $AUDIO_INFO"
echo "  MIDI:    $MIDI_INFO"
echo "  Close Reaper to stop this script."
echo "----------------------------------------"

case "$LAUNCH_METHOD" in
    direct)
        # Linux: run the binary directly; Reaper's stdout goes to the terminal.
        # GDK_BACKEND=x11 forces X11 (not Wayland) for JUCE's native window.
        env $REAPER_ENV "$REAPER_EXE" &
        REAPER_PID=$!
        wait $REAPER_PID 2>/dev/null
        ;;
    open)
        # macOS: `open -a REAPER` launches the app bundle and returns
        # immediately — we wait on the process that matches the REAPER binary
        # name. This catches both an already-running REAPER (user is reminded
        # to restart) and a freshly started one.
        open -a REAPER &
        sleep 1
        # wait for the REAPER process (allows cmd-Q to also exit this script)
        REAPER_PID=$(pgrep -n -f "/Applications/REAPER.app/Contents/MacOS/REAPER" 2>/dev/null || true)
        if [ -n "$REAPER_PID" ]; then
            wait "$REAPER_PID" 2>/dev/null
        fi
        ;;
esac
