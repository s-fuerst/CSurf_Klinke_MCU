#!/usr/bin/env bash
#
# scripts/build-portable-linux.sh
#
# Build a MAXIMALLY PORTABLE reaper_csurf_mcu_klinke.so for Linux inside a
# Debian 11 container (glibc 2.31) with a statically-linked C++ runtime.
# Output: dist/reaper_csurf_mcu_klinke.so
#
# The resulting binary runs on essentially every current desktop Linux distro
# (glibc >= 2.31): Ubuntu 20.04+, Debian 11+, Fedora, Arch, RHEL/Rocky 9+.
# (RHEL/Rocky 8 = glibc 2.28 is the one gap.)
#
# Requires podman (rootless, no daemon) OR docker. No sudo needed with podman.
#
# See docker/release-linux.Dockerfile for the full rationale.

set -euo pipefail

# Resolve repo root from this script's location (works from any CWD).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

# Prefer podman (rootless, no daemon); fall back to docker.
if command -v podman >/dev/null 2>&1; then
    RT=podman
elif command -v docker >/dev/null 2>&1; then
    RT=docker
else
    echo "error: neither podman nor docker found in PATH" >&2
    exit 1
fi

echo "==> runtime: $RT"
echo "==> building portable .so (Debian 11 base, static C++ runtime)..."

mkdir -p dist

# docker needs BuildKit for --output; podman supports it natively.
if [ "$RT" = "docker" ]; then
    export DOCKER_BUILDKIT=1
fi

"$RT" build \
    --target export \
    --output type=local,dest=dist \
    -f docker/release-linux.Dockerfile \
    .

SO="dist/reaper_csurf_mcu_klinke.so"
echo
echo "==> built: $SO  ($(du -h "$SO" | cut -f1))"
echo
echo "    deploy:"
echo "      cp \"$SO\" ~/.config/REAPER/UserPlugins/"
echo "    (fully restart REAPER to reload the extension)"
