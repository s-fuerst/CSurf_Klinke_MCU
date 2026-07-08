# AGENTS.md — Guide for Contributors & AI Agents

> Read this first. It orients you to what this project is, how it fits
> together, how to build it, and what the current revival effort is about.

**When you're stuck or are unsure, ask.** You don't have to figure
everything out by yourself — asking questions or directly requesting
help is expected and encouraged. The project is complex and the
revival touches many moving parts; a quick question can save hours of
guesswork.

### ⚠️ Language rule (HIGH PRIORITY — read before writing anything)

Conversations with the maintainer may be in German, but **everything
written into a file MUST be in English — no exceptions.** This covers
all of it:

- **Source code** — identifiers, strings, log messages
- **Comments** — inline (`//`, `/* */`), file headers, TODO/FIXME notes
- **Commit messages** (already required to be English; restated here)
- **Docs, README, AGENTS.md, manual text, MEMD.md entries**
- **Generated code, config files, shell scripts**

Never write German into a file — not even a single comment. When in
doubt, write it in English.

## 1. What this project is

**CSurf_Klinke_MCU** is a [Reaper](https://www.reaper.fm/) *control surface
extension* (a `.dll`/`.so`/`.dylib` loaded by Reaper) that adds enhanced
support for hardware controllers speaking the **Mackie Control Universal
(MCU)** MIDI protocol — the original Mackie Control as well as compatible
controllers (Behringer X32, QCon, iCON, etc.).

It is *not* a standalone app and *not* a VST. Reaper hosts it as a plugin
through its **csurf** (control surface) API. Once loaded, it registers a
control surface called *"Mackie Control Protocol (Klinke)"* that the user
selects in Reaper's *Preferences → Control/OSC/web*.

## 2. Revival context (read this)

The project has been largely dormant. The **goal of the revival is to make
it build and run cross-platform — Windows, macOS, and Linux — while keeping
dependency versions as close to the originals as feasible.** Originally built
against JUCE 1.52, the project was upgraded to **JUCE 8** (module build)
as part of the revival, because JUCE 1.52 could not target Apple Silicon
and modern macOS. Boost was upgraded from 1.39 to **1.91.0** (header-only):
Boost 1.39 (2009) does not compile under modern libc++/C++17 without an
ever-growing pile of patches; 1.91.0 compiles cleanly on all platforms.

> The original build is **Windows + Visual Studio only**. A cross-platform
> **CMake** build has been added (see §4) and is now the source of truth for
> all three platforms. The old VS `.vcxproj` / `.sln` files are archived in
> `archive/vs-legacy/`.

### Prerequisites for the CMake build (exact versions pinned)

All three deps live at the **repo root**:

1. **JUCE 8** (module build) → `juce_8/`
   - Fetched by `./scripts/fetch_deps.sh`: `git clone --branch 8.0.14 https://github.com/juce-framework/JUCE juce_8`
   - The CMake build uses `add_subdirectory(juce_8)` to link `juce::juce_gui_basics`.
2. **Boost 1.91.0** (headers only) → `boost_1_91_0/`
   - `https://archives.boost.io/release/1.91.0/source/boost_1_91_0.tar.bz2`
   - Only headers are used (smart pointers, `signals2`). No compiled libs.
3. **REAPER SDK** (WDL + SWELL + plugin headers) → `reaper-sdk/`
   - From the same Stenzel fork: `cp -r original-klinke/reaper-sdk .`
   - Alt: Cockos — `https://www.reaper.fm/sdk/plugin/plugin.php`
4. **Linux system packages** (build host):
   `sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev libcurl4-openssl-dev`

> The CMake build expects all three deps at the repo root; no environment
> variables are needed.

## 4. How to build

**Status:**
- **Linux** — CMake build working (baseline).
- **Windows** — CMake build working (MSVC, native Win32, res.rc, JUCE 8).
- **macOS** — CMake build working (Clang, Cocoa/Metal, JUCE 8, SWELL).

### Windows (CMake, MSVC)

```powershell
# one-time: fetch the three pinned deps to the repo root
.\scripts\fetch_deps.sh   # Git Bash or WSL

# Build host needs Visual Studio 2019 or later (MSVC toolset v142+)
# and CMake 3.10+.

# configure + build (from Developer Command Prompt or PowerShell with MSVC env)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Output: `build/Release/reaper_csurf_mcu_klinke_x64.dll`. Deploy by copying
to `%APPDATA%\REAPER\UserPlugins\` and adding **"Mackie Control Protocol
(Klinke)"** in Reaper → Preferences → Control/OSC/web. The Debug config
yields `build/Debug/reaper_csurf_mcu_klinke_x64.dll`.

Key differences from Linux:
- **Native Win32 dialogs**: the surface-edit dialog is defined in `res.rc`
  and compiled by the MSVC resource compiler. SWELL is not used on Windows;
  `SWELL_PROVIDED_BY_APP` is not defined, and `swell-modstub-generic.cpp` /
  `res_linux.cpp` are excluded.
- **winmm.lib**: linked explicitly for `timeGetTime()` (used in
  `csurf_mcu.cpp`, `Transport.cpp`, `ButtonManager.cpp`).
- **Output name**: follows the .vcxproj convention — `reaper_csurf_mcu_klinke_x64.dll`.

#### Building from WSL (the Windows host, driven from the Linux shell)

If you develop inside WSL on a Windows machine, you do not need to leave the
WSL shell to produce the Windows `.dll`. `scripts/build-windows.sh` drives the
native MSVC toolchain from WSL and copies the result into REAPER's
`UserPlugins`:

```bash
./scripts/fetch_deps.sh                        # one-time (see CRLF note below)
scripts/build-windows.sh               # incremental: build + deploy (configure skipped after first run)
scripts/build-windows.sh --clean       # wipe build_win/, configure + build from scratch
scripts/build-windows.sh --reconfigure # re-run CMake (after CMakeLists.txt / source-list edits), then build
scripts/build-windows.sh --debug       # Debug config -> build_win/Debug/
scripts/build-windows.sh --no-deploy   # build only, do not copy to UserPlugins
```

**Incremental speed caveat**: with no flags the script is build-only (fast to
launch), but an in-place incremental still takes ~45s because MSBuild rebuilds
all sources on every run -- its up-to-date check fails over the
`\\wsl.localhost`/9P source mount (it stats thousands of JUCE/Boost headers
and a jittery mtime always wins). That is an inherent cost of building over
the WSL->Windows bridge, not a bug; Linux reaches ~15s thanks to native ext4
+ Ninja. A `/mnt/c` build mode (rsync deps once + source per build onto native
NTFS) would give Linux-like incremental speed; not yet implemented.

How it works (the non-obvious bits, so they are not re-debugged):
- **Interop**: the script locates Visual Studio + `vcvars64.bat` via
  `vswhere.exe`, then runs a generated `.bat` through `cmd.exe` (invoked from
  `/mnt/c` so cmd's current directory is a real drive, not a UNC path).
- **UNC -> drive letter**: cmd.exe reaches the WSL repo via
  `pushd \\wsl.localhost\<distro>\...`, which maps the UNC path to a temporary
  drive letter (e.g. `Z:`). This is what lets MSVC / `rc.exe` / juceaide read
  the source straight off the WSL filesystem -- no source copy to `/mnt/c`.
- **Visual Studio generator, not Ninja**: Ninja's stat-based dependency model
  breaks on the `\\wsl.localhost`/9P mount (CMake's try-compile source shows up
  as "missing"). The VS generator uses MSBuild, which tolerates the mapped
  drive. The generator is auto-picked from the installed VS major version
  (override with `MCU_VS_GENERATOR='Visual Studio 17 2022'`).
- **Portable CMake >= 3.22**: JUCE 8 requires CMake 3.22, but VS2019 bundles
  only 3.20. The script downloads a portable CMake 3.31.6 once into
  `~/.cache/csurf-klinke-mcu/` and reuses it.
- **`CMAKE_SUPPRESS_REGENERATION=ON`**: silences the `MSB8064`/`MSB8065`
  dependency-tracking warnings MSBuild emits on the UNC mount (the regen
  step's dependency tracking is unreliable over 9P). The script always runs
  configure before build, so disabling auto-regeneration costs nothing.

Output: `build_win/Release/reaper_csurf_mcu_klinke_x64.dll`. The script
auto-detects `%APPDATA%\REAPER\UserPlugins\` (under `/mnt/c/Users/*`) and
copies the `.dll` there unless `--no-deploy` is given (set `MCU_USERPLUGINS=`
to override the destination). Fully restart REAPER to load it.

Requirements: Visual Studio 2019+ (Community or Build Tools) with the
**MSVC v142/v143 x64** and **Windows 10/11 SDK** components, `vswhere.exe`
(ships with the VS installer), and internet access on first run (the
portable-CMake download is ~50 MB).

> **CRLF note**: the repo is checked out with `core.autocrlf=true`, so shell
> scripts are CRLF and `bash` will refuse them with `bash\r: No such file or
> directory`. If that happens, strip once: `sed -i 's/\r$//' scripts/fetch_deps.sh`.
`scripts/build-windows.sh` itself is committed LF.

### Linux (CMake, baseline)

```bash
# one-time: fetch the three pinned deps to the repo root
./scripts/fetch_deps.sh

# build-host packages (libcurl is new for JUCE 8)
sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev libcurl4-openssl-dev

# configure + build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(nproc)"
```

Output: `build/reaper_csurf_mcu_klinke.so`. Deploy by copying it to
`~/.config/REAPER/UserPlugins/` and adding **"Mackie Control Protocol
(Klinke)"** in Reaper → Preferences → Control/OSC/web. Pass
`-DMCU_DEBUG_LOG=OFF` to disable the debug log (on by default).

#### Portable Linux build (the distribution artifact)

A `.so` built on a modern distro (e.g. Arch/CachyOS, glibc 2.43) embeds high
GLIBC symbol versions and will **not** load on Ubuntu LTS / Debian stable /
Fedora — glibc is forward-compatible only ("build on old, run on new"). The
native build above is for **local development**; for a **download artifact**
that runs on essentially every current desktop Linux, build inside a container
instead:

```bash
./scripts/build-portable-linux.sh     # → dist/reaper_csurf_mcu_klinke.so
```

What it does (`docker/release-linux.Dockerfile`):
- Builds on **Debian 11 (bullseye)** → pins GLIBC requirement at ≤ 2.31.
- Statically links the C++ runtime (`-static-libstdc++ -static-libgcc`) →
  `libstdc++.so` / `libgcc_s.so` are **not** runtime dependencies.
- Installs a current CMake from cmake.org (JUCE 8 needs ≥ 3.22; Debian 11
  ships 3.18).
- `MCU_DEBUG_LOG=OFF` (a release artifact should not spam the log).
- Multi-stage build with a `scratch` export stage that emits just the `.so`
  into `dist/`.

Uses rootless **podman** (preferred — no daemon, no sudo), falls back to
**docker**. Result: an 11 MB `.so` whose direct dependencies are only
`libcurl`, `libfontconfig`, `libfreetype`, `libpthread`, `libdl`, `libm`,
`libc` — all of which ship on every desktop Linux running Reaper.

**Coverage** (glibc ≥ 2.30): Ubuntu 20.04+, Debian 11+, Fedora 31+, Arch,
openSUSE Leap 15.4+, RHEL/Rocky/Alma 9+. The one gap is **RHEL/Rocky/Alma 8**
(glibc 2.28) — an aging server/workstation distro rarely used for Reaper.

**X11 note:** JUCE loads X11 dynamically at runtime (`dlopen`), so `libX11`
is *not* a hard link-time dependency. The native build happens to list it in
`NEEDED` only because CachyOS' gcc does not default to `-Wl,--as-needed`;
Debian's gcc does, so the container build drops the unused entry. Both
builds are symbol-identical regarding X11 and behave the same at runtime.

**Known container build quirks** (documented so they are not re-debugged):
- JUCE builds its `juceaide` codegen helper by **re-invoking CMake as a
  subprocess** whose passthrough args do **not** include `CMAKE_C[XX]_FLAGS`,
  so forcing `-I/usr/include/freetype2` via the outer flags does **not**
  reach juceaide.
- juceaide compiles `juce_graphics.cpp`, which needs `<ft2build.h>` (a flat
  header living under `/usr/include/freetype2/`) and `<fontconfig/fontconfig.h>`.
  Fix: symlink `/usr/include/ft2build.h` and `/usr/include/freetype` into the
  default include path, and install `libfontconfig1-dev`. (Standard
  JUCE-on-Debian/Ubuntu Docker workaround.)

#### Versioning (VERSION file + build counter)

The version string baked into the surface (shown in Reaper's surface list as
*Mackie Control Protocol (Klinke v… build …)*) comes from a **`VERSION`**
file at the repo root, not from a hard-coded constant:

```
# Format: <version> <build-count>
0.9.1.3 0
```

- **`<version>`** (e.g. `0.9.1.3`) is bumped **manually** for a release —
  edit the `VERSION` file (e.g. → `0.9.2.0`). When you bump the version,
  reset the build-count to `1` (or `0`).
- **`<build-count>`** is **auto-incremented** on every `cmake` configure and
  compiled into the binary as `… build N`. The number stored in the file is
  the build number of the **last** build produced.

At configure time CMake reads `VERSION`, bumps the count, rewrites `VERSION`,
and generates `build/Version.h` (`#define MCU_VERSION_STRING "v0.9.1.3 build N"`),
which `csurf_mcu.h` includes and uses in `CSurf_MCU::GetDescString()`.

> **Do not run `cmake ..` without building afterwards** — it bumps the counter
> without producing a binary. The build flow is always `cmake .. && cmake --build`.

#### Deploying after a build (agent responsibility)

There is **no auto-deploy step in CMake** (so CI, foreign machines, and the
future Windows/macOS branches are not surprised). Instead, **the agent copies
the freshly built artifact into the Reaper plugin directory after every
successful build**. The agent knows the host platform (it ran the
build), so on Linux it runs:

```bash
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

On macOS it would run:

```bash
cp build/reaper_csurf_mcu_klinke.dylib ~/Library/Application\ Support/REAPER/UserPlugins/
```

On Windows:

```powershell
copy build\Release\reaper_csurf_mcu_klinke_x64.dll %APPDATA%\REAPER\UserPlugins\
```

Reaper must be **fully restarted** (not just reloaded) to pick up the new
`.so`/`.dll`/`.dylib`.

The CMake build uses **SWELL** (WDL) for the surface-edit dialog on Linux
(`res_linux.cpp` + `res.rc_mac_dlg`, generated from `res.rc` via
`WDL/swell/swell_resgen.pl`).

### macOS (CMake, Clang — working)

```bash
# one-time: fetch the three pinned deps to the repo root
./scripts/fetch_deps.sh

# Build host needs Xcode Command Line Tools (or full Xcode) and cmake.
# No additional system packages needed — JUCE 8 links macOS frameworks
# (Cocoa, IOKit, Metal, QuartzCore, etc.) automatically via module metadata.

# configure + build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(sysctl -n hw.ncpu)"
```

Output: `build/reaper_csurf_mcu_klinke.dylib`. Deploy by copying to
`~/Library/Application Support/REAPER/UserPlugins/` and adding
**"Mackie Control Protocol (Klinke)"** in Reaper → Preferences →
Control/OSC/web. Minimum macOS version: 10.15 (Catalina) — JUCE 8 requires it.

Key differences from Linux:
- **SWELL**: uses the `SWELL_dllMain` export (not `dlopen`/`libSwell.so`).
  REAPER calls `SWELL_dllMain` at plugin load time with a `_GetFunc` pointer.
- **Dialog resources**: SWELL's default FLIPPED|NOAUTOSIZE autogen flags are
  correct on macOS; no style/scaling overrides needed (unlike Linux).
- **Frameworks**: all linked automatically by `juce::juce_gui_basics`.

## 3. Tech stack & dependencies

| Dependency               | Env var                | Required version                                     | Notes                                                                                                                                                                                           |
|--------------------------+------------------------+------------------------------------------------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Reaper Extension SDK** | `REAPER_EXTENSION_SDK` | matches `reaper_plugin_functions.h` (pinned in repo) | Provides `csurf.h`, `reaper_plugin_functions.h`, `ptrlist.h`, `IReaperControlSurface`, `reaper_csurf_reg_t`, `REAPER_PLUGIN_ENTRYPOINT`.                                                        |
| **JUCE**                 | `JUCE_DIR`             | **8.0.14** (modules via `add_subdirectory`)          | GUI framework for all editor dialogs/components. Now licensed AGPLv3/JUCE dual. Modules: `juce_gui_basics` pulls `juce_core`/`juce_events`/`juce_graphics`/`juce_data_structures` transitively. |
| **Boost**                | `BOOST`                | **1.91.0** (header-only)                             | Mainly `boost/signals2.hpp`. No compiled libs needed. Upgraded from 1.39 (which did not compile under modern libc++/C++17).                                                                                                                                       |

The per-platform build instructions are in §4. No environment variables are
required for the CMake build — it finds all three deps at the repo root.

### `KlinkeLookAndFeel.h` (JUCE 8 specific)
- `KlinkeLookAndFeel.h` — minimal `LookAndFeel_V4` subclass that forces
  readable text and checkbox tick colours on JUCE 8 dialogs.
  Applied per-window in `CCSModesEditor::setMainComponent()`.
  Global `LookAndFeel::setColour()` is avoided — it breaks dialog interactivity.

## 5. Architecture & code map

### Plugin lifecycle & event flow

All sources live under `src/` in the tree described in §8.

```
reaper loads the .dll/.so/.dylib
  → src/csurf_main.cpp: REAPER_PLUGIN_ENTRYPOINT
      registers csurf_mcu_modified_reg  ("csurf")
  → CSurf_MCU  (src/core/csurf_mcu.cpp/.h)  implements IReaperControlSurface
      owns: Transport, DisplayHandler, Display(s), Region, CCSManager
      handles: MIDI in/out to the MCU, per-frame updates, config, surface-edit dialog
  → CCSManager (src/core/CCSManager.cpp/.h)
      routes hardware events (buttons/faders/VPOTs/LEDs) to the active CCSMode
      manages touch state, LED state, mode switching
  → CCSMode subclasses  (the actual features)
```

### The modes (`CCSMode` subclasses) — the heart of the features
| Mode | Files | What it does |
|---|---|---|
| **MultiTrackMode** | `MultiTrackMode.*`, `MultiTrack*.*`, `MeterBridge*` | Mixer channel strips: faders, VPOT params, select/mute/solo/rec, folders, meters. |
| **PanMode** | `PanMode.*` | Pan control. |
| **PerformanceMode** | `PerformanceMode.*` | **Unimplemented stub.** Constructed and freed by `CCSManager` but its `B_VPOT_INSTRUMENT` binding is **commented out** (`CCSManager::buttonVPOTassign`), so it is never reachable from hardware. Currently only renders the static text "Performance Mode" on the display; the header also contains abandoned wxWidgets threading scaffolding. Intended for a future real-time Reaper performance readout — see §6. |
| **SendMode / ReceiveMode** | `SendReceiveModeBase.*`, `SendMode.*`, `ReceiveMode.*` | Routing/sends & receives per channel. |
| **CommandMode** ("Action Mode") | `CommandMode.*`, `CommandMode*Component.*`, `Actions*` | Maps the 8 VPOTs (×6 CCs each, ×2 with Shift, ×8 banks) to Reaper actions. |
| **PlugMode** ("FX Mode") | `PlugMode.*`, `PlugMode*Component.*`, `PlugAccess.*`, `PlugMap*`, `PlugPresetManager.*`, `PlugWindowManager.*`, `Plugin*Watcher.*` | The largest subsystem: maps plugin/FX parameters to faders & VPOTs, with banks/pages, parameter maps, presets, and an auto-opening FX window watcher. |

### Other key subsystems
- **`src/core/Tracks.*`** — track state tracking (selection, mute/solo, name, level).
- **`src/core/Transport.*`** — play/stop/record/rewind/FFWD, markers, loop.
- **`src/hardware/VPOT_LED.*`** — models the V-Pot LED ring state/mode sent to hardware.
- **`src/hardware/mcu_button_defines.h`** — the MIDI CC ↔ MCU button/VPOT mapping table.
- **`src/hardware/display/Display.*` / `DisplayHandler.*`** — the two 2x55-char MCU displays.
- **`src/ui/CCSModesEditor.*`** + all `*Component.*` files — JUCE-based GUI editors
  for each mode's settings (shown on screen, not on the controller).
- **`src/core/Options.*` / `ProjectConfig.*`** — global options and per-project config
  persistence (saved inside the Reaper project).
- **`src/core/Region.*`** — store loop/time selections as Reaper regions.
- **`resources/res.rc` / `resource.h`** — Windows resources for the surface-edit dialog
  (the Reaper preferences panel for this surface).

### Conventions & gotchas
- **Macros:** `safe_call(p,f)`, `safe_delete(x)`, `safe_delete_array(x)`
  (defined in `csurf_mcu.h`) are used pervasively for NULL-safe calls/deletes.
- **Mode guards:** `CHECKMODE` / `CHECKMODEANDCHANNEL` in mode methods bail
  out if the active mode changed under the caller — always respect these.
- **Sentinel GUIDs:** `GUID_NOT_ACTIVE` (all-zero) and `GUID_MASTER` mark
  "no track" and the master track throughout the plug/track code.
- **1-based channel arrays:** many `[9]` arrays are 1-based; index 0 is the
  master fader. `ASSERT` (in `src/core/McuAssert.h`) guards channel ranges.
- **`EXT_B`** is a *compile-time* switch for the extender variant; build the
  main unit and the B unit from the same source.
- **Version string** comes from the `VERSION.txt` file (repo root) — see §4. The
  build counter auto-increments; bump the version part manually for a release.
  `csurf_mcu.h` uses `MCU_VERSION_STRING` (from generated `Version.h`) in
  `GetDescString()`.
- **No auto-format style is enforced;** match the surrounding file's style
  (roughly 2-space indent, `m_` member prefix, `p` pointer-arg prefix).

## 6. Known issues & open work (from `notes.org`, `whats_new.org`)
- **Distribution:** ReaPack packaging is desired but not done.
- **PerformanceMode is a stub, intentionally kept.** `PerformanceMode.*`
  is allocated/freed by `CCSManager` but never activated — the
  `B_VPOT_INSTRUMENT` → `m_pPerformanceMode` assignment is commented out in
  `CCSManager::buttonVPOTassign()`. The plan is to **implement it later** as
  a real Reaper performance readout (CPU/disk meter, etc. on the MCU
  display). Leave the stub in place; do not remove it or wire up a trivial
  activation that would just show the placeholder text.

## 7. References
- **Reaper Extension / csurf SDK** — https://github.com/justinfrankel/reaper-sdk
  (most relevant: `reaper-sdk/reaper-plugins/reaper_csurf/`)
- **Reaper** — https://www.reaper.fm/
- **Similar projects (good references):**
  - CSI (Control Surface Integrator) — https://github.com/reaper-csi/reaper_csurf_integrator
  - DrivenByMoss — https://github.com/git-moss/DrivenByMoss
- **JUCE 8** — https://github.com/juce-framework/JUCE (tag 8.0.14)

## 8. Repo layout (quick map)

```
# === Build-system files (repo root) ===
CMakeLists.txt Version.h.in VERSION.txt  build config + version counter
AGENTS.md gplv3.txt readme.txt notes.org  docs, license, dev notes

# === External dependencies (fetched by scripts/fetch_deps.sh) ===
juce_8/                 JUCE 8.0.14 (modules, add_subdirectory)
boost_1_91_0/           Boost 1.91.0 (headers only)
reaper-sdk/             REAPER SDK + WDL/SWELL

# === Project source tree ===
src/
├── csurf_main.cpp      REAPER_PLUGIN_ENTRYPOINT
├── JuceHeader.h        JUCE module umbrella include
├── res_linux.cpp       Linux SWELL dialog resources
├── core/               plugin core + config + state
│   ├── csurf_mcu.{cpp,h}    CSurf_MCU — main IReaperControlSurface impl
│   ├── CCSManager.{cpp,h}   mode dispatcher, LED/touch state
│   ├── CCSMode.{cpp,h}      mode base class (CCSMode)
│   ├── Tracks.{cpp,h}       track state (selection, mute/solo, level)
│   ├── Transport.{cpp,h}    play/stop/record/rewind/FFWD, markers, loop
│   ├── Options.{cpp,h}      global options persistence
│   ├── ProjectConfig.{cpp,h} per-project config (saved in .rpp)
│   ├── Region.{cpp,h}       loop/time selection → Reaper regions
│   ├── UndoEnd.{cpp,h}      undo-end sentinel
│   ├── McuAssert.h          ASSERT/ASSERT_M/DBOUT macros
│   ├── McuDebugLog.h        MCU_LOG(…) debug logging (compile-time ON/OFF)
│   └── std_helper.h         erase_if, findByPtr template helpers
├── hardware/           low-level MCU I/O
│   ├── ButtonManager.{cpp,h}  incoming MIDI → button events
│   ├── VPOT_LED.{cpp,h}       V-Pot LED ring state/model
│   ├── mcu_button_defines.h   MIDI CC ↔ MCU button/VPOT mapping table
│   ├── MeterBridge.{cpp,h}    abstract meter-bridge base class
│   └── display/
│       ├── Display.{cpp,h}        per-unit LCD rendering
│       ├── DisplayHandler.{cpp,h} display routing & update logic
│       └── Selector.{cpp,h}       parameter selector ring
├── ui/                 JUCE on-screen editors (shared)
│   ├── CCSModesEditor.{cpp,h}    surface-edit master dialog
│   ├── KlinkeLookAndFeel.h       JUCE 8 colour overrides
│   └── TabbedComponentWithCallback.{cpp,h}
├── action/             Action Mode (VPOTs → Reaper actions)
│   ├── Actions.{cpp,h}
│   ├── ActionsDisplay.{cpp,h}
│   └── editor/
│       └── ActionsDialogComponent.{cpp,h}
├── modes/              MCU feature modes
│   ├── multitrack/     mixer: faders, VPOT params, select/mute/solo/rec
│   │   ├── MultiTrackMode.{cpp,h}  MultiTrackOptions*.{cpp,h}
│   │   ├── MultiTrackSelector.{cpp,h}
│   │   ├── MultiTrackMeterBridge.{cpp,h}
│   │   ├── PanMode.{cpp,h}
│   │   ├── PerformanceMode.{cpp,h}  (intentional stub — see §6)
│   │   └── editor/
│   │       ├── TrackStatesEditorComponent.{cpp,h}
│   │       └── TrackStatesTableComponent.{cpp,h}
│   ├── commands/       Command (Action) Mode — 8 VPOTs × 6 CCs × 2 (Shift) × 8 banks
│   │   ├── CommandMode.{cpp,h}
│   │   └── editor/
│   │       ├── CommandModeMainComponent.{cpp,h}
│   │       ├── CommandModePageComponent.{cpp,h}
│   │       └── CommandModeVPOTComponent.{cpp,h}
│   ├── plugin/         FX Mode — plugin parameter control
│   │   ├── PlugMode.{cpp,h}  PlugAccess.{cpp,h}  PlugMap*.{cpp,h}
│   │   ├── PlugPresetManager.{cpp,h}  PlugWindowManager.{cpp,h}
│   │   ├── PluginWatcher.{cpp,h}  PlugMoveWatcher.{cpp,h}
│   │   ├── PlugModeMeterBridge.{cpp,h}  PlugModeSelectors.{cpp,h}
│   │   └── editor/
│   │       └── PlugMode*Component.{cpp,h}  (14 editor files)
│   └── sends/          Send/Receive Mode
│       ├── SendReceiveModeBase.{cpp,h}  SendMode.{cpp,h}  ReceiveMode.{cpp,h}
│       └── SendReceiveMeterBridge.{cpp,h}

# === Vendored / static resources (not source, on include path) ===
vendor/                 csurf.h
resources/              res.rc  resource.h  res.rc_mac_dlg  res.rc_mac_menu

# === Build infrastructure ===
scripts/
├── fetch_deps.sh         one-time: clone/download JUCE + Boost + REAPER SDK
├── build-portable-linux.sh  podman/docker → dist/reaper_csurf_mcu_klinke.so
├── build-windows.sh      WSL native MSVC build → %APPDATA%\REAPER\UserPlugins\
├── build-windows-fast.sh  WSL /mnt/c copy-build (experimental)
├── build_and_run.sh      Linux: build + deploy + start REAPER
├── debug_reaper.sh       launch REAPER with GDB attached
└── start_reaper.sh       launch REAPER for testing
docker/                 release-linux.Dockerfile (Debian 11 container build)

# === Archives (historical, not built) ===
archive/vs-legacy/      dead .vcxproj/.sln/.dsp (replaced by CMake)
archive/juce-1.52-patches/  old JUCE 1.52 build files

# === Other ===
manual/                 LaTeX user manual (EN/DE)
ai-docs/                extender-support planning documents
dist/                   portable-Linux .so artifact output
build/  build_win/      local build outputs (gitignored)
.prettierrc             (not currently enforced — see §5 conventions)
```
