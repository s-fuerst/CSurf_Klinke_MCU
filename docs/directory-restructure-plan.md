# Directory Restructure Plan

> **Status:** Proposal — not yet executed.  
> **Date:** 2026-06-22  
> **Rationale:** The current flat layout (67 `.cpp`/`.h` files in repo root)
> mirrors Visual Studio virtual folders that were never materialized on disk.
> This plan creates a real nested directory structure that reflects the
> logical architecture.

---

## Design decisions

| Decision | Rationale |
|---|---|
| `.h` and `.cpp` co-located in same directory | Modern C++ convention; no `Header Files`/`Source Files` split |
| `src/` as top-level container | Keeps repo root clean (`CMakeLists.txt`, deps, docs) |
| Max 3 levels deep | `src/modes/plugin/editor/` is the deepest nest |
| Follows VS virtual folder logic | Mirrors `reaper_csurf.vcxproj.filters` groupings |
| SDK/vendored files stay in repo root | `csurf.h`, `reaper_plugin_functions.h`, etc. are not project source |

---

## Full file tree (target state)

```
src/
├── csurf_main.cpp
├── res_linux.cpp
├── windows.h
├── McuDebugLog.h
│
├── core/
│   ├── csurf_mcu.cpp
│   ├── csurf_mcu.h
│   ├── CCSManager.cpp
│   ├── CCSManager.h
│   ├── CCSMode.cpp
│   ├── CCSMode.h
│   ├── ButtonManager.cpp
│   ├── ButtonManager.h
│   ├── Selector.cpp
│   └── Selector.h
│
├── state/
│   ├── Tracks.cpp
│   ├── Tracks.h
│   ├── Transport.cpp
│   ├── Transport.h
│   ├── VPOT_LED.cpp
│   ├── VPOT_LED.h
│   ├── Region.cpp
│   ├── Region.h
│   ├── Options.cpp
│   ├── Options.h
│   ├── ProjectConfig.cpp
│   ├── ProjectConfig.h
│   ├── UndoEnd.cpp
│   └── UndoEnd.h
│
├── display/
│   ├── Display.cpp
│   ├── Display.h
│   ├── DisplayHandler.cpp
│   ├── DisplayHandler.h
│   ├── ActionsDisplay.cpp
│   ├── ActionsDisplay.h
│   ├── ActionsDialogComponent.cpp
│   └── ActionsDialogComponent.h
│
├── meter/
│   ├── MeterBridge.cpp
│   ├── MeterBridge.h
│   ├── MultiTrackMeterBridge.cpp
│   ├── MultiTrackMeterBridge.h
│   ├── SendReceiveMeterBridge.cpp
│   ├── SendReceiveMeterBridge.h
│   ├── PlugModeMeterBridge.cpp
│   └── PlugModeMeterBridge.h
│
├── ui/
│   ├── CCSModesEditor.cpp
│   ├── CCSModesEditor.h
│   ├── TabbedComponentWithCallback.cpp
│   └── TabbedComponentWithCallback.h
│
└── modes/
    ├── commands/
    │   ├── Actions.cpp
    │   ├── Actions.h
    │   ├── CommandMode.cpp
    │   ├── CommandMode.h
    │   ├── CommandModeMainComponent.cpp
    │   ├── CommandModeMainComponent.h
    │   ├── CommandModePageComponent.cpp
    │   ├── CommandModePageComponent.h
    │   ├── CommandModeVPOTComponent.cpp
    │   └── CommandModeVPOTComponent.h
    │
    ├── multitrack/
    │   ├── MultiTrackMode.cpp
    │   ├── MultiTrackMode.h
    │   ├── MultiTrackOptions.cpp
    │   ├── MultiTrackOptions.h
    │   ├── MultiTrackOptions2.cpp
    │   ├── MultiTrackOptions2.h
    │   ├── MultiTrackSelector.cpp
    │   ├── MultiTrackSelector.h
    │   ├── PanMode.cpp
    │   ├── PanMode.h
    │   ├── PerformanceMode.cpp
    │   ├── PerformanceMode.h
    │   ├── TrackStatesEditorComponent.cpp
    │   ├── TrackStatesEditorComponent.h
    │   ├── TrackStatesTableComponent.cpp
    │   └── TrackStatesTableComponent.h
    │
    ├── sends/
    │   ├── SendMode.cpp
    │   ├── SendMode.h
    │   ├── ReceiveMode.cpp
    │   ├── ReceiveMode.h
    │   ├── SendReceiveModeBase.cpp
    │   └── SendReceiveModeBase.h
    │
    └── plugin/
        ├── PlugMode.cpp
        ├── PlugMode.h
        ├── PlugAccess.cpp
        ├── PlugAccess.h
        ├── PluginWatcher.cpp
        ├── PluginWatcher.h
        ├── PlugMoveWatcher.cpp
        ├── PlugMoveWatcher.h
        ├── PlugMap.cpp
        ├── PlugMap.h
        ├── PlugMapManager.cpp
        ├── PlugMapManager.h
        ├── PlugPresetManager.cpp
        ├── PlugPresetManager.h
        ├── PlugWindowManager.cpp
        ├── PlugWindowManager.h
        ├── PlugModeOptions.cpp
        ├── PlugModeOptions.h
        ├── PlugMode2ndOptions.cpp
        ├── PlugMode2ndOptions.h
        ├── PlugModeSelectors.cpp
        ├── PlugModeSelectors.h
        ├── PlugMapSaveDialog.cpp
        ├── PlugMapSaveDialog.h
        │
        └── editor/
            ├── PlugModeComponent.cpp
            ├── PlugModeComponent.h
            ├── PlugModeBankComponent.cpp
            ├── PlugModeBankComponent.h
            ├── PlugModeBankReferenceComponent.cpp
            ├── PlugModeBankReferenceComponent.h
            ├── PlugModeChannelComponent.cpp
            ├── PlugModeChannelComponent.h
            ├── PlugModeFaderComponent.cpp
            ├── PlugModeFaderComponent.h
            ├── PlugModePageComponent.cpp
            ├── PlugModePageComponent.h
            ├── PlugModePageReferenceComponent.cpp
            ├── PlugModePageReferenceComponent.h
            ├── PlugModeParamComponent.cpp
            ├── PlugModeParamComponent.h
            ├── PlugModeMapInfoComponent.cpp
            ├── PlugModeMapInfoComponent.h
            ├── PlugModeSingleBankComponent.cpp
            ├── PlugModeSingleBankComponent.h
            ├── PlugModeSingleChannelComponent.cpp
            ├── PlugModeSingleChannelComponent.h
            ├── PlugModeSinglePageComponent.cpp
            ├── PlugModeSinglePageComponent.h
            ├── PlugModeVPOTComponent.cpp
            ├── PlugModeVPOTComponent.h
            ├── PlugModeVPOTTableComponent.cpp
            └── PlugModeVPOTTableComponent.h
```

---

## Files NOT moved (stay in repo root)

These are SDK headers, vendored utilities, or build artifacts — not project
source code:

```
(repo root)/
├── csurf.h                         ← REAPER csurf SDK header
├── reaper_plugin_functions.h       ← REAPER plugin SDK (402 KB)
├── mcu_button_defines.h            ← MIDI CC ↔ MCU button mapping table
├── Assert.h                        ← ASSERT macro utility
├── std_helper.h                    ← std::map template helpers
├── resource.h                      ← Windows resource IDs
├── res.rc                          ← Windows dialog resources
├── CMakeLists.txt
├── fetch_deps.sh
├── reaper_csurf.sln
├── reaper_csurf.vcxproj
├── reaper_csurf.vcxproj.filters
├── AGENTS.md
├── MEMD.md
├── gplv3.txt
├── notes.org
├── whats_new.org
├── whats_new.txt
├── readme.txt
├── .gitignore
├── juce_1_52/                      ← dependency
├── boost_1_39_0/                   ← dependency
├── reaper-sdk/                     ← dependency
├── JUCE-changes/                   ← JUCE 1.52 patches
├── manual/                         ← LaTeX user manual
└── docs/                           ← project documentation
```

---

## Mapping: VS virtual folder → new directory

| VS Filter (.vcxproj.filters) | New directory | File count |
|---|---|---|
| `(root)` → csurf_mcu, ButtonManager, ... | `src/core/` | 10 |
| `(root)` → Tracks, Transport, VPOT_LED, Region, ProjectConfig, UndoEnd | `src/state/` | 14 |
| `(root)` → (entrypoint, platform shims) | `src/` (root) | 4 |
| `Display_h / Display_src` | `src/display/` | 8 |
| `MeterBridge_h / MeterBridge_src` | `src/meter/` | 8 |
| `CCSModes_h / CCSModes_src` → `Basics` → CCSModesEditor | `src/ui/` | 4 |
| `CCSModes_h / CCSModes_src` → `Basics` → Options | `src/state/` | — |
| `CCSModes_h / CCSModes_src` → `CommandMode` | `src/modes/commands/` | 10 |
| `CCSModes_h / CCSModes_src` → `MultiTrackModes` | `src/modes/multitrack/` | 16 |
| `CCSModes_h / CCSModes_src` → `SendMode` | `src/modes/sends/` | 6 |
| `CCSModes_h / CCSModes_src` → `PlugMode` (root) | `src/modes/plugin/` | 24 |
| `CCSModes_h / CCSModes_src` → `PlugMode` → `PlugModeEditor` | `src/modes/plugin/editor/` | 28 |
| **Total** | | **132** |

---

## Implementation checklist

When this plan is executed, the following must be updated:

- [ ] **Move files** — `git mv` each `.cpp`/`.h` into its target directory
- [ ] **`CMakeLists.txt`** — update all source file paths in `add_library()`
- [ ] **`#include` directives** — ~200 includes need relative paths:
  - `#include "csurf_mcu.h"` → `#include "core/csurf_mcu.h"`
  - `#include "PlugMode.h"` → `#include "modes/plugin/PlugMode.h"`
  - `#include "PlugModeComponent.h"` → `#include "modes/plugin/editor/PlugModeComponent.h"`
  - etc.
- [ ] **`reaper_csurf.vcxproj`** — update all `<ClCompile Include="...">` and `<ClInclude Include="...">` paths (or deprecate the .vcxproj once CMake is the source of truth)
- [ ] **`reaper_csurf.vcxproj.filters`** — may be deleted (directory structure replaces virtual folders)
- [ ] **Verify build** — Linux CMake build must still compile clean
