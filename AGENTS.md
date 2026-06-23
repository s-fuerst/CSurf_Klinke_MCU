# AGENTS.md — Guide for Contributors & AI Agents

> Read this first. It orients you to what this project is, how it fits
> together, how to build it, and what the current revival effort is about.

**When you're stuck or are unsure, ask.** You don't have to figure
everything out by yourself — asking questions or directly requesting
help is expected and encouraged. The project is complex and the
revival touches many moving parts; a quick question can save hours of
guesswork.

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
the existing dependency versions (JUCE 1.52 and Boost) unchanged.** Reaper
itself runs on all three platforms and its csurf SDK is cross-platform, so
this is primarily a build-system and platform-porting effort, not a rewrite.

> The original build is **Windows + Visual Studio only**. A cross-platform
> **CMake** build has now been added (see §4): **Linux** is the working
> baseline; the Windows and macOS CMake paths are stubbed and come next. The
> VS `.vcxproj` remains the source of truth for the **Windows** build until
> its CMake branch is filled in.

### Prerequisites for the CMake build (exact versions pinned)

Following the build instructions from the Rothchild Linux port (we use the
*instructions*, not their repo). All three deps live at the **repo root**:

1. **JUCE 1.52** (amalgamated build) → `juce_1_52/`
   - Simplest: clone the Stenzel Bitbucket fork and copy its bundled copy:
     `hg clone https://bitbucket.org/Stenzel/csurf_klinke_mcu original-klinke`
     then `cp -r original-klinke/juce_1_52 .`
   - Alt (no Mercurial): download the Stenzel repo as a tarball/zip and
     extract `juce_1_52/`; or `https://github.com/julianstorer/JUCE/tree/juce-1.52`.
   - The CMake build needs the **amalgamated** files (`juce_amalgamated.cpp`),
     so the Stenzel copy (known-good) is preferred over the modular GitHub tree.
2. **Boost 1.39.0** (headers only) → `boost_1_39_0/`
   - `https://archives.boost.io/release/1.39.0/source/boost_1_39_0.tar.bz2`
   - Only headers are used (smart pointers, `signals2`). No compiled libs.
3. **REAPER SDK** (WDL + SWELL + plugin headers) → `reaper-sdk/`
   - From the same Stenzel fork: `cp -r original-klinke/reaper-sdk .`
   - Alt: Cockos — `https://www.reaper.fm/sdk/plugin/plugin.php`
4. **Linux system packages** (build host):
   `sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev`

> The original VS `.vcxproj` reads these via env vars (`JUCE`, `BOOST`,
> `REAPER_EXTENSION_SDK`). The CMake build instead expects them at the repo
> root, so the env vars are no longer needed for the CMake path.

## 3. Tech stack & dependencies

| Dependency | Env var | Required version | Notes |
|---|---|---|---|
| **Reaper Extension SDK** | `REAPER_EXTENSION_SDK` | matches `reaper_plugin_functions.h` (pinned in repo) | Provides `csurf.h`, `reaper_plugin_functions.h`, `ptrlist.h`, `IReaperControlSurface`, `reaper_csurf_reg_t`, `REAPER_PLUGIN_ENTRYPOINT`. |
| **JUCE** | `JUCE` | **1.52 exactly** (not backward compatible with newer JUCE) | GUI framework for all editor dialogs/components. JUCE 1.52 predates official x64 support, so the repo ships patches in `JUCE-changes/` (see below). |
| **Boost** | `BOOST` | ≥ 1.39 (header-only) | Mainly `boost/signals2.hpp`. No compiled libs needed. |

The three SDK roots are referenced by the Visual Studio project through the
environment variables above. **All three must be set before building.**

### `JUCE-changes/` (important)
- `JUCE-changes/juce_Config.h` — patched JUCE config; replace the one from
  the JUCE 1.52 download with this.
- `JUCE-changes/Builds/` — an updated JUCE project that adds the **x64**
  target (JUCE 1.52 shipped only Win32).

These patches are why you cannot simply drop in a newer JUCE — the code is
written against the 1.52 API and `juce_Config.h` enables a specific subset
of JUCE modules.

## 4. How to build

**Status:**
- **Linux** — CMake build working (`CMakeLists.txt`, baseline). See below.
- **Windows** — CMake build NOT yet wired in (the `WIN32` branch in
  `CMakeLists.txt` fails with a clear message); use the VS `.vcxproj` for
  now. Next milestone after the Linux baseline is confirmed.
- **macOS** — CMake build NOT yet wired in; planned after Windows.

### Linux (CMake, baseline)

```bash
# one-time: fetch the three pinned deps to the repo root
./fetch_deps.sh

# build-host packages
sudo apt install build-essential cmake libfreetype-dev libx11-dev libxext-dev

# configure + build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(nproc)"
```

Output: `build/reaper_csurf_mcu_klinke.so`. Deploy by copying it to
`~/.config/REAPER/UserPlugins/` and adding **"Mackie Control Protocol
(Klinke)"** in Reaper → Preferences → Control/OSC/web. Pass
`-DMCU_DEBUG_LOG=OFF` to disable the debug log (on by default).

The CMake build uses **SWELL** (WDL) for the surface-edit dialog on Linux
(`res_linux.cpp` + `res.rc_mac_dlg`, generated from `res.rc` via
`WDL/swell/swell_resgen.pl`).

### Windows (Visual Studio — current source of truth)

1. Install Visual Studio 2019 (toolset `v142`) with the Windows 10 SDK.
2. Set env vars `JUCE`, `REAPER_EXTENSION_SDK`, `BOOST` to each SDK's root.
3. Apply `JUCE-changes/` to your JUCE 1.52 tree (replace `juce_Config.h`,
   build the x64 JUCE libs using the included project).
4. Open `reaper_csurf.sln` and build.

**Build matrix** (`reaper_csurf.vcxproj`):
- Configurations: `Debug`, `Release`, `Release_B`, `Klinke`
- Platforms: `Win32`, `x64`
- Output type: `DynamicLibrary` → `reaper_csurf_mcu_klinke.dll`
- `Release_B` / the `EXT_B` define build the **"Protocol B" extender**
  variant (second MCU unit). The same source compiles two surface types via
  the `EXT_B` preprocessor switch and the runtime `m_is_mcuex` flag.

The `.vcxproj` is the reference for include paths, preprocessor defines
(`EXT_B`, JUCE module flags), and the source-file set that `CMakeLists.txt`
must reproduce (it does — all 65 files verified present).

## 5. Architecture & code map

### Plugin lifecycle & event flow
```
reaper loads the .dll
  → csurf_main.cpp: REAPER_PLUGIN_ENTRYPOINT
      registers csurf_mcu_modified_reg  ("csurf")
  → CSurf_MCU  (csurf_mcu.cpp/.h)  implements IReaperControlSurface
      owns: Transport, DisplayHandler, Display(s), Region, CCSManager
      handles: MIDI in/out to the MCU, per-frame updates, config, surface-edit dialog
  → CCSManager (CCSManager.cpp/.h)
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
- **`Tracks.*`** — track state tracking (selection, mute/solo, name, level).
- **`Transport.*`** — play/stop/record/rewind/FFWD, markers, loop.
- **`VPOT_LED.*`** — models the V-Pot LED ring state/mode sent to hardware.
- **`mcu_button_defines.h`** — the MIDI CC ↔ MCU button/VPOT mapping table.
- **`Display.*` / `DisplayHandler.*`** — the two 2x55-char MCU displays.
- **`CCSModesEditor.*`** + all `*Component.*` files — JUCE-based GUI editors
  for each mode's settings (shown on screen, not on the controller).
- **`Options.*` / `ProjectConfig.*`** — global options and per-project config
  persistence (saved inside the Reaper project).
- **`Region.*`** — store loop/time selections as Reaper regions.
- **`res.rc` / `resource.h`** — Windows resources for the surface-edit dialog
  (the Reaper preferences panel for this surface).

### Conventions & gotchas
- **Macros:** `safe_call(p,f)`, `safe_delete(x)`, `safe_delete_array(x)`
  (defined in `csurf_mcu.h`) are used pervasively for NULL-safe calls/deletes.
- **Mode guards:** `CHECKMODE` / `CHECKMODEANDCHANNEL` in mode methods bail
  out if the active mode changed under the caller — always respect these.
- **Sentinel GUIDs:** `GUID_NOT_ACTIVE` (all-zero) and `GUID_MASTER` mark
  "no track" and the master track throughout the plug/track code.
- **1-based channel arrays:** many `[9]` arrays are 1-based; index 0 is the
  master fader. `ASSERT` (in `Assert.h`) guards channel ranges.
- **`EXT_B`** is a *compile-time* switch for the extender variant; build the
  main unit and the B unit from the same source.
- **Version string** is hard-coded in `CSurf_MCU::GetDescString()` — bump it
  there on release.
- **No auto-format style is enforced;** match the surrounding file's style
  (roughly 2-space indent, `m_` member prefix, `p` pointer-arg prefix).

## 6. Known issues & open work (from `notes.org`, `whats_new.org`)
- **License:** finish migrating headers + manual text from GPLv2 to GPLv3.
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
- **JUCE 1.52** — https://github.com/julianstorer/JUCE/releases (historical)

## 8. Repo layout (quick map)
```
csurf_main.cpp / csurf_mcu.{cpp,h}   plugin entry + main control-surface class
CCSManager.{cpp,h} / CCSMode.{cpp,h} mode dispatcher + mode base class
MultiTrack*.{cpp,h} PanMode.* Send*.* Receive*.*   mixer/routing modes
CommandMode.* Actions*.*                       Action Mode (VPOTs → actions)
PlugMode.* Plug*Component.* PlugAccess.* PlugMap*.* PlugPreset*.* Plugin*.*  FX mode
Tracks.* Transport.* VPOT_LED.* Display*.* Region.*  core hardware/track glue
*Component.{cpp,h} CCSModesEditor.*            JUCE on-screen editors
Options.* ProjectConfig.*                      settings + project persistence
mcu_button_defines.h csurf.h reaper_plugin_functions.h ptrlist.h  SDK headers
manual/                                        LaTeX user manual (EN/DE)
JUCE-changes/                                  patched juce_Config.h + x64 build
res.rc resource.h                              Windows surface-edit dialog
reaper_csurf.sln/.vcxproj                      Visual Studio build (current)
gplv3.txt notes.org whats_new.{org,txt} readme.txt   license + notes
```
