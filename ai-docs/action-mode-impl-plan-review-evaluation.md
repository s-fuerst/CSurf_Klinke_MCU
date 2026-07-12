# Evaluation of `action-mode-impl-plan-review.md`

> Date: 2026-07-12
> Evaluator: AI agent (codebase audit + claim verification)
> Verdict: The review contains **one major factual error**, several valid
> observations, and mixed-to-inaccurate severity ratings.

---

## Critical Error in the Review

### 🔴 Review Claim #1: "GetResourcePath() returns the Reaper executable directory on Linux — not writable"

**This is false.** The Reaper SDK documents `GetResourcePath()` (in
`reaper-sdk/sdk/reaper_plugin_functions.h:2605`) as:

> returns path where ini files are stored, other things are in subdirectories.

This is the user's Reaper resource directory:
- Windows: `%APPDATA%\REAPER\` (or portable install dir)
- macOS: `~/Library/Application Support/REAPER/`
- Linux: `~/.config/REAPER/`

**All three are writable.** The review confuses the *resource* directory
with the *program* directory (where the reaper binary lives).

Evidence from the existing codebase — every subsystem that writes files on
non-Windows uses `GetResourcePath()`:

| File | Path | Purpose |
|---|---|---|
| `Options.cpp:215` | `GetResourcePath() + "/MCU/Config/"` | Options XML config |
| `ActionsDisplay.cpp:79` | `GetResourcePath() + "/MCU/Config/"` | GlobalActions.xml |
| `PlugMapManager.cpp:75` | `GetResourcePath() + "/UserPlugins/MCU/PlugMaps/"` | Installed PlugMaps |
| `McuDebugLog.h:27` | `GetResourcePath()` | Debug log files |

This is the **established pattern across the codebase**. Following it is
correct, not "perpetuating a bug." The review's proposed alternative
(manually constructing `~/.config/REAPER/` via `userHomeDirectory`) is
equivalent on standard installs but **breaks portable installs** where
the resource dir may be elsewhere.

**Verdict: This is NOT a critical issue. The plan's approach (matching the
existing Options.cpp pattern) is correct.**

---

## Actually Broken: Current `CommandMode::getConfigFile()`

The review missed the **real** bug here.

`CommandMode::getConfigFile()` (line 296) has **no `#ifdef _WIN32` guard at all**:

```cpp
File CommandMode::getConfigFile() {
  File configDir =
      File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() +
      String("\\Reaper\\MCU\\Config\\");
  // NO platform branch! Always uses userDocumentsDirectory + backslashes
```

On Linux this resolves to something like `~/Documents\Reaper\MCU\Config\`
(with backslashes!), which is wrong regardless of whether JUCE normalizes the
separators. The fix should add `#ifdef _WIN32` / `#else` matching the pattern
used by `Options::getConfigFile()` and `ActionsDisplay::getConfigFile()`.

**The plan's §2.1 implicitly addresses this by proposing the new `#ifdef` /
`#else` pattern, but doesn't call out that the current code has NO platform
branch. This should be explicitly noted.**

---

## Valid Review Points (agreed)

### ✅ Review #2: `writeConfigFile()` in `~CommandMode` can write defaults

**Partially valid.** If `readConfigFile()` failed (no file, or corrupt file),
calling `writeConfigFile()` in the dtor would write default-constructed data.
This IS a risk, but:

1. It's **existing behavior** — the editor close already does this (open
   editor with corrupt file → see defaults → close editor → defaults written).
2. The scenario is unlikely: corrupt `ActionMode.xml` + no editor opened +
   clean shutdown.
3. The proposed `m_bConfigLoaded` guard is a clean fix.

**Severity should be: 🟡 Medium, not 🟠 High.** The bug already exists; the
plan doesn't introduce it, it just widens the trigger window from "editor
close" to "any dtor".

### ✅ Review #3: Destructor-based file I/O during Reaper shutdown is fragile

**Valid.** `XmlElement::writeToFile()` depends on JUCE's file I/O, which may be
partially torn down during plugin unload. The better approach is to save in
`deactivate()` (when switching AWAY from the mode). However:

- `CommandMode` does NOT currently override `deactivate()` (base class impl
  is empty), so a `deactivate()` override needs to be added.
- `deactivate()` is also called during shutdown (mode switch before unload),
  so it's the same teardown window — but at least it's more explicit.
- The safest approach: save in both `deactivate()` AND provide an explicit
  save that `CCSManager` calls before tearing down modes.

**Severity: 🟡 Medium (correct by review).** Not a showstopper, but the plan
should discuss the trade-off.

---

## Review Points That Need Correction

### ⚠️ Review #5: Stale line numbers

The review says "The spelling error is at line **269**, not ~140."

**Both are wrong.** The grep shows it's at line **280**:
```
280:	m_pDisplay->changeText(1, 0, "No actions are name for this bank...
```

The review criticizes the plan's line reference while getting it wrong
itself. Line numbers are expected to drift.

**Severity: 🟢 Trivial.** Line references in planning docs are approximate
by nature, confirmed by `grep` during implementation.

### ⚠️ Review #7: "No macOS path in any config-path discussion"

The review's own proposed fix uses `userApplicationDataDirectory` for macOS:

```cpp
#elif defined(__APPLE__)
  File dir = File::getSpecialLocation(File::userApplicationDataDirectory)
                 .getChildFile("Application Support/REAPER/MCU/Config");
```

**This is equivalent to `GetResourcePath()`** on macOS, which returns
`~/Library/Application Support/REAPER/`. Adding a separate macOS branch is
redundant — `GetResourcePath()` handles all platforms. The existing Options.cpp
pattern uses `#ifdef _WIN32` / `#else` (not `#elif __APPLE__`), and that's
sufficient.

The review also suggests:
```cpp
#else
  // Linux: ~/.config/REAPER/MCU/Config/
  File dir = File::getSpecialLocation(File::userHomeDirectory)
                 .getChildFile(".config/REAPER/MCU/Config");
```

This is equivalent to `GetResourcePath()` on standard Linux installs but
**breaks portable installs** where the resource dir may differ. `GetResourcePath()`
is the canonical API for a reason.

**Verdict: The review's "correct fix" is less correct than just using
`GetResourcePath()` consistently.**

---

## Missing Issues (not in the review)

### 🟡 Missing: `CommandMode::getConfigFile()` uses `\\Reaper\\MCU\\Config\\` on ALL platforms

The review focuses on the plan's proposed fix but misses the root cause: the
current function has no platform branch at all. On Linux/macOS, it resolves
`userDocumentsDirectory` (e.g., `~/Documents`) + Windows backslash path. This
is the real bug, and the review doesn't call it out explicitly.

### 🟢 Missing: `readConfigFile()` silently discards partial data

If only pages 0-3 parse successfully and page 4 is corrupt, `readFromXML()`
returns `false` for page 4, but pages 0-3 have already been mutated. The
function then returns `false` (fully failed). But the pages 0-3 data is
already applied — there's no rollback mechanism.

This is existing behavior, not introduced by the plan.

---

## Accuracy of Review Severity Ratings

| Review # | Review Severity | Actual Severity | Notes |
|---|---|---|---|
| 1 | 🔴 Critical | 🟢 No issue | `GetResourcePath()` IS writable on Linux — review is factually wrong |
| 2 | 🟠 High | 🟡 Medium | Existing behavior, unlikely trigger, easy to guard with flag |
| 3 | 🟠 High | 🟡 Medium | Valid concern but edge case; deactivate()-based save is better |
| 4 | 🟡 Medium | 🟡 Medium | Correct — import needs more spec detail |
| 5 | 🟡 Medium | 🟢 Trivial | Line numbers drift; both the plan AND review got this wrong (280, not 269 or 140) |
| 6 | 🟡 Medium | 🟡 Medium | Fair — P3 estimate is optimistic |
| 7 | 🟢 Low | 🟢 No issue | `GetResourcePath()` handles macOS too |
| 8 | 🟢 Low | 🟢 Low | Valid observation, useful for docs |
| 9 | 🟢 Low | 🟢 Low | Valid clarification |

---

## What the Review Gets Right

- The two-subsystem decomposition is correct ✅
- The import merge semantics discussion is useful ✅
- The CommandPageSelector mirroring observation is insightful ✅
- The JUCE teardown concern (save in dtor) is a real consideration ✅
- The effort estimate adjustment for P3 (4h → 6-8h) is reasonable ✅
- The Actions subsystem distinction (hardcoded vs. configurable) is important ✅
- The plan's overall structure and phase ordering is sound ✅
- The bottom line ("~80% solid") is fair ✅

---

## Summary

The review makes **one critical factual error** — its most severe finding
(🔴 Critical #1) is based on misunderstanding `GetResourcePath()`. The SDK
confirms it returns the writable Reaper resource directory on all platforms,
and the existing codebase already relies on this everywhere.

With that error corrected, the review's remaining findings are mostly valid
medium/low-severity observations. The plan should incorporate:

1. **Review #2 / #3**: Use `deactivate()` instead of (or in addition to) dtor
   for config save; add `m_bConfigLoaded` guard.
2. **Review #4**: Add import validation/rollback spec and version-attribute
   handling.
3. **Review #8**: Document CommandPageSelector mirroring behavior explicitly.
4. **Review #9**: Clarify CommandMode presets vs. Actions subsystem in §6.2.
5. **Noted gap**: The current `CommandMode::getConfigFile()` has no platform
   branch at all — the plan's fix handles this implicitly but should call it
   out.

The review's "correct fix" for config paths (manually constructing
`~/.config/REAPER/` etc.) is **not recommended** — `GetResourcePath()` is the
canonical Reaper API and the existing codebase pattern.
