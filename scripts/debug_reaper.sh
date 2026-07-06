#!/bin/bash
# debug_reaper.sh — Launch Reaper under gdb, auto-save crash backtrace
set -e

REAPER_DIR="$HOME/opt/REAPER"
LOGFILE="$HOME/reaper_crash.log"

cd "$REAPER_DIR"
ulimit -c unlimited 2>/dev/null || true

echo "Starting Reaper under gdb..."
echo "Crash backtrace will be saved to: $LOGFILE"
echo "----------------------------------------"

gdb -batch \
    -ex "set environment GDK_BACKEND=x11" \
    -ex "run" \
    -ex "thread apply all bt" \
    -ex "quit" \
    --args ./reaper 2>&1 | tee "$LOGFILE"

echo ""
echo "----------------------------------------"
echo "Done. Full output saved to: $LOGFILE"
echo "Copy the FIRST thread's backtrace (the one that crashed)."
