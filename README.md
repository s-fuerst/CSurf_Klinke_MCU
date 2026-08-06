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

### Linux and macOS: recommended build, deploy, and run script

From the repository root, use:

```bash
./scripts/build-and-run-linux-macos.sh --release
```

The script configures a Release build, builds it, deploys the extension to
REAPER's `UserPlugins` directory, and starts REAPER. The supported public
options are:

```bash
./scripts/build-and-run-linux-macos.sh            # incremental build, deploy, and run
./scripts/build-and-run-linux-macos.sh --release  # configure + build Release, deploy, and run
./scripts/build-and-run-linux-macos.sh --debug    # configure + build Debug, deploy, and run
./scripts/build-and-run-linux-macos.sh --clean    # remove build/, then configure, build, deploy, and run
./scripts/build-and-run-linux-macos.sh --reconfigure # rerun CMake, then build, deploy, and run
./scripts/build-and-run-linux-macos.sh --no-deploy # build without deploying or starting REAPER
./scripts/build-and-run-linux-macos.sh -j8        # use eight parallel build jobs
./scripts/build-and-run-linux-macos.sh --help     # show usage
```

The plain invocation (no options) builds incrementally and skips the CMake
configure step — unless `build/` does not exist yet or is unconfigured
(e.g. a fresh checkout right after `fetch_deps.sh`), in which case it
configures automatically. No manual `cmake ..` is required. Use
`--reconfigure` or `--clean` to force a fresh configure.

The scripts locate the repository root from their own path, so the current
directory does not matter when the script is invoked with a path. For example,
the commands above are intended for the repository root; from another directory
use an absolute path or a relative path that points to the script. Do not rely
on calling the script by its bare filename through `PATH`.

On Linux, the script starts `$HOME/opt/REAPER/reaper` with `GDK_BACKEND=x11`.
On macOS, it starts the installed REAPER application. Fully quit REAPER before
running the script when testing a newly built extension.

### Linux: manual CMake alternative

```bash
# Build-host packages (Debian/Ubuntu)
sudo apt install build-essential cmake libfreetype-dev libx11-dev \
  libxext-dev libcurl4-openssl-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMCU_DEBUG_LOG=OFF
cmake --build . -- -j$(nproc)
cp reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

For a **portable build** that runs on older distros (glibc ≥ 2.31,
Ubuntu 20.04+), use the container build:

```bash
./scripts/build-portable-linux.sh   # → dist/reaper_csurf_mcu_klinke.so
```

### Windows (MSVC)

From a Developer Command Prompt or PowerShell with the MSVC environment:

```powershell
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMCU_DEBUG_LOG=OFF
cmake --build . --config Release
copy Release\reaper_csurf_mcu_klinke_x64.dll %APPDATA%\REAPER\UserPlugins\
```

To build from WSL, use the native-NTFS mirror script from the repository root:

```bash
./scripts/build-windows-from-wsl.sh --setup       # one-time mirror setup
./scripts/build-windows-from-wsl.sh               # incremental Release build + deploy
./scripts/build-windows-from-wsl.sh --release     # reconfigure + build Release + deploy
./scripts/build-windows-from-wsl.sh --debug       # reconfigure + build Debug + deploy
./scripts/build-windows-from-wsl.sh --clean       # remove build_win/, then build Release + deploy
./scripts/build-windows-from-wsl.sh --reconfigure # rerun CMake, then build + deploy
./scripts/build-windows-from-wsl.sh --no-deploy   # build without copying to REAPER
./scripts/build-windows-from-wsl.sh -j8           # use eight parallel build jobs
./scripts/build-windows-from-wsl.sh --help        # show usage
```

Ninja uses one configuration per `build_win/` directory. When changing between
Release and Debug, add `--clean` to the command. As with the Linux/macOS
script, the current directory is not significant when the script is invoked
with a path.

### macOS: manual CMake alternative

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMCU_DEBUG_LOG=OFF
cmake --build . -- -j$(sysctl -n hw.ncpu)
cp reaper_csurf_mcu_klinke.dylib ~/Library/Application\ Support/REAPER/UserPlugins/
```

> Minimum macOS version: 10.15 (Catalina) — required by JUCE 8.

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
