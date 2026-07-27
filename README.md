# CSurf_Klinke_MCU — Mackie Control Universal Extension for Reaper

A [Reaper](https://www.reaper.fm/) **control surface extension** that adds
enhanced support for hardware controllers speaking the **Mackie Control
Universal (MCU)** MIDI protocol — the original Mackie Control as well as
compatible controllers (Behringer X‑Touch, X32, QCon, iCON, etc.).

Once installed, the surface appears as **"Mackie Control Protocol (Klinke)"**
in Reaper → Preferences → Control/OSC/web.

## Features

- **Multi-unit support** — up to 8 MCU units (main + extenders, Mackie or
  QCon ProX) with independent MIDI ports and per‑unit options.
- **Mixer mode** — channel strips with faders, pan, mute/solo/rec arm,
  select, folder navigation, and meter bridge.
- **FX mode** — map plugin parameters to faders and V‑Pots, with banks,
  pages, parameter maps, and per‑plugin presets.
- **Action mode** — map any Reaper action to the 8 V‑Pots (×6 CCs each,
  ×2 with Shift, ×8 banks = 768 assignable slots).
- **Send/Receive mode** — adjust track sends and receives per channel.
- **Pan mode** — quick pan control per channel.
- **Transport** — play/stop/record/rewind/FFWD, markers, loop, click,
  and automation mode switching.
- **Region store/recall** — store loop and time selections as Reaper
  regions.

## Platforms

| Platform | Status |
|----------|--------|
| **Linux**   | CMake build (baseline) |
| **Windows** | CMake build with MSVC, native Win32 dialogs |
| **macOS**   | CMake build with Clang, Cocoa/Metal |

The extension is a single `.so` / `.dll` / `.dylib` file deployed to
Reaper's `UserPlugins` directory.

## Building

### Prerequisites

All dependencies live at the repo root. Fetch them once:

```bash
./scripts/fetch_deps.sh
```

This pulls:

| Dependency | Version | Type |
|------------|---------|------|
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.14 | GUI framework (modules via `add_subdirectory`) |
| [Boost](https://www.boost.org/) | 1.91.0 | Header-only (smart pointers, signals2) |
| [REAPER SDK](https://www.reaper.fm/sdk/plugin/plugin.php) | pinned in repo | Plugin API + WDL/SWELL |

### Linux

```bash
# Build-host packages (Debian/Ubuntu)
sudo apt install build-essential cmake libfreetype-dev libx11-dev \
  libxext-dev libcurl4-openssl-dev

# Configure + build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)

# Deploy
cp reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

For a **portable build** that runs on older distros (glibc ≥ 2.31,
Ubuntu 20.04+), use the container build:

```bash
./scripts/build-portable-linux.sh   # → dist/reaper_csurf_mcu_klinke.so
```

### Windows (MSVC)

From a Developer Command Prompt or PowerShell with MSVC environment:

```powershell
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Deploy
copy build\Release\reaper_csurf_mcu_klinke_x64.dll %APPDATA%\REAPER\UserPlugins\
```

To build from WSL, use the fast mirror script:

```bash
./scripts/build-windows-fast.sh --setup   # one-time
./scripts/build-windows-fast.sh           # incremental build + deploy
```

### macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(sysctl -n hw.ncpu)

# Deploy
cp reaper_csurf_mcu_klinke.dylib ~/Library/Application\ Support/REAPER/UserPlugins/
```

> Minimum macOS version: 10.15 (Catalina) — required by JUCE 8.

### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `MCU_DEBUG_LOG` | `ON` | Compile-in debug logging (disable for release builds) |
| `MCU_KLINKE_BUILD` | `OFF` | Private KLINKE preprocessor flag |

## License

CSurf_Klinke_MCU is free software licensed under the
[GNU General Public License v3.0](gplv3.txt).

JUCE 8 is dual‑licensed (AGPLv3 / commercial). This project uses the
AGPLv3‑compatible path.

## References

- [Reaper](https://www.reaper.fm/)
- [Reaper Extension SDK](https://github.com/justinfrankel/reaper-sdk)
- [JUCE 8](https://github.com/juce-framework/JUCE)
- [Boost](https://www.boost.org/)

### Similar projects

- [CSI (Control Surface Integrator)](https://github.com/reaper-csi/reaper_csurf_integrator)
- [DrivenByMoss](https://github.com/git-moss/DrivenByMoss)
