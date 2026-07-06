# Directory Restructure Plan — v3 (current-state re-assessment + revised target)

> **Status:** Proposal — not yet executed.
> **Date:** 2026-07-06
> **Supersedes:** `directory-restructure-plan.md` (v1, 2026-06-22) and
> `directory-restructure-plan2.md` (v2 critique, 2026-06-24).
> **Method:** re-checked against the live tree on 2026-07-06 (accurate file
> counts, real include graph, current CMakeLists.txt, git state, remotes).

---

## TL;DR — what changed since v2, in one paragraph

v2 said **"don't restructure yet"** for three reasons: (1) it would brick the
only working Windows build path (`.vcxproj`), (2) active upstream merges would
cause merge hell, (3) the tree wasn't clean. **All three are now resolved.**
The Windows CMake build is finished and works via two script paths
(`scripts/build-windows.sh` + `scripts/build-windows-fast.sh`); the `.vcxproj`
is frozen at 2024-07-14 and references dead JUCE-1.52/Boost-1.39 env vars; the
only upstream remote is our own `origin` fork (no active upstream integration).
The v2 *design* critiques (categories, include strategy) are still correct and
are folded into this plan. **Conclusion: a restructure is now viable and worth
doing, in two tiers — a zero-risk cleanup tier (do now) and the structural
`src/` move tier (one well-secured mechanical commit).**

---

## 1. Accurate baseline (2026-07-06)

| Metric | v1 claimed | v2 corrected | **v3 actual** |
|---|---|---|---|
| `.cpp` files in repo root | (67) | 66 | **66** |
| `.h` files in repo root | (—) | 71 | **73** |
| **Total source files** | 67 | 137 | **139** |
| Quoted `#include "..."` lines | ~200 | 366 | **437** |
| Distinct quoted include targets | — | 81 | **93** |
| Angle-bracket `#include <...>` | — | — | 62 |

Most-included headers (why a path-rewrite strategy is brutal):

```
44  csurf_mcu.h
40  JuceHeader.h
18  csurf.h
17  McuAssert.h
16  Display.h
15  Tracks.h
13  PlugMap.h
12  PlugAccess.h
11  PlugMode.h
10  PlugModeComponent.h
```

Top-level layout now (was just `docs/` + flat root in v2's day):

```
boost_1_91_0/   build/   dist/   docker/   docs/   juce_8/
JUCE-changes/   manual/  posix_shims/   reaper-sdk/   scripts/
```

---

## 2. Re-evaluation of v2's blockers (all resolved)

| v2 blocker | Status now | Evidence |
|---|---|---|
| **Bricks the only Windows build path (`.vcxproj`)** | **GONE.** CMake is source of truth on all 3 platforms. | `CMakeLists.txt` WIN32 branch fully implemented; `scripts/build-windows.sh` (verified working, MEMD 2026-07-05) + `scripts/build-windows-fast.sh` (the `/mnt/c` fast path v2's MEMD called "unimplemented" — now implemented). |
| **`.vcxproj` is fragile / must be updated** | **GONE — it's dead.** | Last touched `2024-07-14` (commit `651d872`, pre-revival). Still references `$(JUCE)` / `$(BOOST)` / `$(REAPER_EXTENSION_SDK)` env vars 16× — pointing at the **old** JUCE 1.52 / Boost 1.39 roots that no longer exist. Nobody builds from it. |
| **Active upstream merges → merge hell** | **GONE.** | `git remote -v`: only `origin` (bitbucket fork). No `upstream` / `original-klinke` remote configured. No active upstream integration to collide with. |
| **Tree not clean** | **Trivial.** | Only `VERSION.txt` is dirty (auto-incremented build counter — expected; commit or `git checkout` before the move). |

**The only residual v2 concerns are *design* ones** (categories, include
strategy) — addressed in §3 and §4. There is no longer a *timing* reason to
defer.

---

## 3. v2 design critiques — confirmed against the live include graph

v2 made several cohesion claims. I verified each by reading actual `#include`
directives. Results:

### 3a. MeterBridge subclasses couple to their modes — CONFIRMED
```
PlugModeMeterBridge.cpp      → #include "PlugMode.h", "PlugAccess.h"
MultiTrackMeterBridge.cpp    → #include "Tracks.h"
SendReceiveMeterBridge.cpp   → #include "Tracks.h", "SendReceiveModeBase.h"
```
**Action:** each subclass moves **into its mode's directory**. Only the
abstract base `MeterBridge.{cpp,h}` stays in a shared location. (v1's flat
`meter/` folder pulled cohesive code apart for no benefit.)

### 3b. `editor/` subfolder inconsistency — fix with an objective rule
v1 gave PlugMode an `editor/` but not CommandMode, even though both have a
parallel set of `*Component` editor files. **Rule for v3:** every file whose
name ends in `Component` (the project's JUCE on-screen editor convention) goes
into that mode's `editor/` subfolder. This is objective and applies uniformly:

- `commands/editor/` ← `ActionsDialogComponent`, `CommandModeMainComponent`,
  `CommandModePageComponent`, `CommandModeVPOTComponent`
- `multitrack/editor/` ← `TrackStatesEditorComponent`, `TrackStatesTableComponent`
- `plugin/editor/` ← all 14 `PlugMode*Component` files

### 3c. `state/` grab-bag — keep, but acknowledge it
`Tracks`, `Transport`, `VPOT_LED`, `Region`, `Options`, `ProjectConfig`,
`UndoEnd`. Renaming/splitting (`transport/`, `config/`, …) was v2's suggestion,
but it adds nesting without clarifying much — these are all "domain/track state
& config" and the bucket is honest enough. **Keep `state/`**; do not
over-engineer.

### 3d. `ActionsDisplay` — goes to `modes/commands/` (behavioral cohesion wins)
The compile-time include graph looks like the **inverse** of the MeterBridge
case (`ActionsDisplay.cpp` couples to `Display`/`DisplayHandler`/`csurf_mcu`,
not to `Actions.h`/`CommandMode.h`; it is even owned as a `CSurf_MCU` member).
On structure alone it would stay in `display/`.

**But the maintainer's domain knowledge overrides structure here** (2026-07-06):
the labels `ActionsDisplay` renders (`m_strLabel[16][8]`, persisted as
`GlobalActions.xml`) **are the actions the user triggers in Action Mode**. The
Name/Value button (CC `0x34`, global handler `OnNameValue` in `ButtonManager.cpp:91`)
shows them on demand. Semantically this is Action-Mode display content, even
though the activation wiring happens to live in core and runs mode-independently.

For a directory layout humans navigate by, **behavioral/semantic cohesion beats
compile-time coupling**. **Decision: move `ActionsDisplay.{cpp,h}` to
`modes/commands/`** alongside `Actions.{cpp,h}` and `CommandMode`.

> **Architectural note (pre-existing, not introduced by this move):** because
> `csurf_mcu.cpp` (core) instantiates and owns `m_pActionDisplay`, moving the
> file makes a `core → modes/commands` include dependency visible (core reaches
> across into a mode). That inversion already exists today (core owns an
> Action-Mode display object); the file move only surfaces it. The clean fix —
> transferring ownership from `CSurf_MCU` to `CommandMode` and (optionally)
> gating `OnNameValue` to the active mode — is a **separate refactor**, out of
> scope for the mechanical file reorg. Tracked here so it is not forgotten.

### 3e. `posix_shims/` — the proof that the include strategy works here
There is **already** a subdirectory on the include path:
```cmake
target_include_directories(... PRIVATE ${SRC_DIR}/posix_shims)
```
and 4 files (`Actions.cpp`, `CCSManager.cpp`, `PlugWindowManager.cpp`,
`Region.cpp`) resolve `<windows.h>` through it. This empirically validates the
v2 include strategy on this exact codebase — adding more subdirs to the
include path is a known-working pattern, not theory.

---

## 4. The include strategy — 0 edits, not 437

**Do NOT rewrite any `#include`.** Add all `src/` subdirectories to the target's
include path so the existing flat-name includes keep resolving. This is the
single biggest improvement over v1.

**Decision (maintainer, 2026-07-06): use `GLOB_RECURSE` with `CONFIGURE_DEPENDS`.**
That option makes CMake re-check the directory glob at build time and auto
reconfigure when a directory is added/removed under `src/` — the one real gap
of plain GLOB for include dirs (adding a *file* to an existing dir is already
fine; only a *new directory* needs a reconfigure, which `CONFIGURE_DEPENDS`
handles automatically). Cost: a quick directory-stat per build. The existing
`--reconfigure` rule in AGENTS still applies after hand-edits to `CMakeLists.txt`.

```cmake
# Include-path strategy: every src/ subdirectory on the path (so flat-name
# #include "csurf_mcu.h", "PlugMode.h", etc. keep working unchanged), PLUS the
# vendor/ and resources/ siblings (csurf.h #includes resource.h;
# res_linux.cpp #includes res.rc_mac_dlg). No #include is rewritten.
file(GLOB_RECURSE MCU_SRC_DIRS LIST_DIRECTORIES true CONFIGURE_DEPENDS ${SRC_DIR}/src)

target_include_directories(reaper_csurf_mcu_klinke PRIVATE
    ${CMAKE_BINARY_DIR}     # generated Version.h
    ${MCU_SRC_DIRS}         # src/** (all project source subdirs)
    ${SRC_DIR}/vendor       # checked-in vendored headers (csurf.h, reaper_plugin_functions.h @v5.92)
    ${SRC_DIR}/resources    # dialog resources (resource.h, res.rc_mac_dlg)
    ${JUCE_DIR} ${SDK_DIR} ${WDL_DIR} ${SWELL_DIR} ${BOOST_DIR})
```

> Note: after the move the repo root holds **no** headers, so `${SRC_DIR}` is no
> longer an include directory (it was only needed while vendored headers lived
> in root). `vendor/` and `resources/` replace it explicitly — two lines instead
> of a GLOB, because they are fixed single-level siblings, not a recursive tree.

> **Why GLOB is safe for *include dirs* (not for sources).** The CMake guidance
> "don't GLOB sources" targets `target_sources` — a missing file there silently
> drops a TU from the build. For *include directories* the risk is smaller: once
> a dir is on the path, new headers in it resolve immediately; only a brand-new
> directory needs a reconfigure, and `CONFIGURE_DEPENDS` covers exactly that.
> Scope the glob to `${SRC_DIR}/src` so it cannot pick up editor swap files or
> build artifacts. Alternative (zero-magic): list the ~12 `src/` subdirs
> explicitly. Either works; GLOB+CONFIGURE_DEPENDS is chosen for DRY.

**Because no `#include` changes, the move is pure `git mv` + one CMakeLists
edit.** That makes it bisectable, revertable, and reviewable as a single commit.

---

## 5. Revised target tree (v3)

Legend: `← moved` = relocated from v1's plan; `(shared)` = used by >1 subsystem.

### Top-level layout after reorg

Nothing project-owned lingers in the repo root except build-system files and
conventional root docs (see §6). The four project-bearing top-level dirs are
**siblings** (vendored/resources kept out of `src/`, symmetric with the fetched
deps):

```
src/         project source code (tree below)
vendor/      checked-in vendored headers (← repo root): csurf.h, reaper_plugin_functions.h@v5.92
resources/   dialog resources (← repo root): res.rc, resource.h, res.rc_mac_dlg
scripts/     all shell scripts (← repo root): fetch_deps.sh, build_and_run.sh, debug_reaper.sh, start_reaper.sh
posix_shims/ <windows.h> shim (unchanged, root-level include dir)
[+ fetched deps juce_8/ reaper-sdk/ boost_1_91_0/ ; docker/ docs/ manual/]
```

`vendor/` and `resources/` are added to `target_include_directories` (2 lines,
beyond the `src/` GLOB) because `csurf.h` #includes `resource.h` and
`res_linux.cpp` #includes `res.rc_mac_dlg`.

### `src/` subtree

```
src/
├── csurf_main.cpp                       entry point (REAPER_PLUGIN_ENTRYPOINT)
├── res_linux.cpp                        SWELL dialog resources (Linux/macOS)
├── JuceHeader.h                         JUCE master include (← repo root; 40 includers)
│
├── core/
│   ├── csurf_mcu.{cpp,h}                main control-surface class
│   ├── CCSManager.{cpp,h}               mode dispatcher
│   ├── CCSMode.{cpp,h}                  mode base class
│   ├── ButtonManager.{cpp,h}
│   ├── Selector.{cpp,h}
│   ├── mcu_button_defines.h             MIDI CC ↔ MCU button map (← repo root; included by csurf_mcu.h)
│   ├── McuDebugLog.h                    logging macros (project-wide)
│   ├── KlinkeLookAndFeel.h              JUCE 8 per-window look-and-feel
│   ├── McuAssert.h                      ASSERT macro  (← repo root; merged util/ into core/)
│   └── std_helper.h                     std::map helpers (← repo root; merged util/ into core/)
│
├── state/
│   ├── Tracks.{cpp,h}
│   ├── Transport.{cpp,h}
│   ├── VPOT_LED.{cpp,h}
│   ├── Region.{cpp,h}
│   ├── Options.{cpp,h}
│   ├── ProjectConfig.{cpp,h}
│   └── UndoEnd.{cpp,h}
│
├── display/
│   ├── Display.{cpp,h}                  generic MCU 2x55 display model
│   ├── DisplayHandler.{cpp,h}
│   └── MeterBridge.{cpp,h}              abstract base only
│
├── ui/
│   ├── CCSModesEditor.{cpp,h}           on-screen settings window shell
│   └── TabbedComponentWithCallback.{cpp,h}
│
└── modes/
    ├── commands/
    │   ├── Actions.{cpp,h}
    │   ├── ActionsDisplay.{cpp,h}        ← Action-Mode action labels (behavioral cohesion; see §3d)
    │   ├── CommandMode.{cpp,h}
    │   └── editor/
    │       ├── ActionsDialogComponent.{cpp,h}
    │       ├── CommandModeMainComponent.{cpp,h}
    │       ├── CommandModePageComponent.{cpp,h}
    │       └── CommandModeVPOTComponent.{cpp,h}
    │
    ├── multitrack/
    │   ├── MultiTrackMode.{cpp,h}
    │   ├── MultiTrackMeterBridge.{cpp,h}      ← moved from meter/ (couples Tracks)
    │   ├── MultiTrackOptions.{cpp,h}
    │   ├── MultiTrackOptions2.{cpp,h}
    │   ├── MultiTrackSelector.{cpp,h}
    │   ├── PanMode.{cpp,h}
    │   ├── PerformanceMode.{cpp,h}            (intentional stub — see AGENTS §6)
    │   └── editor/
    │       ├── TrackStatesEditorComponent.{cpp,h}
    │       └── TrackStatesTableComponent.{cpp,h}
    │
    ├── sends/
    │   ├── SendReceiveModeBase.{cpp,h}
    │   ├── SendMode.{cpp,h}
    │   ├── ReceiveMode.{cpp,h}
    │   └── SendReceiveMeterBridge.{cpp,h}     ← moved from meter/ (couples base)
    │
    └── plugin/
        ├── PlugMode.{cpp,h}
        ├── PlugAccess.{cpp,h}
        ├── PlugMap.{cpp,h}
        ├── PlugMapManager.{cpp,h}
        ├── PlugMapSaveDialog.{cpp,h}
        ├── PlugPresetManager.{cpp,h}
        ├── PlugWindowManager.{cpp,h}
        ├── PluginWatcher.{cpp,h}
        ├── PlugMoveWatcher.{cpp,h}
        ├── PlugMode2ndOptions.{cpp,h}
        ├── PlugModeOptions.{cpp,h}
        ├── PlugModeSelectors.{cpp,h}
        ├── PlugModeMeterBridge.{cpp,h}        ← moved from meter/ (couples PlugMode)
        └── editor/
            ├── PlugModeComponent.{cpp,h}
            ├── PlugModeBankComponent.{cpp,h}
            ├── PlugModeBankReferenceComponent.{cpp,h}
            ├── PlugModeChannelComponent.{cpp,h}
            ├── PlugModeFaderComponent.{cpp,h}
            ├── PlugModeMapInfoComponent.{cpp,h}
            ├── PlugModePageComponent.{cpp,h}
            ├── PlugModePageReferenceComponent.{cpp,h}
            ├── PlugModeParamComponent.{cpp,h}
            ├── PlugModeSingleBankComponent.{cpp,h}
            ├── PlugModeSingleChannelComponent.{cpp,h}
            ├── PlugModeSinglePageComponent.{cpp,h}
            ├── PlugModeVPOTComponent.{cpp,h}
            └── PlugModeVPOTTableComponent.{cpp,h}
```

**Depth:** max 3 (`src/modes/plugin/editor/`) — same ceiling as v1.

---

## 6. Files that stay in the repo root (NOT moved)

After this plan the repo root holds only build-system files and conventional
root docs — **no project source, headers, resources, or scripts**:

| File(s) | Why root |
|---|---|
| `CMakeLists.txt`, `Version.h.in`, `VERSION.txt` | Build-system root files (CMake expects them here). |
| `AGENTS.md`, `gplv3.txt`, `readme.txt` | Conventional repo-root files. |
| `Linux-Port-Changes.md`, `Mac-Port-Changes.md`, `notes.org`, `whats_new.{org,txt}` | Project notes/logs. (Optional: move to `docs/` — see §7e.) |

Everything else moves: source → `src/`, vendored headers → `vendor/`, resources
→ `resources/`, scripts → `scripts/`. `posix_shims/`, `docker/`, `docs/`,
`manual/`, and the fetched deps are already organized and untouched.

> `reaper_plugin_functions.h` moves to `vendor/` **pinned at v5.92** (the root
> copy's version). Upgrading to the `reaper-sdk/sdk/` v7.74 copy is a separate
> deferred step — see §7f.

---

## 7. Tier 0 — zero-risk cleanup (do independently, before or after the move)

These improve the repo *regardless* of the `src/` move and carry near-zero
risk. Recommended as a separate prep commit.

### 7a. Retire pre-revival dead build artifacts (VS project files + JUCE-changes/)
CMake is now source of truth on **all three** platforms. Two clusters of frozen
pre-revival build artifacts can go:

**(i) Dead Visual Studio project files** — frozen 2024-07-14, point at
non-existent JUCE-1.52/Boost-1.39 roots:
```
reaper_csurf.dsp            reaper_csurf.dsw
reaper_csurf.sln            reaper_csurf.vcproj
reaper_csurf.vcxproj        reaper_csurf.vcxproj.filters
reaper_csurf.vcxproj.user   res.aps
```

**(ii) `JUCE-changes/`** — 3 files (80 KB), frozen **2016-02-15**: a VS2010
solution + `juce_Config.h` for the old *JUCE 1.52 + manually-compile-the-libs*
workflow. Nothing in the active build references it (no CMake/script/source;
`juce_Config.h` is unused — JUCE 8 is configured via CMake cache flags). Pure
dead weight.

**Treatment (decided 2026-07-06):** move to `archive/` — VS files to
`archive/vs-legacy/` (keep `.vcxproj.filters` virtual-folder mapping as the
historical categorization reference), `JUCE-changes/` to
`archive/juce-1.52-patches/`. (Git history preserves both either way; archiving
keeps them greppable for one release cycle. `git rm` outright is equally valid
for `JUCE-changes/` — unlike the `.filters`, it has no reference value.)

> **Related stale-doc follow-up:** `readme.txt:48-56` still instructs users to
> "use JUCE 1.52" and replace files from `JUCE-changes/` — entirely pre-revival.
> When `JUCE-changes/` is archived/removed, that readme section must be rewritten
> or marked superseded by `AGENTS.md` (don't leave a dangling reference).

### 7b. Remove stale debug artifact
`docs/.juce-upgrade-remaining-errors.log` (928 lines, dated 2026-06-25) is a
leftover from the JUCE-upgrade debugging phase; the build is clean now.
`git rm` it. (It's a dotfile — easy to miss.)

### 7c. Fix the stale CMakeLists header comment
The comment block at the top of `CMakeLists.txt` still says:
```
• Windows — NOT yet wired into CMake (the .vcxproj remains the source of
  truth until the WIN32 branch below is implemented).
```
but the WIN32 branch below is **fully implemented and working**. Update to
reflect reality (Linux/macOS/Windows all wired).

### 7d. Move ALL scripts → `scripts/` (including `fetch_deps.sh`)
Every shell script moves into `scripts/`; nothing script-related stays in root.
Two resolve their own location as the repo root and need a one-line fix on move;
the other two are host-only and path-independent:

| Script | Adjust on move? |
|---|---|
| `fetch_deps.sh` | **Yes** — `ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"` → append `/..` so it still writes `juce_8/`, `reaper-sdk/`, `boost_1_91_0/` at the **repo root**, not into `scripts/`. |
| `build_and_run.sh` | **Yes** — `SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"` is used as the CMake source dir and `build/` parent → append `/..` to point at repo root. |
| `debug_reaper.sh` | No — only references `$HOME/opt/REAPER`; location-independent. |
| `start_reaper.sh` | No — dito. |

Follow-up (mechanical, one-time): update the ~10 `./fetch_deps.sh` references in
`AGENTS.md` (§2, §4) and the CMake guard message (`"Run ./fetch_deps.sh first"`)
to `./scripts/fetch_deps.sh`. Moved helper scripts are CRLF in places (see
AGENTS CRLF note); convert to LF on move.

### 7e. (Optional) Consolidate project notes/logs → `docs/`
The root still holds `Linux-Port-Changes.md`, `Mac-Port-Changes.md`,
`notes.org`, `whats_new.{org,txt}`. They are project docs and could join `docs/`
for a fully clean root (leaving only `readme.txt`, `gplv3.txt`, `AGENTS.md`).
Zero-risk `git mv`; not required for the reorg. Skip if you prefer them visible
at the root.

### 7f. Deferred — separate future steps (NOT part of this reorg)
Tracked so they are not lost (also in MEMD `Active Context`):
- **Upgrade `reaper_plugin_functions.h` v5.92 → v7.74** — after the move, delete
  the `vendor/` copy; the build then resolves via `${SDK_DIR}`. Deliberate step:
  a REAPER API version change with potential behavior impact, not a file-reorg
  side-effect.
- **Remove `EXT_B` (extender variant)** — the compile-time switch + B-unit build
  path (AGENTS §5).
- **Finish GPL migration** — headers + manual text GPLv2 → GPLv3 (AGENTS §6).

---

## 8. Execution plan for the structural move (Tier 1)

One atomic, well-secured commit. Revertable by `git revert` since it touches no
`#include`.

1. **Clean tree:** `git checkout VERSION.txt` (or commit the counter bump first).
2. **`git mv`** every `.cpp`/`.h` per §5. (~139 file moves; scriptable — see §9.)
3. **Edit `CMakeLists.txt`:**
   - change `target_sources(... Actions.cpp ...)` → glob or explicit `src/...`
     paths; update `res.rc` → `resources/res.rc` in the WIN32 branch;
   - add `src/` subdirs to `target_include_directories` (§4), **plus `vendor/`
     and `resources/`** (csurf.h #includes resource.h; res_linux.cpp #includes res.rc_mac_dlg);
   - move the `res_linux.cpp` / `swell-modstub*.cpp` references to `src/res_linux.cpp` etc.
4. **No `#include` edits** — verify with `grep -rn '#include' src/ vendor/ resources/ | wc -l`
   before/after (must be unchanged: 437 + 62 = 499).
5. **Verify build on Linux** (baseline): `cmake -B build && cmake --build build`.
6. **Verify build on Windows** (from WSL): `scripts/build-windows.sh --reconfigure`.
7. **(If macOS available)** verify via the macOS branch.
8. **Commit** as a single commit: `"refactor: move sources into src/ subtree
   (no include changes; CMake include-path strategy)"`.

### Failure modes to watch
- **`reaper_plugin.h` / SWELL includes** resolve via `${WDL_DIR}` / `${SWELL_DIR}`,
  not via repo root — unaffected by the move. (Verified: 8 `#include "reaper_plugin.h"`
  come from `reaper-sdk`, not root.)
- **Generated `Version.h`** lives in `${CMAKE_BINARY_DIR}` (on the path) — unaffected.
- **`posix_shims/windows.h`** — stays at repo root (it shadows `<windows.h>`
  via its own include dir); do **not** move it into `src/`.

---

## 9. Scripted move helper (sketch)

Because the move is mechanical, a throwaway script avoids typos. Sketch:

```bash
#!/usr/bin/env bash
# scripts/restructure-move.sh — one-shot file mover per plan v3. Run once, then delete.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

mkdir -p src/{core,state,display,ui}
mkdir -p src/modes/{commands,sends,plugin}/editor src/modes/multitrack/editor
mkdir -p vendor resources scripts

m() { git mv "$1" "$2"; }   # m <src> <destdir/>

# core
for f in csurf_mcu CCSManager CCSMode ButtonManager Selector; do m $f.cpp src/core/; m $f.h src/core/; done
m csurf_main.cpp src/
m res_linux.cpp src/
m JuceHeader.h src/                       # JUCE master include (← repo root; 40 includers)
m McuDebugLog.h src/core/; m KlinkeLookAndFeel.h src/core/
m mcu_button_defines.h src/core/          # ← repo root (included by csurf_mcu.h)
# util headers merged into core/ (maintainer decision: no separate util/)
m McuAssert.h src/core/; m std_helper.h src/core/
# state
for f in Tracks Transport VPOT_LED Region Options ProjectConfig UndoEnd; do m $f.cpp src/state/; m $f.h src/state/; done
# display (generic infra only; ActionsDisplay moves to commands/ — see §3d)
for f in Display DisplayHandler MeterBridge; do m $f.cpp src/display/; m $f.h src/display/; done
# ui
for f in CCSModesEditor TabbedComponentWithCallback; do m $f.cpp src/ui/; m $f.h src/ui/; done
# commands (ActionsDisplay included: Action-Mode label content lives here)
m Actions.cpp src/modes/commands/; m Actions.h src/modes/commands/
m ActionsDisplay.cpp src/modes/commands/; m ActionsDisplay.h src/modes/commands/
m CommandMode.cpp src/modes/commands/; m CommandMode.h src/modes/commands/
for f in ActionsDialogComponent CommandModeMainComponent CommandModePageComponent CommandModeVPOTComponent; do m $f.cpp src/modes/commands/editor/; m $f.h src/modes/commands/editor/; done
# multitrack
for f in MultiTrackMode MultiTrackMeterBridge MultiTrackOptions MultiTrackOptions2 MultiTrackSelector PanMode PerformanceMode; do m $f.cpp src/modes/multitrack/; m $f.h src/modes/multitrack/; done
for f in TrackStatesEditorComponent TrackStatesTableComponent; do m $f.cpp src/modes/multitrack/editor/; m $f.h src/modes/multitrack/editor/; done
# sends
for f in SendReceiveModeBase SendMode ReceiveMode SendReceiveMeterBridge; do m $f.cpp src/modes/sends/; m $f.h src/modes/sends/; done
# plugin (non-editor)
for f in PlugMode PlugAccess PlugMap PlugMapManager PlugMapSaveDialog PlugPresetManager PlugWindowManager PluginWatcher PlugMoveWatcher PlugMode2ndOptions PlugModeOptions PlugModeSelectors PlugModeMeterBridge; do m $f.cpp src/modes/plugin/; m $f.h src/modes/plugin/; done
# plugin editor
for f in PlugModeComponent PlugModeBankComponent PlugModeBankReferenceComponent PlugModeChannelComponent PlugModeFaderComponent PlugModeMapInfoComponent PlugModePageComponent PlugModePageReferenceComponent PlugModeParamComponent PlugModeSingleBankComponent PlugModeSingleChannelComponent PlugModeSinglePageComponent PlugModeVPOTComponent PlugModeVPOTTableComponent; do m $f.cpp src/modes/plugin/editor/; m $f.h src/modes/plugin/editor/; done

# vendored checked-in headers → vendor/ (both end up on the include path)
m csurf.h vendor/                         # only copy (NOT in reaper-sdk/); 19 includers
m reaper_plugin_functions.h vendor/       # PINNED at v5.92 (v7.74 upgrade deferred — §7f)
# resources → resources/ (res.rc includes resource.h; res_linux.cpp includes res.rc_mac_dlg)
m res.rc resources/; m resource.h resources/; m res.rc_mac_dlg resources/
# scripts → scripts/ (fetch_deps.sh + build_and_run.sh need a /.. repo-root fix AFTER the move — §7d)
m fetch_deps.sh scripts/; m build_and_run.sh scripts/; m debug_reaper.sh scripts/; m start_reaper.sh scripts/

echo "Moves done. Next:"
echo "  1. fix fetch_deps.sh + build_and_run.sh: append /.. to the ROOT/SCRIPT_DIR resolution (§7d)"
echo "  2. edit CMakeLists.txt: target_sources paths, add vendor/ + resources/ to include dirs, res.rc → resources/res.rc"
echo "  3. build (Linux + Windows), then commit"
```

(Recount after running: `find src vendor resources \( -name '*.cpp' -o -name '*.h' \) | wc -l`
must equal 139; `find . -maxdepth 1 \( -name '*.cpp' -o -name '*.h' \) | wc -l` must equal
**0** — after this plan NO project headers or sources remain in the repo root.)

---

## 10. Open decisions for the maintainer

1. **Tier 0 first, then Tier 1?** (Recommended — isolates risk.) Or one big commit? — *the only remaining open decision.*

**Resolved (2026-07-06):**
- **Dead VS files + `JUCE-changes/`** → archive (`archive/vs-legacy/`, `archive/juce-1.52-patches/`); keep `.vcxproj.filters` virtual-folder mapping as a historical reference. `JUCE-changes/` confirmed obsolete (JUCE 1.52 / VS2010, last touched 2016, unused by the JUCE-8 build).
- **Include magic** → `GLOB_RECURSE` + `CONFIGURE_DEPENDS` (see §4).
- **`util/` for `McuAssert.h` + `std_helper.h`** → merged into `core/` (a 2-file folder wasn't worth it).
- **`ActionsDisplay` placement** → `modes/commands/` (behavioral cohesion overrides structural coupling; see §3d).

---

## 11. What NOT to do (lessons from v1/v2)

- ❌ Rewrite 437 `#include` directives to add path prefixes. (v1's plan — pure
  cost, zero benefit, breaks on every future `git mv`.) Use the include-path
  strategy in §4.
- ❌ Put MeterBridge subclasses in a shared `meter/` — each couples to its mode (§3a).
  (The abstract `MeterBridge` base stays in `display/`.)
- ❌ Give PlugMode an `editor/` but not CommandMode — use the `*Component` rule (§3b).
- ❌ Touch `posix_shims/` — it's a working root-level include shim, not project source.

> Note: `ActionsDisplay` **does** move to `modes/commands/` (§3d) — the one
> case where behavioral cohesion ("these are the Action-Mode action labels",
> per maintainer) overrides the structural-coupling argument. Do not re-litigate
> it on include-graph grounds.
