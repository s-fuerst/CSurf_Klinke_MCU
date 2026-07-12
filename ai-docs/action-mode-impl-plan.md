# Action Mode — Implementation Plan

> Date: 2026-07-12
> Status: **PLANNING** (no code changes yet)
> Companion to: `extender-support.md` (master plan), `extender-wp-f-widening-audit.md` (8-channel audit)

## 0. Executive Summary

The Action Mode consists of **two independent subsystems** that together provide
the MCU Action-Mode feature described in the manual (`mcu_klinke_manual.tex`,
`text_en/actionmode.tex`):

1. **`Actions` + `ActionsDisplay`** — Global Reaper action registration for
   MCU hardware buttons (rec, solo, mute, select, transport, VPOT assign, F-keys,
   automation, modifiers, fader touch). These are one-shot: pressing a button
   fires the corresponding Reaper action.

2. **`CommandMode`** — The VPOT-based mode that maps 8 VPOTs × 6 CC states ×
   2 (Shift) × 8 banks = 768 unique MIDI CC addresses to Reaper actions via
   MIDI-Learn. Includes an on-screen JUCE editor, per-page configuration, and
   LCD display of action names.

Both subsystems are **functional and tested** — this plan covers improvements,
not ground-up construction. The work is organized into 5 phases, each
independent and buildable in isolation.

**Total estimated effort:** ~2.5–3.5 days (see timeline in §7).

---

## 1. Current State (Detailed Inventory)

### 1.1 Actions + ActionsDisplay (`src/action/`)

| File | Lines | Purpose |
|---|---|---|
| `Actions.h` | 58 | Singleton, ~150 action registrations, `Action` inner class |
| `Actions.cpp` | 182 | Registers actions as Reaper commands, dispatches synthetic MIDI events to `ButtonManager` |
| `ActionsDisplay.h` | 40 | 16-modifier display matrix (Shift/Ctrl/Alt/Option combos) for action labels |
| `ActionsDisplay.cpp` | 107 | LCD rendering, `GlobalActions.xml` read/write |

**What it does:**
- `Actions::instance()` creates ~150 Reaper actions (one-shot "key" and toggle
  "button"), each mapped to a specific MCU button by its MIDI CC `buttonId`.
- `commandCallback()` synthesizes MIDI events (Note On/Off) and feeds them to
  `ButtonManager::dispatchMidiEvent()`, so the existing button handler paths
  execute.
- Key actions fire a down+up pair immediately; button actions toggle state.
- `ActionsDisplay` manages a 16-entry label matrix (4 modifier bits) for
  displaying action names on the MCU LCD. Persisted as `GlobalActions.xml`.

**What it doesn't do (gaps):**
- No import/export of action assignment sets (manual §4 says users should
  import them).
- Config path uses Windows-style `\\Reaper\\MCU\\Config\\` on Windows, and
  `GetResourcePath()/MCU/Config/` on Linux/macOS — neither is the standard
  user config directory for cross-platform.
- `GlobalActions.xml` is read once at construction; no UI to edit labels
  without manually editing the XML.

### 1.2 CommandMode (`src/modes/commands/`)

| File | Lines | Purpose |
|---|---|---|
| `CommandMode.h` | 110 | Mode class, `Page` inner class, `CommandPageSelector` |
| `CommandMode.cpp` | 280 | Activation, VPOT CC generation, display update, config I/O |
| `CommandModeMainComponent.h` | 75 | Tabbed editor window (8 pages) |
| `CommandModeMainComponent.cpp` | 145 | Creates 8 `CommandModePageComponent` tabs |
| `CommandModePageComponent.h` | 72 | Per-page editor: page name label + 16 VPOT components |
| `CommandModePageComponent.cpp` | 141 | Layout: page name, action labels, sliders |
| `CommandModeVPOTComponent.h` | 70 | Per-VPOT: name (6 char), relative toggle, 2 speed sliders |
| `CommandModeVPOTComponent.cpp` | 217 | Widget binding to `Page` data model |

**What it does:**
- Inherits `MultiTrackMode` → gets fader/pan/mute/solo/rec strip behavior free.
- 8 pages (banks), each with 8 VPOTs × 2 (Shift). Each VPOT sends 6 CCs:
  rotate-left, rotate-right, rotate-left-pressed, rotate-right-pressed,
  press, release. Relative mode uses CC 0x40+offset encoding.
- Press-and-hold EQ button opens page selector on LCD.
- Alt+EQ opens the JUCE editor (`CommandModeMainComponent`);
  Alt+other-VPOT-assign opens other mode editors.
- Per-page config persisted as `<REAPER_RESOURCE>/MCU/Config/ActionMode.xml`.
- Display: line 1 shows VPOT action names (or "No actions are name[d]…" if
  none assigned); line 3 shows DB/pan values on ProX units.

**What it doesn't do (gaps):**
1. **Config path not portable** — `getConfigFile()` hardcodes
   `\\Reaper\\MCU\\Config\\` using `File::userDocumentsDirectory`. On Linux
   this falls through to `GetResourcePath()` which is the install directory
   (not writable for most users). Should use Reaper's resource path
   (`GetResourcePath()`) on all platforms, or the standard user config dir.
2. **Spelling error** — line ~140: "No actions are name for this bank" → "named".
3. **No import/export** — users must configure all 128 slots (8 VPOTs × 2
   shift × 8 pages) by hand or edit XML. No way to import the preset action
   assignments mentioned in the manual.
4. **`writeConfigFile()` called only on editor close** (`~CommandModeMainComponent`).
   If the mode is deactivated without opening/closing the editor (e.g. Reaper
   shutdown), changes may be lost. The dtor of `CommandMode` does NOT call
   `writeConfigFile()` — only the editor dtor does.
5. **Multi-unit extender design unresolved** — see §5.

### 1.3 Integration Points

```
CSurf_MCU::Run()
 └─ ButtonManager::dispatchMidiEvent()
     └─ CSurf_MCU::OnMidiEvent()
         └─ CCSManager::buttonVPOTassign(B_VPOT_EQ)  → switches to CommandMode
             └─ CommandMode::activate()
                 ├─ MultiDisplay::switchToAll()  → LCD routing
                 ├─ disableMCUMeter()
                 └─ updateDisplay()  → show VPOT names on line 1

Editor: Alt+EQ
 └─ CCSManager::buttonVPOTassign()  [VK_ALT path]
     └─ CCSModesEditor::setMainComponent(m_pCommandMode, true)
         └─ CommandMode::createEditorComponent()
             └─ CommandModeMainComponent  (tabs × 8 pages)

VPOT movement:
 └─ CSurf_MCU::OnMidiEvent()  → CCSManager::vpotMoved(channel, numSteps)
     └─ CommandMode::vpotMoved(channel, numSteps)
         └─ kbd_OnMidiEvent(&evt, -1)  → Reaper MIDI-learn dispatcher
```

---

## 2. Phase 1 — Bug Fixes & Polish (low-risk, immediate)

**Goal:** Fix small correctness issues and code quality problems. No new
features. No architectural changes.

### 2.1 Fix config file path cross-platform (`CommandMode.cpp`)

**Current:**
```cpp
File CommandMode::getConfigFile() {
  File configDir =
      File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() +
      String("\\Reaper\\MCU\\Config\\");
  ...
}
```

**Problem:** Uses `userDocumentsDirectory` on Windows, but
`GetResourcePath()` on Linux (ActionsDisplay.cpp). Inconsistent, and
`userDocumentsDirectory` is the wrong location for app config.

**Fix:** Match the pattern used by `ActionsDisplay::getConfigFile()` but
make it cross-platform properly:
```cpp
File CommandMode::getConfigFile() {
#ifdef _WIN32
  File configDir = File::getSpecialLocation(File::userApplicationDataDirectory)
                       .getChildFile("REAPER/MCU/Config");
#else
  File configDir = String(GetResourcePath()) + "/MCU/Config";
#endif
  if (!configDir.exists())
    configDir.createDirectory();
  return configDir.getChildFile("ActionMode.xml");
}
```

Or better, use `GetResourcePath()` on all platforms (it returns the Reaper
resource dir, which is the canonical location for extension data).

**Files:** `src/modes/commands/CommandMode.cpp` (lines ~283-291)

### 2.2 Fix spelling error in display message

**Current:** `"No actions are name for this bank (press Alt-EQ)."`

**Fix:** `"No actions are named for this bank (press Alt+EQ)."`

**Files:** `src/modes/commands/CommandMode.cpp` (line ~140)

### 2.3 Fix `ActionsDisplay::getConfigFile()` cross-platform path

Same pattern as above — ensure consistent with CommandMode.

**Files:** `src/action/ActionsDisplay.cpp` (lines ~85-99)

### 2.4 Ensure config is saved on `CommandMode` destruction

**Current:** `writeConfigFile()` is only called from
`~CommandModeMainComponent()`. If the editor was never opened, or if Reaper
shuts down without closing the editor, changes may be lost.

**Fix:** Add `writeConfigFile()` call to `CommandMode::~CommandMode()`:

```cpp
CommandMode::~CommandMode(void) {
  writeConfigFile();  // <-- ADD THIS
  safe_delete(m_pMainComponent);
  for (int i = 0; i < 8; i++) {
    safe_delete(m_pPage[i]);
  }
  safe_delete(m_pSelector);
}
```

The order matters — write BEFORE deleting pages.

**Files:** `src/modes/commands/CommandMode.cpp` (line ~108)

### 2.5 Add XML root element version attribute

**Current:** `CommandMode::writeConfigFile()` creates `<ACTIVE_MODE_CONFIG
version="1">` (already has version). But `ActionsDisplay::writeConfigFile()`
creates `<GLOBAL_ACTIONS_CONFIG>` without a version attribute.

**Fix:** Add `version="1"` attribute for future migration safety.

**Files:** `src/action/ActionsDisplay.cpp` (line ~97)

---

## 3. Phase 2 — Config Path Unification (medium-risk)

**Goal:** Make both Action Mode config files use the same, correct, cross-platform
directory. No behavioral changes.

### 3.1 Define a shared config path helper

Create or use an existing utility that returns `{ReaperResource}/MCU/Config/`
on all platforms. Option: add a static method to an existing utility class, or
a free function in a new header.

```cpp
// src/core/ConfigPath.h
#pragma once
#include "JuceHeader.h"

inline File getMcuConfigDir() {
#ifdef _WIN32
  File dir = File::getSpecialLocation(File::userApplicationDataDirectory)
                 .getChildFile("REAPER/MCU/Config");
#else
  File dir = String(GetResourcePath()) + "/MCU/Config";
#endif
  if (!dir.exists())
    dir.createDirectory();
  return dir;
}

inline File getMcuConfigFile(const String &name) {
  return getMcuConfigDir().getChildFile(name);
}
```

### 3.2 Migrate both config readers/writers

Replace the duplicated path logic in:
- `CommandMode::getConfigFile()`
- `ActionsDisplay::getConfigFile()`

With calls to `getMcuConfigFile("ActionMode.xml")` and
`getMcuConfigFile("GlobalActions.xml")`.

**Files:** `src/modes/commands/CommandMode.cpp`, `src/action/ActionsDisplay.cpp`

### 3.3 Clean up `ActionsDisplay::getConfigFile(bool bLookAtProgramDir)`

The `bLookAtProgramDir` parameter is always `true` (read) or `false` (write),
which should both go to the same user-config location. After unification,
this parameter is dead code on all platforms. Simplify to no parameter:

```cpp
// Before: getConfigFile(true) for read, getConfigFile(false) for write
// After:  getConfigFile() for both
```

---

## 4. Phase 3 — Import/Export Action Assignments (new feature)

**Goal:** Let users import preset action assignments (as described in the manual
§4), share configurations, and back up their settings.

### 4.1 Design

The manual (`text_en/installation.tex`, `text_en/actionmode.tex`) mentions
importing action assignments but no UI exists for it. The ActionMode.xml
format is well-defined (XML with PAGE/VPOT elements). We'll add:

1. **"Import" button** in the `CommandModeMainComponent` editor — opens a file
   chooser, reads an XML file, merges/replaces pages.
2. **"Export" button** — writes current config to a user-chosen file.
3. **"Reset to defaults" button** — clears all assignments.

### 4.2 Implementation

**New UI elements in `CommandModeMainComponent`:**
- Add a button row **above** the tabbed pages (not inside the tabs — import
  applies to all pages, not just the current one). Suggested layout: a
  `Component` strip at the top of `CommandModeMainComponent` containing
  `TextButton` "Import…", "Export…", "Reset".
- Wire them to file chooser dialogs (`FileChooser` with `"*.xml"` filter)
  and the new import/export/reset methods.
- Error reporting: use `AlertWindow::showMessageBox()` for malformed XML,
  partial import failures, and file I/O errors — never fail silently.

**New methods on `CommandMode`:**
- `bool importFromFile(const File &file)` — reads XML, validates structure
  before applying any changes, then merges into pages. Validation checks:
  - Root element must be `<ACTIVE_MODE_CONFIG>`.
  - The `version` attribute (if present) must be `1` — reject
    unknown/future versions with a clear error message.
  - Each `<PAGE>` child must have a valid `index` attribute (0–7).
  - Each page must contain exactly 16 `<VPOT>` elements.
  - **Rollback on failure:** Parse into a temporary `Page[8]` array
    first; only replace `m_pPage` entries if ALL 8 pages parse
    successfully. A partial failure leaves the current config untouched.
- `void exportToFile(const File &file)` — writes current config to the
  chosen file (calls `writeConfigFile()` with target path).
- `void resetToDefaults()` — reinitializes all 8 pages to empty defaults,
  calls `updateDisplay()`. Prompts for confirmation first.

**Files:**
- `src/modes/commands/CommandMode.h` — new method declarations
- `src/modes/commands/CommandMode.cpp` — implementations
- `src/modes/commands/editor/CommandModeMainComponent.h/.cpp` — UI buttons

### 4.3 Merge semantics

On import, for each `<PAGE>` in the source XML:
- If a page with matching `index` already exists → replace it.
- If no matching index → ignore (we have exactly 8 pages, no more).

This preserves the user's untouched pages.

---

## 5. Phase 4 — Multi-Unit Extender Design (planning only)

**Important:** This phase is **design documentation only**. Implementation is
deferred to a separate work package (WP-ActionMode-Extender) AFTER the
per-mode WP for CommandMode is prioritized in the master plan
(`extender-support.md` §7).

### 5.1 Design Decision

Per `extender-wp-f-widening-audit.md` §3.4, the `CommandMode` 8s are
**per-block-8 (VPOTs/banks/pages)**, not surface-channel counts. They are
correct as-is and no widening is needed for channel strips 9+.

The open question is: **what does an extender do in Action Mode?**

| Option | Description | Pro | Con |
|---|---|---|---|
| **A: Mirror (Recommended)** | All units show the same 8 VPOTs / page / bank. Extender VPOTs duplicate main-unit VPOTs. | Simple, predictable, no new concepts for users. Matches "linked mode" default. | Redundant hardware capability. |
| **B: Extend banks** | Main = pages 0-3, extender = pages 4-7. VPOTs 1-8 on main, 9-16 on extender. | Uses all hardware. Double action count accessible at once. | Complex page mapping, confusing when units are released. |
| **C: Independent per-unit** | Each unit gets its own page/bank. Unit 0 page 0, Unit 1 page 2, etc. | Maximum flexibility. | Complex, breaks "linked mode" default, requires per-unit config UI. |

**Recommendation: Option A (Mirror)** — follows the master plan's linked-mode
default (§7). The user sees the same 8 VPOTs with the same actions on every
unit. The LCD on each unit shows the same action names. This is the
least-surprising behavior and requires the fewest code changes.

### 5.2 Implementation sketch (Option A)

When N > 1:
- `CommandMode::activate()` already calls `MultiDisplay::switchToAll()`,
  which routes each child display to its own handler → **display works for
  free.**
- `CommandMode::vpotMoved()` uses `m_pActivePage->m_iIndex` as the MIDI
  channel (0xB0 + pageIndex). Reaper's MIDI-learn dispatcher receives this
  from any unit. No change needed — **the CC is the same regardless of which
  unit's VPOT was moved.**
- `CommandMode::vpotPressed()` — same logic, MIDI channel = 0xB0 + pageIndex.
- `CommandMode::updateVPOTs()` — already iterates `getNumberOfChannelStrips()`
  (widened in WP-F). VPOT LEDs are already per-channel via CCSManager.
- `CommandMode::updateDisplay()` — the MultiDisplay handles routing.
  `changeField(1, i+1, ...)` with global field numbers (1..N*8) routes to
  the correct unit's LCD.

**Result: essentially zero code changes needed.** The existing per-block-8
design + MultiDisplay routing + CCSManager channel translation makes
mirroring work automatically.

- **Page selector behavior:** When the user holds EQ to pick a page, the
  `CommandPageSelector` activates on all units' LCDs (via
  `MultiDisplay::switchToAll()`). VPOT 1 on the extender ALSO selects the
  page — this works automatically because `CCSManager::vpotPressed()` routes
  extender VPOTs to `CommandMode::vpotPressed()`, which calls the selector's
  `select(channel-1)`. The global channel → local channel translation in
  `CCSManager` makes this transparent. No code change needed, but worth
  documenting in the extender design doc.

### 5.3 What would Option B require?

If we later decide to go with Option B (extend banks):
- The `Page` array would need to map differently per unit (unit 0 → pages
  0-3, unit 1 → pages 4-7, etc.).
- `vpotMoved()` would need to know which unit's VPOT was moved to select the
  right page.
- The page selector (hold EQ) would need to show different page sets per unit.
- The editor would need per-unit page tabs or a unit selector.

This is a significant scope increase and should be its own WP.

---

## 6. Phase 5 — Global Actions Enhancement (stretch)

**Goal:** Improve the `Actions` / `ActionsDisplay` subsystem, which is separate
from `CommandMode`.

### 6.1 Add label editing UI for GlobalActions

Currently, `GlobalActions.xml` labels can only be edited by hand in the XML
file. There's no UI. Consider adding:
- A tab or button in the existing editor that lets users edit action display
  labels.
- Open question: is this valuable enough? The GlobalActions are rarely changed
  after initial setup. Defer unless requested.

### 6.2 Bundle preset action assignments

**Two distinct subsystems, two different presets:**

1. **CommandMode presets** (`ActionMode.xml`): The manual
   (`text_en/installation.tex`) describes importing action assignments for
   the VPOT-based CommandMode. We can ship a `DefaultActionMode.xml` in the
   distribution and copy it to the user config dir on first run (when no
   config file exists). This replaces the "No actions are named" experience
   with working VPOT action assignments.

2. **Actions subsystem** (`Actions::addActions()`): The ~124 global Reaper
   action registrations (rec, solo, mute, transport buttons, etc.) are
   **hardcoded** at compile time — the button-to-action mapping is a C++
   table in `Actions.cpp`, not a config file. What *could* be shipped is a
   `DefaultGlobalActions.xml` with sensible display labels for the
   16-modifier × 8-field LCD matrix (currently all empty strings).

These should not be conflated — the CommandMode preset covers VPOT
assignments; the Actions subsystem preset covers LCD display labels for the
hardware buttons.

### 6.3 Consider using `reaper_plugin_info_t::GetFunc("MIDI_GetRecentInputDevice")`

The `ActionsDisplay` header comment mentions "Using the MIDI-learn function
in the action display." Currently, the user must manually MIDI-learn each
action in Reaper's action list. There may be an API to automate this, but
it's not exposed in the public SDK. Research needed.

---

## 7. Implementation Timeline

| Phase | Description | Effort | Risk | Dependencies |
|---|---|---|---|---|
| **P1** | Bug fixes & polish (§2) | 3 hours | Low | None |
| **P2** | Config path unification (§3) | 2 hours | Low | P1 |
| **P3** | Import/Export (§4) | 6–8 hours | Medium | P1, P2 |
| **P4** | Multi-unit design document (§5) | 1 hour | N/A | (doc only) |
| **P5** | Global Actions enhancement (§6) | TBD | Medium | None |

**Suggested order:** P1 → P2 → P4 (doc) → P3 → P5

P1 and P2 are pure cleanup/fixes and should be done first. P3 adds the most
user-visible value. P4 is documentation that unblocks the extender per-mode
WP later. P5 is optional/stretch.

### Build & Test Checklist for Each Phase

After each phase:
```bash
# Linux
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(nproc)"
cp reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/

# macOS (if applicable)
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(sysctl -n hw.ncpu)"
cp reaper_csurf_mcu_klinke.dylib ~/Library/Application\ Support/REAPER/UserPlugins/

# Windows (from WSL)
./scripts/build-windows.sh
```

Test: load in Reaper, press EQ to enter Action Mode, verify display, open
editor with Alt+EQ, edit VPOT names, verify persistence across Reaper restart.

---

## 8. Files Affected Summary

| Phase | File | Change |
|---|---|---|
| P1 | `src/modes/commands/CommandMode.h` | Add `deactivate()` + `m_bConfigLoaded` member |
| P1 | `src/modes/commands/CommandMode.cpp` | Fix config path (add #ifdef), spelling (line 280), deactivate() save with guard |
| P1 | `src/action/ActionsDisplay.cpp` | Fix config path, add XML version attr |
| P2 | `src/core/ConfigPath.h` | **NEW** — shared config path helper |
| P2 | `src/modes/commands/CommandMode.cpp` | Use shared helper |
| P2 | `src/action/ActionsDisplay.cpp` | Use shared helper, simplify getConfigFile |
| P3 | `src/modes/commands/CommandMode.h` | Add importFile(), exportToFile(), resetToDefaults() |
| P3 | `src/modes/commands/CommandMode.cpp` | Implement import/export/reset |
| P3 | `src/modes/commands/editor/CommandModeMainComponent.h` | Add import/export/reset buttons |
| P3 | `src/modes/commands/editor/CommandModeMainComponent.cpp` | Wire button handlers |
| P3 | `CMakeLists.txt` | Add new files if any |
| P4 | `ai-docs/extender-commandmode-design.md` | **NEW** — extender design doc |
| P5 | TBD | TBD |

---

## 9. Open Questions

1. **Config directory:** Use `GetResourcePath()` on non-Windows (the Reaper
   resource dir, documented as "where ini files are stored" — writable on
   all platforms). On Windows use `userDocumentsDirectory` for backward
   compatibility with existing installs (matches `Options.cpp`). A future
   migration to `GetResourcePath()` on all platforms could clean this up,
   but requires migrating existing user configs.

2. **Import merge strategy:** Replace matching pages only, or replace all 8?
   Recommend: replace matching pages (indexed), leave others — users can
   selectively import.

3. **Default action assignments:** Should we ship a `DefaultActionMode.xml`?
   The manual references importing presets — if the original Klinke distribution
   had one, we should include it.

4. **Extender behavior:** Option A (mirror) is recommended. Confirm before
   implementation.

5. **`Actions` class lifetime:** The singleton `Actions::instance()` creates
   all ~150 action registrations at first use. It's destructed at plugin
   unload. Is there a memory leak concern with `m_literals` (heap-allocated
   char arrays that are never freed until dtor)? Not a leak — they're freed
   in the dtor. But the singleton pattern could be simplified.
