#!/bin/bash
# start_reaper.sh — Launch Reaper with JACK audio + MIDI bridge
# Starts a2jmidid, launches Reaper, cleans up on exit.

set -e

A2J_PID=""
REAPER_DIR="$HOME/opt/REAPER"

cleanup() {
    echo ""
    echo "Shutting down..."
    if [ -n "$A2J_PID" ] && kill -0 "$A2J_PID" 2>/dev/null; then
        echo "Stopping a2jmidid (PID $A2J_PID)..."
        kill "$A2J_PID" 2>/dev/null
        wait "$A2J_PID" 2>/dev/null
        echo "a2jmidid stopped."
    fi
    exit 0
}

trap cleanup INT TERM EXIT

# Check if another a2jmidid is already running
if pgrep -x a2jmidid > /dev/null; then
    echo "a2jmidid is already running (PID $(pgrep -x a2jmidid))"
else
    echo "Starting a2jmidid..."
    a2jmidid -e &
    A2J_PID=$!
    sleep 1
    if ! kill -0 "$A2J_PID" 2>/dev/null; then
        echo "ERROR: a2jmidid failed to start!"
        exit 1
    fi
    echo "a2jmidid running (PID $A2J_PID)"
    echo "  ALSA MIDI ports bridged to JACK:"
    aconnect -l 2>/dev/null | grep -E 'client.*QCON|client.*Pro X' || true
fi

echo ""
echo "Launching Reaper..."
echo "  Audio: JACK → PipeWire → Focusrite Saffire Pro 40"
echo "  MIDI:  a2jmidid bridge active"
echo "----------------------------------------"

cd "$REAPER_DIR"
GDK_BACKEND=x11 ./reaper &

REAPER_PID=$!
echo "Reaper PID: $REAPER_PID"
echo "Close Reaper to stop this script (auto-cleans a2jmidid)."

# Wait for Reaper to exit
wait $REAPER_PID 2>/dev/null

# Cleanup happens via trap EXIT
A2J_PID=$(pgrep -x a2jmidid || true)
