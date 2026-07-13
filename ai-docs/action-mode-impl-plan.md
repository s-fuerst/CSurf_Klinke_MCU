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
not ground-up construction. The work is organized into 3 phases, each
independent and buildable in isolation.

**Total estimated effort:** ~1.5–2 days (see timeline in §5).

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
3. **Action assignment is manual by design** — users assign VPOT CCs to
   Reaper actions via MIDI-learn in Reaper's Actions dialog (as documented
   in the manual, §\ref{globalactions}). The extension provides the
   CC-generating VPOTs and the on-screen editor for labels/banks; Reaper
   handles the actual action binding. There is no import/export mechanism
   for assignments — and there will not be one.
4. **`writeConfigFile()` called only on editor close** (`~CommandModeMainComponent`).
   If the mode is deactivated without opening/closing the editor (e.g. Reaper
   shutdown), changes may be lost. The dtor of `CommandMode` does NOT call
   `writeConfigFile()` — only the editor dtor does.
5. **Multi-unit extender design unresolved** — resolved in §4 (Phase 3):
   8 global pages with a per-unit active-page cursor.

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

**Problem:** The current code has **no platform branch at all** — it uses
`userDocumentsDirectory` with Windows backslash paths on every platform.
On Linux this resolves to something like `~/Documents\Reaper\MCU\Config\`
which is wrong regardless of whether JUCE normalizes the separators.

**Fix:** Use `GetResourcePath()` on all platforms, following the established
pattern from `Options::getConfigFile()` (`src/core/Options.cpp:215`):

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

`GetResourcePath()` returns the writable Reaper resource directory on all
platforms (`~/.config/REAPER/` on Linux, `~/Library/Application Support/REAPER/`
on macOS, `%APPDATA%/REAPER/` on Windows) and handles portable installs correctly.

**Files:** `src/modes/commands/CommandMode.cpp` (lines ~283-291)

### 2.2 Fix spelling error in display message

**Current:** `"No actions are name for this bank (press Alt-EQ)."`

**Fix:** `"No actions are named for this bank (press Alt+EQ)."`

**Files:** `src/modes/commands/CommandMode.cpp` (line ~280)

### 2.3 Fix `ActionsDisplay::getConfigFile()` cross-platform path

Same pattern as above — ensure consistent with CommandMode.

**Files:** `src/action/ActionsDisplay.cpp` (lines ~85-99)

### 2.4 Ensure config is saved on mode deactivation

**Current:** `writeConfigFile()` is only called from
`~CommandModeMainComponent()`. If the editor was never opened, or if Reaper
shuts down without closing the editor, changes may be lost.

**Problem with dtor-based save:** Saving in `~CommandMode()` is fragile for
two reasons:

1. **Default-data overwrite:** If `readConfigFile()` failed (e.g. no config
   file exists yet) and the mode is destroyed, `writeConfigFile()` would write
   default-constructed data to disk — emptying all pages and overwriting any
   manually-edited `ActionMode.xml`.

2. **JUCE teardown during Reaper shutdown:** `XmlElement::writeToFile()`
   depends on JUCE's file I/O and possibly `MessageManager`. During plugin
   unload, JUCE subsystems may already be partially torn down.

**Fix:** Save in `CommandMode::deactivate()` (called when switching AWAY from
the mode), guarded by a `m_bConfigLoaded` flag:

```cpp
// CommandMode.h — add member:
bool m_bConfigLoaded;  // set true on successful readConfigFile()

// CommandMode.cpp — override deactivate():
void CommandMode::deactivate() {
  if (m_bConfigLoaded) {
    writeConfigFile();
  }
  MultiTrackMode::deactivate();  // call base class
}

// CommandMode.cpp — in readConfigFile(), after successful parse:
m_bConfigLoaded = true;
```

`deactivate()` is also called during shutdown (mode switch before unload),
providing a more explicit save point than the destructor. The
`m_bConfigLoaded` guard prevents overwriting the config file with defaults
when no config was ever successfully loaded.

**Files:** `src/modes/commands/CommandMode.h` (add `deactivate()` declaration
+ `m_bConfigLoaded` member), `src/modes/commands/CommandMode.cpp` (add
`deactivate()` implementation + `m_bConfigLoaded = true` in `readConfigFile()`)

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

## 4. Phase 3 — Multi-Unit Per-Unit Page Selection (implementation)

**Goal:** With more than one hardware unit connected, each unit independently
selects which of the 8 shared pages (banks) it displays and sends CCs from.
Default: unit 0 → page 0, unit 1 → page 1, etc. The page selector (hold EQ)
picks the page for the unit whose VPOT was pressed.

Per `extender-wp-f-widening-audit.md` §3.4, the `CommandMode` 8s are
**per-block-8 (VPOTs/banks/pages)**, not surface-channel counts — they are
already correct for channel strips 9+. The work here is purely the
per-unit *active-page cursor*.

### 4.1 Background: global channel → unit + local channel

`CCSManager` dispatches VPOT events with the **global channel** (1-based,
1..N*8). The two helpers in `CSurf_MCU` (`csurf_mcu.h:435`) split it:

```cpp
HardwareUnit *unitForChannel(int g) const { return m_units[(g - 1) / 8]; }
static int localOf(int g) { return (g - 1) % 8 + 1; }  // 1-based local 1..8
int availableChannels() const { return numUnits() * 8; }
```

So for global channel `g`: **unit index = `(g-1)/8`**, **local channel =
`(g-1)%8`** (0-based). The synthetic MIDI CCs that `CommandMode` generates
must use the *local* channel for `byte1` (VPOT within the unit) and the
*unit's active page* for `byte0` (the CC's MIDI channel = page index).

### 4.2 Data structure change (`CommandMode.h`)

Replace the single active-page pointer with a per-unit page index. There are
at most 8 units (`SurfaceConfig` has 8 fixed entries):

```cpp
// CommandMode.h — replace:
//   Page *m_pActivePage;
// with:
  int m_iActivePageIndex[8];  // [unit] -> page index 0..7

// helper: which page is active for the unit owning global channel g?
  int activePageIndexForChannel(int g) const {
    return m_iActivePageIndex[(g - 1) / 8];
  }
```

`CommandPageSelector::select()` currently asserts `index < 8` on a *global*
channel, which would fire for channel 9+. The selector must instead compute
unit + local page from the global channel (see §4.5).

### 4.3 Constructor init (`CommandMode.cpp`)

Default the per-unit cursor to "unit x → page x" (clamped to 7):

```cpp
CommandMode::CommandMode(CCSManager *pManager) : MultiTrackMode(pManager) {
  for (int i = 0; i < 8; i++) {
    m_pPage[i] = new Page(this, i);
    m_iActivePageIndex[i] = i;  // unit x -> page x (clamped to 0..7)
  }
  readConfigFile();
  m_pSelector = new CommandPageSelector(pManager->getDisplayHandler(), this);
  m_pMainComponent = NULL;
}
```

(`m_pActivePage` is gone; every read site is converted below.)

### 4.4 VPOT event routing (`vpotMoved` / `vpotPressed`)

Both currently do `channel--` then use `m_pActivePage->m_iIndex` for `byte0`
and the (now global) `channel` for `byte1 = 0x10 * channel`. Fix: split into
unit + local channel, look up the unit's page:

```cpp
bool CommandMode::vpotMoved(int channel, int numSteps) {
  int unit = (channel - 1) / 8;
  int localChan = (channel - 1) % 8;          // 0-based
  unsigned char midi_byte0 = 0xb0 + m_iActivePageIndex[unit];
  int shift = m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT) ? 1 : 0;
  if (shift)
    midi_byte0 += 0x08;
  unsigned char midi_byte1 = 0x10 * localChan;
  // ... rest unchanged, using m_pPage[m_iActivePageIndex[unit]] for speed/relative
  Page *pActive = m_pPage[m_iActivePageIndex[unit]];
  if (pActive->m_bRelative[shift][localChan] == true) { /* ... */ }
  // ...
}
```

`vpotPressed()` gets the identical `unit`/`localChan` split; `byte1 =
0x10 * localChan + (pressed ? 0x06 : 0x07)`. The VPOT-LED lookup
`getVPOT(channel + 1)` in `vpotMoved` already uses the *global* channel and
stays as-is (VPOT LEDs are per-channel via CCSManager — §4.6).

### 4.5 Display rendering (`updateDisplay`)

Currently line 1 is filled from the single `m_pActivePage`. Because
`MultiDisplay::changeField(row, globalField 1..N*8)` already routes each
global field to its owning unit's LCD, line 1 just needs to be filled
per-global-field from that field's unit's active page:

```cpp
void CommandMode::updateDisplay() {
  MultiTrackMode::updateDisplay();
  // ... ProX row-3 block unchanged (already global-channel aware) ...

  int shift = m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT) ? 1 : 0;
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();

  // any unit has at least one named command on its active page?
  // (drives the "No actions are named" fallback per unit)
  m_pDisplay->clearLine(1);
  for (int g = 1; g <= nStrips; g++) {
    int unit = (g - 1) / 8;
    int localChan = (g - 1) % 8;
    String name = m_pPage[m_iActivePageIndex[unit]]->getCommandName(shift, localChan);
    m_pDisplay->changeField(1, g, name.toRawUTF8());
  }
}
```

The single "No actions are named" fallback line was a single-page concept;
with per-unit pages it is dropped (empty fields render blank per unit). If a
unit-wide fallback is still wanted, it must be decided *per unit* (e.g.
blank vs. the hint) — see §4.7.

### 4.6 Page selector (`CommandPageSelector`)

Two changes: (a) `activateSelector()` must show on **all** units (so any unit
can pick), and (b) `select()` must set the page for the picking unit only.

```cpp
class CommandPageSelector : public Selector {
public:
  void activateSelector() {
    MultiDisplay *md = dynamic_cast<MultiDisplay *>(m_pDisplay);
    for (int i = 0; i < 8; i++)
      md->broadcastField(1, i + 1,
          m_pCommandMode->m_pPage[i]->m_strPageName.toRawUTF8());
    // show the picker on every unit
    if (md) md->switchToAll();
    else m_pCommandMode->m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
  }

  // select() is called by CCSManager with (globalChannel - 1)
  bool select(int globalIndex) {
    int unit = globalIndex / 8;
    int localPage = globalIndex % 8;       // 0..7
    m_pCommandMode->m_iActivePageIndex[unit] = localPage;
    return false;  // close selector globally after one pick
  }
};
```

Notes:
- The page name on field `i+1` is the same on every unit, so `broadcastField`
  (sends one field to all children) is the right primitive — or simply
  `changeField(1, i+1, ...)` per global field, since all units share the
  same 8 page names.
- The selector closes globally after one pick (CCSManager calls
  `m_pActualMode->activate()` + `m_selectorActive = false`). Each unit's
  `updateDisplay()` then renders its own (possibly new) active page.

### 4.7 Edge cases & decisions

- **`updateVPOTs()`** needs no change: it already iterates
  `getNumberOfChannelStrips()` and the LED ring is per-channel. ✅
- **Editor** (`CommandModeMainComponent`) edits the 8 global pages — unchanged.
  A page edited for one unit is instantly the same page for any unit viewing
  it; `updateDisplay()` reflects each unit's cursor. ✅
- **"No actions are named" hint:** with per-unit cursors the global hint line
  no longer fits. Decision: render blank fields per unit (no hint), or keep
  the hint only when the *transport/main unit's* active page is empty. Pick
  the simpler blank-field behaviour unless the maintainer prefers the hint.
- **Single-unit (N=1):** `(g-1)/8 == 0` for all g, so behaviour collapses to
  the status quo — page 0 active, identical to today. ✅
- **`supportsExtendedChannels()`:** `CommandMode` must return `true` so
  `CCSManager` forwards channels > 8 (already the case after WP-F; verify).

### 4.8 Build & test

After implementation:
```bash
# Linux (needs a 2-unit surface config to exercise multi-unit)
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(nproc)"
cp reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

Test matrix:
- **N=1:** unchanged behaviour — page 0 active, hold EQ + VPOT picks page,
  CC MIDI channel = page index, single-unit display correct.
- **N=2:** unit 0 defaults to page 0, unit 1 to page 1; hold EQ + VPOT on
  unit 1 changes unit 1's page only; unit 0's display/page unchanged;
  each unit's line 1 shows its own page's names; CC byte0 reflects the
  picking unit's page index.

---

## 5. Implementation Timeline

| Phase | Description | Effort | Risk | Dependencies |
|---|---|---|---|---|
| **P1** | Bug fixes & polish (§2) | 3 hours | Low | None |
| **P2** | Config path unification (§3) | 2 hours | Low | P1 |
| **P3** | Multi-unit per-unit page selection (§4) | 4–6 hours | Medium | P1 |

**Suggested order:** P1 → P2 → P3

P1 and P2 are pure cleanup/fixes and should be done first. P3 implements the
per-unit page cursor and requires a multi-unit surface config to test.

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

## 6. Files Affected Summary

| Phase | File | Change |
|---|---|---|
| P1 | `src/modes/commands/CommandMode.h` | Add `deactivate()` + `m_bConfigLoaded` member |
| P1 | `src/modes/commands/CommandMode.cpp` | Fix config path (add #ifdef), spelling (line 280), deactivate() save with guard |
| P1 | `src/action/ActionsDisplay.cpp` | Fix config path, add XML version attr |
| P2 | `src/core/ConfigPath.h` | **NEW** — shared config path helper |
| P2 | `src/modes/commands/CommandMode.cpp` | Use shared helper |
| P2 | `src/action/ActionsDisplay.cpp` | Use shared helper, simplify getConfigFile |
| P3 | `src/modes/commands/CommandMode.h` | Replace `m_pActivePage` with `m_iActivePageIndex[8]` + helper; update `CommandPageSelector` |
| P3 | `src/modes/commands/CommandMode.cpp` | Per-unit page lookup in ctor, `vpotMoved`, `vpotPressed`, `updateDisplay` |
| P3 | `src/modes/commands/CommandMode.h` (`CommandPageSelector`) | `select(globalIndex)` splits unit/local; `activateSelector()` uses `switchToAll` |

---

## 7. Open Questions

1. **Config directory:** ✅ **RESOLVED** — Use `GetResourcePath()` on all
   platforms, following the existing `Options.cpp` pattern:
   `#ifdef _WIN32` → `userApplicationDataDirectory`, `#else` → `GetResourcePath()`.
   This is the canonical Reaper API (writable on all platforms, handles
   portable installs) and already used by `Options.cpp`, `ActionsDisplay.cpp`,
   `PlugMapManager.cpp`, and `McuDebugLog.h`.

2. **Extender behavior:** ✅ **RESOLVED** — **8 global pages, per-unit page
   selection.** Each unit independently chooses which of the 8 shared pages
   it displays. Default: unit 0 → page 0, unit 1 → page 1, etc. The page
   selector (hold EQ) selects the page for the unit whose VPOT was pressed.
   This is a hybrid: pages are shared (not per-unit), but each unit has its
   own page cursor. Fully specified in §4 (Phase 3), including the
   global-channel → unit/local split, per-unit `m_iActivePageIndex[8]`, and
   the per-unit page selector.

3. **"No actions are named" hint with per-unit cursors:** With N>1 the
   single global hint line no longer applies. §4.7 proposes blank fields per
   unit (simpler) vs. hint-on-main-unit-only. Confirm the blank-field choice
   unless the maintainer prefers the hint.

4. **`Actions` class lifetime:** ✅ **RESOLVED** — Leave the singleton as-is.
   No leak, works correctly, not worth refactoring.
