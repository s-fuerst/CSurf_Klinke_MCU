# Critical Review: `action-mode-impl-plan.md`

> Date: 2026-07-12
> Reviewer: AI agent (codebase audit + plan analysis)
> Status: Review complete — 1 critical, 2 high, 3 medium, 2 low issues found

## Overall Assessment

The plan is **well-structured and mostly accurate**, but it has several significant technical oversights, an incomplete understanding of the existing code behavior, and one proposed fix that is actively wrong. Below is a phase-by-phase analysis with findings, severity ratings, and concrete corrections.

---

## §1 — Current State Inventory

**Accuracy: Good, with one important omission.**

The inventory correctly identifies the two subsystems (Actions + CommandMode) and their file layouts. However, §1.1 understates the `Actions` subsystem:

> `Actions::instance()` creates ~150 Reaper actions

It's actually **~124 actions** (8×(Rec, Solo, Mute, Select, VPush, F-keys, GlobalView, FaderTouch, FXFavorite) + ~20 singletons + toggle variants). The "~150" counts both key and button variants as separate — which is correct because `addKeyAction` internally calls `addButtonAction`, giving each physical button *two* Reaper command IDs (one-shot "key" + toggle "button"). The plan should mention this because any import/export of action bindings must handle both variants.

The gap list in §1.2 is mostly correct, but item 5 ("config save on CommandMode destruction") is incompletely diagnosed — see §2.4 below.

---

## §2 — Phase 1: Bug Fixes

### §2.1 — Fix config file path (CommandMode.cpp)

**Severity: 🔴 CRITICAL — proposed fix is wrong for Linux**

The plan's proposed fix:

```cpp
#ifdef _WIN32
  File configDir = File::getSpecialLocation(File::userApplicationDataDirectory)
                       .getChildFile("REAPER/MCU/Config");
#else
  File configDir = String(GetResourcePath()) + "/MCU/Config";
#endif
```

- **Windows branch**: ✅ Correct. Reaper uses `%APPDATA%\REAPER\` (`userApplicationDataDirectory`), not `userDocumentsDirectory`.
- **Linux branch**: ❌ Wrong. `GetResourcePath()` returns the Reaper *executable* directory (e.g., `/opt/REAPER/`), which is **not writable** for normal users. Reaper's convention on Linux is `~/.config/REAPER/`.

**Correct fix:**

```cpp
File getMcuConfigDir() {
#ifdef _WIN32
  File dir = File::getSpecialLocation(File::userApplicationDataDirectory)
                 .getChildFile("REAPER/MCU/Config");
#elif defined(__APPLE__)
  File dir = File::getSpecialLocation(File::userApplicationDataDirectory)
                 .getChildFile("Application Support/REAPER/MCU/Config");
#else
  // Linux: ~/.config/REAPER/MCU/Config/
  File dir = File::getSpecialLocation(File::userHomeDirectory)
                 .getChildFile(".config/REAPER/MCU/Config");
#endif
  if (!dir.exists())
    dir.createDirectory();
  return dir;
}
```

Or better: use `userApplicationDataDirectory` consistently on all platforms with the platform-appropriate sub-path.

**Additional cleanup:** The current `CommandMode::getConfigFile()` has a confusing condition:

```cpp
if (!configDir.exists() ||
    !File(configDir.getFullPathName() + AM_FILE).exists()) {
  configDir.createDirectory();
}
```

This calls `createDirectory()` when the directory doesn't exist OR the file doesn't exist. If the directory *does* exist but the file doesn't, it re-creates the directory (harmless no-op). If both exist, `createDirectory()` is *not* called — which is fine since the directory already exists. The logic works by accident. The simpler pattern:

```cpp
if (!configDir.exists())
  configDir.createDirectory();
```

suffices, and the plan's proposed fix uses this.

---

### §2.2 — Fix spelling error

**Severity: ✅ Correct, minor line-number issue**

`"No actions are name for this bank"` → `"No actions are named for this bank (press Alt+EQ)."`

Verified in source. The `+` and `Alt+EQ` match existing conventions.

**Correction:** The plan says line ~140 — the actual line is **269** in the current `CommandMode.cpp`. The plan's line reference is stale.

---

### §2.3 — Fix `ActionsDisplay::getConfigFile()`

**Severity: ⚠️ Under-specified**

The plan says "Same pattern as above" but doesn't show the actual fix. The current code already has `#ifdef _WIN32` / `#else` branches but uses `userDocumentsDirectory` on Windows. The fix should change Windows to `userApplicationDataDirectory` and Linux to `~/.config/REAPER/…` (see §2.1).

The `bLookAtProgramDir` parameter (to be removed in §3.3) currently serves as a no-op on non-Windows — both `getConfigFile(true)` and `getConfigFile(false)` return the same path. After unification it will be dead on all platforms; removing it is correct.

---

### §2.4 — Ensure config is saved on `CommandMode` destruction

**Severity: 🟠 HIGH — two problems**

The plan correctly identifies that `~CommandModeMainComponent` calls `writeConfigFile()` but `~CommandMode` does not. The proposed fix:

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

**Problem 1: Default-data overwrite**

If `readConfigFile()` failed (e.g., no config file exists yet) and the mode is destroyed, `writeConfigFile()` will write **default-constructed data** to disk — emptying all pages and overwriting any manually-edited `ActionMode.xml`. The fix should either:

- Guard with a `m_bConfigLoaded` flag (set on successful read), or
- Accept this as intended behavior (matches editor-close semantics).

**Problem 2: JUCE teardown during Reaper shutdown**

`writeConfigFile()` uses `XmlElement::writeToFile()`, which depends on JUCE's file I/O and possibly `MessageManager`. During Reaper plugin unload, JUCE subsystems may already be partially torn down. A safer approach: save in `CommandMode::deactivate()` (called when switching *away* from the mode), not in the destructor. The plan doesn't mention this alternative at all.

---

### §2.5 — Add XML version attribute to ActionsDisplay

**Severity: ✅ Correct, low-risk**

Trivial. The plan notes the `<ACTIVE_MODE_CONFIG>` (CommandMode) already has `version="1"`. Good for migration safety.

---

## §3 — Phase 2: Config Path Unification

### §3.1 — Define shared helper `getMcuConfigDir()`

**Severity: ⚠️ Same Linux path problem as §2.1**

The proposed inline function repeats the `GetResourcePath()` mistake for Linux. See §2.1 critique.

**Design note:** Placing this in a new header `src/core/ConfigPath.h` as `inline` creates an include dependency on `JuceHeader.h`. Since most files already transitively include it through `csurf_mcu.h` → `CCSModesEditor.h`, this is probably fine. But an `inline` free function is duplicated into every translation unit; a `.cpp` file with a non-inline function would be cleaner.

### §3.2 & §3.3 — Migration and simplification

**Severity: ✅ Correct**

Both are straightforward refactors. The `bLookAtProgramDir` parameter removal is correct since unified paths make it dead code.

---

## §4 — Phase 3: Import/Export

### §4.1 — Design

**Severity: ✅ Good UX, merge semantics need clarification**

The three buttons (Import, Export, Reset) are well-chosen. Merge on import is the right default.

### §4.2 — Implementation

**Severity: 🟡 MEDIUM — missing details**

The plan doesn't specify:

1. **Button placement**: "top or bottom of the editor window" — but `CommandModeMainComponent` is a `TabbedComponentWithCallback` with 8 tabs. Adding buttons requires layout changes. What happens if the user is on page 3 when they click Import? Does it import to all pages or just the current one?

2. **Validation & rollback**: `importFromFile()` should validate the XML structure before replacing any pages. A partial failure (e.g., page 3 corrupt, pages 0-2 valid) needs a policy: roll back entirely, or apply only valid pages with error reporting.

3. **File chooser filter**: No `.xml` extension filter specified.

4. **Error UX**: Malformed XML → `MessageBox` or silent failure? Must be specified.

### §4.3 — Merge semantics

**Severity: 🟡 MEDIUM — incomplete version handling**

"Replace pages present in the file, leave others unchanged" — good. But what about the `<ACTIVE_MODE_CONFIG>` root version attribute? If the import file has a different version than the current code expects, should we reject, warn, or silently upgrade? §2.5 added version attributes specifically for migration safety, but the plan doesn't use them here.

---

## §5 — Phase 4: Multi-Unit Extender Design

**Severity: ✅ Analysis correct, minor gap**

### §5.1 — Design Decision: Option A (Mirror)

The recommendation aligns with the master plan's "linked mode" default. All three options are well-characterized.

### §5.2 — Implementation sketch (Option A)

Correctly observes that mirroring is essentially free:
- `MultiDisplay::switchToAll()` handles display routing ✅
- VPOT CC generation is page-index-based, not unit-based ✅
- VPOT LEDs iterate `getNumberOfChannelStrips()` (already widened) ✅

**Minor gap:** The plan doesn't discuss the `CommandPageSelector` interaction. When the user holds EQ to pick a page, all units show the page selector on their LCDs. Does VPOT 1 on the extender also select pages? With mirroring it should — and it already does because `CCSManager` routes VPOT→global-channel and `CommandMode` is active regardless of which unit generated the event. This works by accident, but should be explicitly noted.

### §5.3 — Option B sketch

**Severity: ✅ Accurate**

Fairly describes why Option B is a separate WP with significant scope increase.

---

## §6 — Phase 5: Global Actions Enhancement

### §6.1 — Label editing UI

**Severity: ✅ Correctly deferred**

### §6.2 — Bundle preset action assignments

**Severity: 🟢 LOW — missing Actions-subsystem distinction**

The plan notes shipping a `DefaultActionMode.xml` for CommandMode. But it doesn't clarify whether this extends to the `Actions` subsystem (the ~124 global Reaper actions in `Actions::addActions()`). Those are **hardcoded** — the button-to-action mapping is a compile-time table, not a config file. What *could* be shipped: `GlobalActions.xml` (display labels for the 16-modifier×8-field matrix) with sensible defaults. The plan should explicitly distinguish this.

### §6.3 — `MIDI_GetRecentInputDevice`

**Severity: 🟢 LOW — correctly marked as research-needed**

The plan correctly notes this API isn't in the public SDK. Should be marked "research only, likely infeasible."

---

## §7 — Implementation Timeline

**Severity: 🟡 MEDIUM — estimates are optimistic**

| Phase | Plan Estimate | Realistic Estimate | Notes |
|---|---|---|---|
| P1 | 2 hours | 3 hours | Config path fix requires 3-platform testing |
| P2 | 1 hour | 2 hours | New header + refactor 2 consumers + remove parameter + cross-platform test |
| P3 | 4 hours | 6–8 hours | 3 buttons + file choosers + import validation/rollback + XML I/O + testing |
| P4 | 1 hour | 1 hour | Documentation only, correct |
| P5 | TBD | — | Correctly marked as stretch |

**Realistic total:** Still ~3 days, but the per-phase distribution is wrong — P2+P3 need more time than allocated.

---

## §8 — Files Affected Summary

**Severity: ✅ Accurate but one missing item and one stale line number**

- The spelling error line is **269**, not ~140 (as noted in §2.2).
- Missing: Phase 1 also touches `src/modes/commands/CommandMode.cpp` for the destructor fix (§2.4) which the table lists under P1 — that's correct.

---

## §9 — Open Questions

**Severity: ⚠️ Question 1 answer is wrong**

> Q1: "Should we use `GetResourcePath()` (Reaper resource dir) or a custom path?"

The plan answers itself: "Current code uses resource path on non-Windows — match that for consistency."

**This is wrong.** The current Linux behavior is itself a bug (writes to install directory, unwritable for normal users). Consistency with a bug is not a virtue.

**Correct answer:** Use platform-appropriate user config directories on all platforms:

| Platform | Path |
|---|---|
| Windows | `%APPDATA%\REAPER\MCU\Config\` |
| macOS | `~/Library/Application Support/REAPER/MCU/Config/` |
| Linux | `~/.config/REAPER/MCU/Config/` |

Questions 2-5 are correctly stated and the recommendations are sound.

---

## Summary of All Issues

| # | Severity | Issue | Location in plan |
|---|---|---|---|
| 1 | 🔴 Critical | Config path uses `GetResourcePath()` on Linux (unwritable install dir) — plan perpetuates this bug in proposed fix | §2.1, §3.1, §9 Q1 |
| 2 | 🟠 High | `writeConfigFile()` in `~CommandMode` overwrites config with defaults if `readConfigFile()` never succeeded | §2.4 |
| 3 | 🟠 High | Destructor save may fail during Reaper shutdown (JUCE teardown). `deactivate()` is safer. | §2.4 |
| 4 | 🟡 Medium | Import feature lacks XML validation/rollback on partial failure and version-attribute handling | §4.2, §4.3 |
| 5 | 🟡 Medium | Stale line number: spelling error is at line 269, not ~140 | §2.2, §8 |
| 6 | 🟡 Medium | Effort estimates for P2+P3 are optimistic (1h→2h, 4h→6-8h) | §7 |
| 7 | 🟢 Low | No macOS path in any config-path discussion | §2.1, §3.1, §9 |
| 8 | 🟢 Low | §5 doesn't discuss CommandPageSelector interaction with mirroring (works by accident, but should be noted) | §5.2 |
| 9 | 🟢 Low | §6.2 doesn't distinguish CommandMode presets from Actions subsystem (hardcoded) | §6.2 |

---

## What the plan gets right

- The two-subsystem decomposition (Actions vs. CommandMode) is correct
- The phase ordering (P1→P2→P4→P3→P5) is sensible
- Option A (mirror) for extender behavior is the right call
- The spelling fix and XML versioning are clean, low-risk improvements
- The merge semantics for import (replace matching pages, leave others) is well-chosen
- The file-affected summary in §8 is comprehensive
- The companion-document references (extender-support.md, extender-wp-f-widening-audit.md) are accurate
- The build-and-test checklist is practical and covers all platforms

---

## Bottom Line

The plan is **~80% solid**. The config-path bug (#1) is a showstopper that must be fixed before Phase 1+2 implementation begins. The destructor-save concern (#2-3) should be resolved, and the import feature (#4) needs more spec detail. With those items addressed, the plan is ready to execute.
