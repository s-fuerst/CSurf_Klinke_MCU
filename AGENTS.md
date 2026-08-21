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

The extension builds and runs on Windows, macOS, and Linux. It uses
**JUCE 8** (module build) and header-only **Boost 1.91.0**. JUCE 1.52
could not target modern macOS and Apple Silicon; Boost 1.39 does not
compile reliably with modern libc++ and C++17.


## 2. How to build

Full build instructions for all platforms — prerequisites, the convenience
scripts, and the manual CMake alternatives — live in
[`README.md`](README.md). Read that file before building. This section only
records what the README does not:

- **Platform implementation notes** (code-level facts, not build steps):
  - **Windows** — the surface-edit dialog is native Win32, defined in
    `res.rc` and compiled by the MSVC resource compiler. SWELL is not used
    (`SWELL_PROVIDED_BY_APP` undefined; `swell-modstub-generic.cpp` and
    `res_linux.cpp` are excluded). `winmm.lib` is linked explicitly for
    `timeGetTime()` (used in `csurf_mcu.cpp`, `Transport.cpp`,
    `ButtonManager.cpp`). Output name follows the .vcxproj convention:
    `reaper_csurf_mcu_klinke_x64.dll`.
  - **Linux** — the surface-edit dialog uses SWELL: `res_linux.cpp` plus
    `res.rc_mac_dlg` (generated from `res.rc` via `WDL/swell/swell_resgen.pl`).
  - **macOS** — SWELL uses the `SWELL_dllMain` export (not
    `dlopen`/`libSwell.so`); REAPER calls it at plugin load time with a
    `_GetFunc` pointer. The dialog autogen flags (FLIPPED|NOAUTOSIZE) are
    correct by default on macOS; JUCE links all frameworks automatically.
- **Agent build conventions** (see §6 "Patterns" for details): versioning via
  VERSION.txt + build counter, agent-driven deploy after every build, the
  `cmake .. && cmake --build` flow rule, logging via MCU_DEBUG_LOG, and the
  portable-build container gotchas.

## 3. Tech stack & dependencies

| Dependency               | Env var                | Required version                                     | Notes                                                                                                                                                                                           |
|--------------------------|------------------------|------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Reaper Extension SDK** | `REAPER_EXTENSION_SDK` | matches `reaper_plugin_functions.h` (pinned in repo) | Provides `csurf.h`, `reaper_plugin_functions.h`, `ptrlist.h`, `IReaperControlSurface`, `reaper_csurf_reg_t`, `REAPER_PLUGIN_ENTRYPOINT`.                                                        |
| **JUCE**                 | `JUCE_DIR`             | **8.0.14** (modules via `add_subdirectory`)          | GUI framework for all editor dialogs/components. Now licensed AGPLv3/JUCE dual. Modules: `juce_gui_basics` pulls `juce_core`/`juce_events`/`juce_graphics`/`juce_data_structures` transitively. |
| **Boost**                | `BOOST`                | **1.91.0** (header-only)                             | Mainly `boost/signals2.hpp`. No compiled libs needed. Upgraded from 1.39 (which did not compile under modern libc++/C++17).                                                                                                                                       |

Per-platform build instructions are in README.md. No environment variables
are required for the CMake build — it finds all three deps at the repo root.

### `KlinkeLookAndFeel.h` (JUCE 8 specific)
- `KlinkeLookAndFeel.h` — minimal `LookAndFeel_V4` subclass that forces
  readable text and checkbox tick colours on JUCE 8 dialogs.
  Applied per-window in `CCSModesEditor::setMainComponent()`.
  Global `LookAndFeel::setColour()` is avoided — it breaks dialog interactivity.

## 4. Architecture & code map

### Plugin lifecycle & event flow

All sources live under `src/` in the tree described in §7.

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
| **PerformanceMode** | `PerformanceMode.*` | **Unimplemented stub.** Constructed and freed by `CCSManager` but its `B_VPOT_INSTRUMENT` binding is **commented out** (`CCSManager::buttonVPOTassign`), so it is never reachable from hardware. Currently only renders the static text "Performance Mode" on the display; the header also contains abandoned wxWidgets threading scaffolding. Intended for a future real-time Reaper performance readout — see §5. |
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
- **Version string** comes from the `VERSION.txt` file (repo root) — see §6
  "Patterns". The build counter auto-increments; bump the version part manually
  for a release.
  `csurf_mcu.h` uses `MCU_VERSION_STRING` (from generated `Version.h`) in
  `GetDescString()`.
- **No auto-format style is enforced;** match the surrounding file's style
  (roughly 2-space indent, `m_` member prefix, `p` pointer-arg prefix).
- **Config format:** `SurfaceConfig` (in `src/core/SurfaceConfig.h/cpp`)
  handles two formats:
  - **Legacy:** `"0 8 <midiIn> <midiOut> <flags>"` — parsed, never re-emitted.
    Becomes unit 1 populated, with units 2–8 disabled.
  - **KLINKE2:** `"KLINKE2 flags=<N> <in>,<out>,<type> ..."` (8 fixed entries).
    Type tokens: `mackie-main`, `mackie-ext`, `prox-main`, `prox-ext`.
    `GetConfigString()` always emits `KLINKE2` with all 8 entries.
  - `src/core/SurfaceConfig.h` exports: `parseSurfaceConfig()`,
    `serializeSurfaceConfig()`, `makeDefaultSurfaceConfig()`,
    `unitConfigFromType()`, `unitTypeToken()`.
  - The dialog shows 8 fixed rows (Unit 1–8) each with: device type combo,
    MIDI input combo, MIDI output combo. Unit 0 (channels 1-8) offers the
    four hardware types (Mackie Main / Extender, QCon ProX / ProX Extender)
    but **not** "Disabled" — the first 8 channels must always exist.
    Units 1-7 offer all five types including Disabled. Unit position and
    main/extender role are **orthogonal** — a main unit may sit at any
    position. Configs with zero main units are allowed (no validation).
  - `createFunc()` constructs a `HardwareUnit` for every configured unit with
    real MIDI devices. Unit 1 is constructed even when its MIDI ports are None.
    Strip input from every configured unit is translated to global channels.
  - Dialog resources: `res.rc` and `res.rc_mac_dlg` (350×310).
  - Unit type encoding (CB_SETITEMDATA and KLINKE2 tokens):
    0 = mackie-main, 1 = mackie-ext, 2 = prox-main, 3 = prox-ext.

### Using the knowledge graph (graphify)

A `graphify-out/graph.json` knowledge graph lives in the repo root. It is the
fastest way to understand cross-subsystem relationships before changing them.
**Treat any architecture question as a graphify query first** when the graph
exists.

**Keep it fresh.** After editing source files, run `graphify_update .` before the
next query — it re-extracts only changed files (cheap) and keeps node locations
and edges accurate. A stale graph silently points you at the wrong line numbers.

**Pick the right tool for the question:**

- **`graphify_explain "<concept>"` — component audit (preferred default).**
  Returns only the direct neighbours of one node — no truncation regardless of
  graph size. Run this **before** changing a class to confirm you have seen every
  caller/dependency. Example: `graphify_explain "Tracks"` before editing
  channel-mapping logic surfaces every consumer of the channel vector.
- **`graphify_path "<from>" "<to>"` — impact / data-flow tracing.** Best for
  "how does an event reach every unit's display?" type questions. It walks a
  single shortest path and lists every station, so missed hops (e.g. a
  `getDisplayHandler()` that only returns unit 0) become obvious.
- **`graphify_query "<q>" --dfs --budget 4000` — deep connectivity.** Use DFS
  (not the default BFS) and a raised `--budget` for "what is everything connected
  to X?" questions. BFS scatters into hundreds of nodes and truncates; DFS stays
  on path and wastes less of the budget on noise.

**Known limitations — use `grep`/`read` instead:**

- **Finding hardcoded constants** (e.g. every `i <= 8` or `resize(8, …)`):
  graphify is *not* a text search. Use `rg -n "<= 8|resize\(8"` and then read
  the hits.
- **Exact code for edits:** graphify node labels and locations orient you, but
  `edit` needs verbatim source text — switch to `read` once you know the file.
- **BFS truncation:** a default-budget BFS over this codebase easily finds
  300+ nodes and cuts ~80% of them. The cut is purely by graph distance from
  the seed (not by relevance), so an important far-away node can vanish.
  Mitigate with narrow seeds, DFS, `graphify_explain`, or `--budget`.

**Workflow that works well here:** graphify (`explain`/`path`) to find the
relevant files and confirm the blast radius → `read` for the exact text → `edit`
→ build + deploy → `graphify_update` to refresh. Don't skip the first step on a
change that touches more than one subsystem — the graph catches cross-subsystem
callers that a single-file read misses.

## 5. References
- **Reaper Extension / csurf SDK** — https://github.com/justinfrankel/reaper-sdk
  (most relevant: `reaper-sdk/reaper-plugins/reaper_csurf/`)
- **Reaper** — https://www.reaper.fm/
- **Similar projects (good references):**
  - CSI (Control Surface Integrator) — https://github.com/reaper-csi/reaper_csurf_integrator
  - DrivenByMoss — https://github.com/git-moss/DrivenByMoss
- **JUCE 8** — https://github.com/juce-framework/JUCE (tag 8.0.14)

## 6. Patterns

- User can run sudo commands when requested — just tell them what to
  run and they'll execute it. No sudo password prompt needed from
  agent side.
- Version source = VERSION.txt file (repo root), NOT
  hard-coded. Format "<version> <build-count>". Bump version manually
  (reset count); count auto-increments per `cmake`
  configure. Generated build/Version.h → MCU_VERSION_STRING in
  csurf_mcu.h GetDescString.
- Deploy is the AGENT's or build scripts job, not CMake's: after a
  successful Linux build, `cp build/reaper_csurf_mcu_klinke.so
  ~/.config/REAPER/UserPlugins/`. No CMake auto-deploy (keeps
  CI/Windows/macOS clean). Reaper needs full restart to reload.
- Every build script archives a copy of the freshly built binary in
  `dist/` (gitignored): build-portable-linux.sh → portable .so,
  build-and-run-linux-macos.sh → native Linux .so / macOS .dylib,
  build-windows-from-wsl.sh → Windows .dll. The copy is made right
  after the build (also with --no-deploy).
- Logging architecture: MCU_DEBUG_LOG = standalone CMake OPTION
  (default ON), NOT tied to CMAKE_BUILD_TYPE → currently EVERY build
  logs, including Release. When OFF, MCU_LOG(...) compiles to
  ((void)0) (see src/core/McuDebugLog.h #else) → zero runtime cost. 16
  call sites in 6 files; most verbose: sendToHardware logs every text
  write (ROW0/ROW1 snd). Release goal: default OFF, ON only for debug
  builds.
- Build-flow rule (always run both steps, otherwise no build-count
  increment): `cd build && cmake .. -DCMAKE_BUILD_TYPE=Release &&
  cmake --build . -- -j$(nproc)`. The increment happens ONLY in the
  configure step (`cmake ..`), NOT in the build step. Running only
  `cmake --build` (incremental) without `cmake ..` first produces a
  build without an incremented counter. Mnemonic: "configure =
  increments, build = links".
- CRLF gotcha: the repo is checked out with core.autocrlf=true, so shell
  scripts may be CRLF and bash will refuse them with "bash\r: No such file
  or directory". If that happens, strip once: sed -i 's/\r$//'
  scripts/fetch_deps.sh. scripts/build-windows-from-wsl.sh itself is
  committed LF.
- Commit messages always in English (user requirement).
- No commits without explicit user instruction (user requirement).
- Always write links out in full (https://...), not as Markdown
  hyperlinks [text](URL) — the user cannot click Markdown links.
- HIGH-PRIORITY LANGUAGE RULE: the dialog with the user may be in
  German, but EVERYTHING written into a file (source, comments,
  strings, logs, commit messages, docs, AGENTS.md, manual, MEMD.md,
  generated code, configs, scripts) MUST be in English. Never write
  German text into a file, not even comments. Anchored at the very top
  of AGENTS.md as "⚠️ Language rule (HIGH PRIORITY)".
- macOS case-insensitivity hazard: project files at repo root (on the
  -I path) collide case-insensitively with system headers on
  APFS. Found: VERSION→<version> (libc++), Assert.h→<assert.h>. The
  clang -Wnonportable-include-path warning ("differs in case from file
  name on disk") is the tell. Mitigation: unique file names
  (VERSION.txt, McuAssert.h). Watch for new ones whenever a new
  root-level header is added.
- macOS build: NEVER set CC to Homebrew gcc — JUCE 8 juceaide needs
  Apple Clang. CMakeLists.txt forces Clang on Darwin. Override with
  -DCMAKE_C_COMPILER if ever needed.
- macOS JUCE event dispatch is automatic via NSRunLoop (driven by
  REAPER main thread); do NOT call dispatchNextMessageOnSystemQueue on
  macOS (no impl in JUCE 8). Linux/X11 needs the manual pump (#if
  JUCE_LINUX in csurf_mcu.cpp Run()).
- macOS SWELL stub: each platform has a DIFFERENT swell-modstub
  variant. macOS = swell-modstub.mm (NSApp delegate constructor),
  Linux = swell-modstub-generic.cpp (SWELL_LOAD_SWELL_DYLIB dlopens
  libSwell.so, OR non-dlopen path with SWELL_dllMain getfunc), Windows
  = none (native Win32). Never use the generic stub on macOS; REAPER
  does not pass the getfunc through SWELL_dllMain there.
- Portable Linux build gotchas (docker/release-linux.Dockerfile, all
  container-scoped): (1) JUCE 8 needs cmake ≥3.22 but Debian 11 ships
  3.18 → install the cmake.org binary tarball. (2) juceaide (JUCE's
  codegen helper) is built by a RECURSIVE cmake subprocess whose
  PASSTHROUGH_ARGS does NOT include CMAKE_C[XX]_FLAGS — so forcing
  -I/usr/include/freetype2 via CMAKE_CXX_FLAGS does NOT reach
  juceaide. (3) juceaide compiles juce_graphics.cpp which needs
  <ft2build.h> (flat header under /usr/include/freetype2/) and
  <fontconfig/fontconfig.h>. Fix: symlink /usr/include/ft2build.h →
  freetype2/ft2build.h AND /usr/include/freetype →
  freetype2/freetype. Plus install libfontconfig1-dev. (4) Debian gcc
  enables -Wl,--as-needed by default → drops unused -l libs from
  NEEDED (X11 gets dropped; harmless — JUCE loads X11 via dlopen). (5)
  Need libxcursor/xinerama/xrandr/xrender/xcomposite-dev +
  libglu1-mesa-dev for JUCE pkg-config module deps. (6) Static
  libstdc++.a on Debian 11 is built with -fPIC, so -static-libstdc++
  works for a SHARED lib.
- Debugging freezes / hangs: since ptrace requires root on this system
  (kernel.yama.ptrace_scope=1), the GDB backtrace command is `sudo gdb
  -p $(pidof reaper) -batch -ex "thread apply all bt"`. This MUST be
  run in the user's terminal where sudo password entry works. The
  agent cannot drive this through the bash tool.
- Reaper GUI needs GDK_BACKEND=x11 on this system (Wayland
  host). Start with: `GDK_BACKEND=x11
  /home/fuerst/opt/REAPER/reaper`. For GDB: `GDK_BACKEND=x11 gdb -ex
  run --args /home/fuerst/opt/REAPER/reaper` — but GDB must be
  interactive (user's terminal) since ptrace requires sudo.
- CRITICAL: NEVER attempt `sudo` commands via the bash tool. The agent
  has no access to the user's password and repeated failed sudo
  attempts lock the user's account. When root privileges are needed
  (e.g. `gdb -p` due to ptrace_scope=1), instruct the user to run the
  command in their own terminal.
- ASan (AddressSanitizer) workflow: `rm -rf build_asan && mkdir
  build_asan && cd build_asan && cmake .. -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address" && cmake --build
  . -j$(nproc) && cp reaper_csurf_mcu_klinke.so
  ~/.config/REAPER/UserPlugins/ && LD_PRELOAD=/usr/lib/libasan.so
  GDK_BACKEND=x11 timeout 15 /home/fuerst/opt/REAPER/reaper 2>&1 | tee
  /tmp/reaper_asan.log`. Buffer overflows are detected during init —
  no user interaction needed.



## 7. Repo layout (quick map)

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
│   ├── SurfaceConfig.{h,cpp}  multi-unit config model + KLINKE2 parser/serializer
│   ├── SurfaceConfigDialog.cpp  config dialog: dlgProc, layout, createFunc, configFunc, reaper_csurf_reg
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
│   │   ├── PerformanceMode.{cpp,h}  (intentional stub — see §5)
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
├── build-windows-from-wsl.sh  WSL MSVC build via /mnt/c Ninja mirror → %APPDATA%\REAPER\UserPlugins\
├── build-and-run-linux-macos.sh  Linux/macOS: build + deploy + start REAPER
├── debug_reaper.sh       launch REAPER with GDB attached
└── start_reaper.sh       launch REAPER for testing
docker/                 release-linux.Dockerfile (Debian 11 container build)

# === Archives (historical, not built) ===
archive/vs-legacy/      dead .vcxproj/.sln/.dsp (replaced by CMake)
archive/juce-1.52-patches/  old JUCE 1.52 build files

# === Other ===
manual/                 LaTeX user manual (EN)
ai-docs/                extender-support planning documents
dist/                   build artifact output (copy of every freshly built binary)
build/  build_win/      local build outputs (gitignored)
```

## 8. MCU hand-off with the Schaltmix plugin (KLINKE-only feature)

The private `--klinke` build (`MCU_KLINKE_BUILD=ON` → `#ifdef KLINKE`) can share
the iCON controllers (Platform M+ = unit 3 = `KLINKE_COMBO_UNIT_INDEX` 2,
Platform X+ = unit 2) with the Schaltmix VST plugin. Both programs keep their
MIDI ports open (multiclient).

The version label in the surface config dialog (`IDC_VERSION_LABEL`) appends a
"k" to the version string in KLINKE builds so the private build is
distinguishable from the public release.
