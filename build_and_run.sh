#!/bin/bash
# build_and_run.sh — Build the extension, deploy it, launch Reaper
#
# Usage:
#   ./build_and_run.sh              # full configure + build + deploy + run
#   ./build_and_run.sh --quick      # incremental build (skip cmake) + deploy + run
#   ./build_and_run.sh --debug      # full configure + build (Debug) + deploy + run
#   ./build_and_run.sh --help       # show this message

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REAPER_DIR="$HOME/opt/REAPER"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="Release"
QUICK=false

# --- Options ----------------------------------------------------------------

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    sed -n '2,8p' "$0"
    exit 0
fi
if [ "$1" = "--debug" ]; then       BUILD_TYPE="Debug";  QUICK=false;  shift; fi
if [ "$1" = "--quick" ]; then       QUICK=true;  shift; fi

# --- Platform detection -----------------------------------------------------

case "$(uname -s)" in
    Linux)   ARTIFACT="reaper_csurf_mcu_klinke.so" ;;
    Darwin)  ARTIFACT="reaper_csurf_mcu_klinke.dylib" ;;
    *)       echo "Unsupported platform: $(uname -s)"; exit 1 ;;
esac

case "$(uname -s)" in
    Linux)   PLUGIN_DIR="$HOME/.config/REAPER/UserPlugins" ;;
    Darwin)  PLUGIN_DIR="$HOME/Library/Application Support/REAPER/UserPlugins" ;;
esac

# --- Build ------------------------------------------------------------------

echo "=== Building $BUILD_TYPE ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$QUICK" = true ]; then
    echo "  (incremental — skipping cmake configure)"
else
    cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        $( [ "$BUILD_TYPE" = "Debug" ] && echo "-DMCU_DEBUG_LOG=ON" || echo "-DMCU_DEBUG_LOG=OFF" )
fi
cmake --build . -- -j"$(nproc)"

# --- Deploy -----------------------------------------------------------------

echo ""
echo "=== Deploying $ARTIFACT → $PLUGIN_DIR ==="
cp "$BUILD_DIR/$ARTIFACT" "$PLUGIN_DIR/$ARTIFACT"
echo "Done."
echo ""

# --- Run Reaper -------------------------------------------------------------

echo "=== Launching Reaper ==="
echo "  Binary:  $REAPER_DIR/reaper"
echo "  Plugin:  $ARTIFACT build $(grep MCU_VERSION_STRING "$BUILD_DIR/Version.h" 2>/dev/null | cut -d'"' -f2 || echo "?")"
echo "  Audio:   JACK → PipeWire → Focusrite Saffire Pro 40"
echo "  MIDI:    direct (no a2jmidid)"
echo "  Close Reaper to stop this script."
echo "----------------------------------------"

cd "$REAPER_DIR"
GDK_BACKEND=x11 ./reaper &
REAPER_PID=$!
wait $REAPER_PID 2>/dev/null
