#!/bin/bash
# build_and_run.sh — Build the extension, deploy it, launch Reaper
#
# Works on Linux (GCC, X11, JACK) and macOS (Apple Clang, CoreAudio/MIDI).
#
# Usage:
#   ./build_and_run.sh              # quick (incremental) build + deploy + run
#   ./build_and_run.sh --release    # full configure + build (Release) + deploy + run
#   ./build_and_run.sh --debug      # full configure + build (Debug) + deploy + run
#   ./build_and_run.sh --klinke     # full configure + build (Release) with KLINKE features
#   ./build_and_run.sh --help       # show this message

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="Release"
QUICK=true
KLINKE=0

# --- Options ----------------------------------------------------------------

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    sed -n '2,13p' "$0"
    exit 0
fi
if [ "$1" = "--debug" ]; then       BUILD_TYPE="Debug";  QUICK=false;  shift; fi
if [ "$1" = "--release" ]; then     BUILD_TYPE="Release"; QUICK=false;  shift; fi
if [ "$1" = "--klinke" ]; then      KLINKE=1;            QUICK=false;  shift; fi

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

# --- Build ------------------------------------------------------------------

echo ""
echo "=== Building $BUILD_TYPE ($UNAME) ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$QUICK" = true ]; then
    echo "  (incremental — skipping cmake configure)"
else
    cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        $( [ "$BUILD_TYPE" = "Debug" ] && echo "-DMCU_DEBUG_LOG=ON" || echo "-DMCU_DEBUG_LOG=OFF" ) \
        $( [ "$KLINKE" = 1 ] && echo "-DMCU_KLINKE_BUILD=ON" || echo "-DMCU_KLINKE_BUILD=OFF" )
fi
cmake --build . -- -j"$JOBS"

# --- Deploy -----------------------------------------------------------------

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
