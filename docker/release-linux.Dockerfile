# docker/release-linux.Dockerfile
#
# Build a MAXIMALLY PORTABLE reaper_csurf_mcu_klinke.so for Linux.
#
# Why this exists:
#   A plugin built on a modern distro (e.g. Arch/CachyOS, glibc 2.43) embeds
#   high GLIBC symbol versions and will NOT load on Ubuntu LTS / Debian stable
#   / Fedora. glibc is forward-compatible only ("build on old, run on new").
#
# Strategy (two levers, both applied):
#   1. Build on an OLD glibc base (Debian 11 "bullseye" = glibc 2.31, gcc 10).
#      This pins the binary's GLIBC symbol requirement at <= 2.31.
#   2. Statically link the C++ runtime (-static-libstdc++ -static-libgcc) so
#      libstdc++.so / libgcc_s.so are NOT a runtime dependency.
#
#   Result: the .so runs on glibc >= 2.31 systems -> Ubuntu 20.04+, Debian 11+,
#   Fedora, Arch, RHEL/Rocky 9+. (RHEL/Rocky 8 = glibc 2.28 is the one gap.)
#
#   The GUI deps (libX11.so.6, libfreetype.so.6, libcurl.so.4,
#   libfontconfig.so.1) stay dynamic on purpose: their SONAMEs are stable for
#   years and every desktop Linux running Reaper already has them.
#
# CMake: JUCE 8 requires CMake >= 3.22, but Debian 11 ships 3.18. We install a
# recent CMake from the official cmake.org binary tarball (broad glibc compat,
# no apt-repo change).
#
# Usage (from repo root):
#   scripts/build-portable-linux.sh
#   # -> dist/reaper_csurf_mcu_klinke.so
#
# Or directly:
#   podman build --target export --output type=local,dest=dist \
#       -f docker/release-linux.Dockerfile .
#
# Works with podman (rootless, no daemon) or docker.

# ── builder ─────────────────────────────────────────────────────────────────
FROM debian:11-slim AS builder

# Debian 11 (bullseye): glibc 2.31, gcc 10.2 (full C++17). Build deps per
# fetch_deps.sh / AGENTS.md. g++ pulls libstdc++-10-dev, which ships the static
# libstdc++.a compiled with -fPIC -- exactly what -static-libstdc++ needs to
# link the C++ runtime into a shared library.
# Build deps per fetch_deps.sh / AGENTS.md, PLUS the packages JUCE 8 discovers
# via pkg-config for juce_gui_basics + juce_graphics on Linux (freetype2,
# x11, xext, xcursor, xinerama, xrandr, xrender, xcomposite, GL). Debian 11
# slim ships none of these by default; the CachyOS host had them all.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        git \
        curl \
        ca-certificates \
        tar \
        xz-utils \
        libfreetype6-dev \
        libfontconfig1-dev \
        libx11-dev \
        libxext-dev \
        libxcursor-dev \
        libxinerama-dev \
        libxrandr-dev \
        libxrender-dev \
        libxcomposite-dev \
        libglu1-mesa-dev \
        mesa-common-dev \
        libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Recent CMake (JUCE 8 needs >= 3.22; Debian 11 only has 3.18). The cmake.org
# Linux x86_64 binaries are built for broad glibc compatibility.
ARG CMAKE_VERSION=3.28.3
RUN curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
        | tar xz -C /opt \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cmake" /usr/local/bin/cmake \
    && ln -s "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/ctest"  /usr/local/bin/ctest
RUN cmake --version

# FreeType headers live under /usr/include/freetype2/ (not /usr/include/), so a
# plain `#include <ft2build.h>` needs -I/usr/include/freetype2. JUCE's pkg-config
# target supplies that for the modules we build -- BUT JUCE builds its `juceaide`
# codegen helper by RE-INVOKING CMake as a subprocess whose passthrough args do
# NOT include CMAKE_C[XX]_FLAGS, so juceaide never gets the -I and its compile
# of juce_graphics.cpp fails on `ft2build.h`. Making the headers resolvable from
# the default include path (symlinks) fixes juceaide without touching JUCE or
# the project. ft2build.h also pulls <freetype/config/ftheader.h>, hence both
# links. This is the standard JUCE-on-Debian/Ubuntu Docker workaround.
RUN ln -sf /usr/include/freetype2/ft2build.h /usr/include/ft2build.h \
 && ln -sfn /usr/include/freetype2/freetype     /usr/include/freetype

WORKDIR /repo

# Fetch the pinned deps first (cached layer; only re-runs when fetch_deps.sh
# changes). .dockerignore keeps host deps/build artifacts OUT of the context,
# so the image always fetches from upstream -- reproducible.
COPY fetch_deps.sh ./
RUN ./fetch_deps.sh

# Now the rest of the source (layer invalidates on source changes only).
COPY . .

# Build with a static C++ runtime. -static-libstdc++ + -static-libgcc remove
# the libstdc++.so / libgcc_s.so runtime dependencies; building on Debian 11
# pins the glibc symbol requirement to <= 2.31.
# MCU_DEBUG_LOG=OFF: release/distribution builds should not spam the debug log.
RUN mkdir -p build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SHARED_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
        -DMCU_DEBUG_LOG=OFF && \
    cmake --build . -j"$(nproc)"

# ── export stage (emits just the .so) ───────────────────────────────────────
# `podman build --target export --output type=local,dest=dist` writes the .so
# straight to ./dist/ -- no `podman run` step needed.
FROM scratch AS export
COPY --from=builder /repo/build/reaper_csurf_mcu_klinke.so /reaper_csurf_mcu_klinke.so
