# Channel Strip Mode — Implementation Plan

> Status: **REFACTORING** (architecture corrected — see §0 for what changed)
> Origin: `notes.org` → "ChannelStrip Idee (Sketch)"

## 0. What changed in this revision (why we are refactoring)

The first implementation (DeepSeek) made a fundamental architecture error:
it built the mode bank-based on `MultiTrackMode`'s channel logic — each of the
8 channels addressed a **different bank track** via `getMediaTrackForChannel(c)`,
and the strip was looked up per `(trackGUID, unitIndex)` against that bank
track. That made every VPOT control a parameter on a *different* track
simultaneously (e.g. VPOT 1 reached the track holding ReaEQ, VPOT 2 the track
holding ReaComp), instead of all 16 VPOTs of a unit driving the 16 parameters
of one strip on the **selected track**.

This revision corrects the model. The `(trackGUID, unitIndex) → stripIndex`
assignment table is correct and stays; what changes is **which track the
VPOTs act on** (the selected track, not the bank track) and how the strip's
live FX slot is resolved (cached per track+strip, not stored in the global
map). Selection semantics are also clarified: a unit without a strip always
shows the strip-name list and is always pickable; a unit with a strip shows
its parameters and is only re-pickable while B_VPOT_TRACK is held.

## 1. Feature (from `notes.org`, clarified)

- A **Channel Strip Map** = exactly 16 parameter bindings (VPOTs 1–8 normal,
  9–16 via Shift) of **one plugin**. Operated via VPOTs. Base is a Track Mode.
- **16 GLOBAL strips**, shared across all projects/tracks. Stock-plugin maps
  ship pre-made. Each strip is ONE plugin + its 16 VPOT→param mapping.
- **Selected track is the reference.** If 0 or ≥2 tracks are selected there is
  no active channel strip (VPOTs dark, row 1 shows the "select one track"
  hint — same text PlugMode uses).
- **Per (trackGUID, unitIndex): an assignment to one of the 16 strips**
  (–1 = none). Persisted in the project (Step E). Each unit on a track may be
  assigned a different strip.
- **VPOTs act on the selected track.** A unit's 8 VPOTs (16 with Shift) drive
  the 16 parameters of the strip assigned to that unit for the selected track.
- **Selection (State 0):** a unit with no assigned strip always shows the 8
  global strip names in row 1 (VPOT 1 → strip 0 … VPOT 8 → strip 7; with
  Shift strips 8–15). VPOT press picks that strip for this unit on the
  selected track and auto-adds the plugin if missing. This is always available
  — no need to hold B_VPOT_TRACK.
- **Re-pick (held TRACK):** a unit that already has a strip shows its
  parameters normally. While B_VPOT_TRACK is **held down**, every unit
  (including those with a strip) shows the strip-name list and a VPOT press
  re-assigns that unit's strip. On release, VPOTs return to parameter control.
- **Insert position** (per strip): first, n-th (2–8), or last — controls where
  `+` inserts the plugin into the chain.
- **Duplicates allowed:** the same plugin may occupy several strips to edit
  different aspects (e.g. EQ once, compressor once). Selecting it again does
  **not** add a second instance — the existing instance is reused.
- **VPOT press (active state):** no separate press mapping. A press sets the
  parameter to 1 if the current value != 1, otherwise to 0 (toggle).
- **Track remembers per-unit strip assignments** and saves them in the project.
- **On-screen editor:** opened by **ALT+TRACK**. A 16-row table —
  `Number | Plugin | short display abbreviation | Insert Pos | Open Map`.
  Global (independent of track/unit).
- **ALT+VPOT-1:** opens the floating FX window of the plugin controlled by
  that slot's binding (PlugMode's open-window-count rules duplicated).
- **ALT+VPOT-7 / ALT+VPOT-8:** move the FX up / down in the chain.
- **ALT+Name/Values:** shows these command labels in the display.
- Modifier choice (ALT vs CONTROL) to be finalised during implementation;
  PlugMode already uses ALT+SELECT for plugin windows, so ALT is the default.

## 2. How it maps onto the existing architecture

Like `PanMode` and `CommandMode`, `ChannelStripMode` inherits from
`MultiTrackMode` and overrides only the VPOT/Display behaviour. Faders,
Sel/Mute/Solo/Rec, bank navigation, LEDs, and row-0 track names come from
`MultiTrackMode` unchanged. The difference from Pan/Action: the VPOTs do
**not** operate on the per-channel bank track (`getMediaTrackForChannel`) but
on the **selected track** (`getSelectedSingleTrack`).

| Concept (notes.org) | Existing pattern to reuse |
|---|---|
| New CCSMode, "Track Mode" base | `MultiTrackMode` subclass (like Pan/Command) |
| Activation via TRACK button | `B_VPOT_TRACK` (`0x28`) — `case` in `CCSManager::buttonVPOTassign` (stays active); press/release sets selection mode |
| "+ adds plugin to track" | `TrackFX_AddByName` (insert pos via `instantiate`); ident list from `EnumInstalledFX` |
| Read/write parameters | `TrackFX_GetParam` / `TrackFX_SetParam`; names from `TrackFX_GetParamName` |
| Track plugin move/delete tracking | `PlugMoveWatcher` singleton — used to invalidate the slot cache |
| Per-track persistence in project | `ProjectConfig::connect2ProjectChangeSignal` + XML |
| Map save/load to file | `PlugMapManager`-style installed/user map locations under `GetResourcePath()` |
| VPOT LEDs, assignment display | `updateVPOTLeds`, `setAssignmentDisplay` in `CCSManager` |
| On-screen JUCE editor | `createEditorComponent`, mirroring `PlugModeComponent` |
| "select one track" hint | same text PlugMode uses (`"You must select a single track."`) |

### REAPER API surface used

- `EnumInstalledFX(index, &name, &ident)` — editor combo box; `ident` carries
  the type prefix (`VST3:`, `VST:`, `AU:`, `JS:`, `DX:`, `CLAP:`).
- `TrackFX_AddByName(track, ident, recFX, instantiate)` — the "+" flow;
  `instantiate <= -1000` encodes the insert position.
- `TrackFX_GetCount`, `TrackFX_GetFXName`, `TrackFX_GetFXGUID` — slot resolution.
- `TrackFX_GetNamedConfigParm(slot, "fx_ident")` — exact VST2/VST3 matching
  (optional API; loaded via `rec->GetFunc`, not `IMPAPI`).
- `TrackFX_GetNumParams`, `TrackFX_GetParamName`, `TrackFX_GetParam`,
  `TrackFX_SetParam` — parameter enumeration, display, control.
- `TrackFX_FormatParamValue` — formatted value display (e.g. "1.0k"). **Optional
  API** (via `rec->GetFunc`, not `IMPAPI`) — cosmetic only; must not block load.
- `TrackFX_CopyToTrack(src, srcFx, dst, dstFx, is_move)` — ALT+VPOT-7/8 reorder.
- `TrackFX_Delete` — available for future "remove binding".

## 3. Data model

```cpp
// One global Channel Strip (one of 16). A single plugin plus the mapping
// of its parameters onto the 8 VPOTs (16 with Shift).
class ChannelStripMap {
    String fxIdent;                 // EnumInstalledFX ident, to find/add the plugin
    String shortName;               // <=5 chars for the display
    enum InsertPos { FIRST, P2..P8, LAST } insertPos;
    int vpotParam[16];              // param index per VPOT pos; -1 = unbound
    String vpotName[16];            // per-VPOT display name (max 6 chars)
    // NO fxGUID here — instance-specific, must not live in a GLOBAL map.
    void writeToXml / readFromXml;  // persists fxident, shortName, inspos, vpotParam/Name
};
```

**fxIdent (stable, track-independent)** drives the "+" logic via
`TrackFX_AddByName`. The live FX slot is resolved at runtime against the
selected track's FX chain and **cached per (trackGUID, stripIndex)**, then
invalidated by `PlugMoveWatcher` (reorder) and FX-delete tracking. The cache
does NOT touch the global `ChannelStripMap`.

### Runtime slot cache

```cpp
// per (trackGUID, stripIndex) -> 0-based FX slot on that track, or -1 (dangling)
std::map<std::pair<String,int>, int> m_slotCache;
```

- `resolveSlot(tr, stripIndex)` returns the cached slot or re-resolves via
  `findSlotByIdent` (exact `fx_ident` first, normalised-name fallback) and
  stores it.
- `PlugMoveWatcher` signal → invalidate all entries for that track.
- FX delete → invalidate that track's entries (re-resolved lazily on next use).

### Persistence (Step E) — implemented

All strip data lives in **one** file:
`~/.config/REAPER/MCU/ChannelStripMaps/channelstrips.xml` (user location,
same style as `PlugMapManager` user maps). One `<STRIP>` per ASSIGNED slot,
holding the header fields AND the VPOT→param mapping together:

```xml
<CHANNELSTRIPS>
  <STRIP nr="1" fxident="VST3:ReaEQ (Cockos)" name="EQ" inspos="first">
    <VPOT nr="1" param="3" name="Band1"/>
    <VPOT nr="2" param="4"/>
    ...
  </STRIP>
  ...
</CHANNELSTRIPS>
```

- Loaded once at startup (`loadStripsFromFile` in the ctor).
- Saved when either editor closes: the main editor (`saveStripsToFile` in
  `removeEditor`) and the mapping editor (`saveStripsToFile` in the
  `ChannelStripParamEditor` destructor).

Per-track per-unit assignment is stored **in the Reaper project** via
`ProjectConfig` (`projectChanged` WRITE/READ/FREE):

```xml
<CHANNELSTRIP_ASSIGNMENTS>
  <ASSIGN track="{guid}" unit="0" strip="3"/>
  ...
</CHANNELSTRIP_ASSIGNMENTS>
```

`ChannelStripMap` exposes `writeToXml(parent, nr)` (header + VPOT children)
and `readFromXml(pStrip)` (header attrs + VPOT children).

## 4. Runtime states (per unit, derived — not switched)

The display/VPOT behaviour of a unit is fully derived from three facts:
(selected track present?) × (unit has strip AND its plugin is present on the
selected track?) × (selection mode = TRACK held?). A unit whose assigned
strip's plugin is missing behaves like a unit without a strip (picker).

| selected track? | unit has strip? | TRACK held? | Row 1 shows | VPOT turn | VPOT press |
|---|---|---|---|---|---|
| no  | –   | –   | "You must select a single track." | –    | –           |
| yes | no  | any | strip names (1–8 / Shift 9–16); a leading `+` (no space) marks strips whose plugin is missing on the selected track | –    | pick strip for this unit (+auto-add) |
| yes | yes (plugin present) | no  | param name (idle) / value (1 s after turn) | nudge | toggle 0/1 |
| yes | yes (plugin present) | yes | strip names                       | –    | re-pick strip for this unit |
| yes | yes, but plugin MISSING on the selected track | any | like "no strip": picker, `+` marks | –    | (re)pick strip for this unit (+auto-add) |

Selection mode is a single bool `m_selectionMode` set by CCSManager on
B_VPOT_TRACK press/release. It only affects units that already have a strip;
units without a strip are always in pick mode.

## 5. Module structure

```
src/modes/channelstrip/
├── ChannelStripMode.{cpp,h}          # MultiTrackMode subclass; VPOTs act on selected track
├── ChannelStripMap.{cpp,h}           # 1 strip: plugin + metadata + 16 VPOT→param bindings (NO fxGUID)
├── ChannelStripAccess.{cpp,h}        # TrackFX_* wrapper; fxIdent→slot resolution + slot cache
└── editor/
    ├── ChannelStripComponent.{cpp,h}        # main editor (16 global strips)
    ├── ChannelStripBindingTable.{cpp,h}     # 16 rows: # | Plugin | abbrev | InsPos | edit…
    └── ChannelStripParamEditor.{cpp,h}      # 2nd editor: VPOT→param mapping for one strip
```

### Wiring (existing files)

- **`CCSManager.{h,cpp}`** — `ChannelStripMode* m_pChannelStripMode`;
  ctor/dtor; `case B_VPOT_TRACK` (stays active); on press set
  `m_selectionMode=true` and activate, on release set `m_selectionMode=false`;
  ALT branch opens the editor; LED logic (BLINK active / ON if selected track
  has a configurable strip / OFF otherwise); `getChannelStripMode()`.
- **`CMakeLists.txt`** — new sources (already added).
- **`csurf_main.cpp`** — `TrackFX_FormatParamValue` and
  `TrackFX_GetNamedConfigParm` both optional via `rec->GetFunc` (not `IMPAPI`).

## 6. Refactor steps (ordered)

### R — Core architecture fix (current milestone)

1. **`ChannelStripMap`**: remove `m_fxGUID`/`isResolved`/`getFxGUID`/`setFxGUID`
   and the `fxguid` XML attribute. Strip is now pure data (fxIdent + metadata
   + 16 VPOT bindings).
2. **`ChannelStripAccess`**: `resolveBinding` no longer mutates the map; it
   returns a slot. Add the per-(trackGUID, stripIndex) slot cache with
   `PlugMoveWatcher` invalidation. `addPlugin` returns the slot without
   writing a GUID into the map.
3. **`ChannelStripMode` core**:
   - `getStripForChannel(c)`: use `getSelectedSingleTrack()` (not
     `getMediaTrackForChannel`); unit = `(c-1)/8`; strip =
     `stripIndexForUnit[unit]` for the selected track.
   - `vpotMoved`/`vpotPressed`: branch on (has strip?) × `m_selectionMode`
     per §4 table. Pick/re-pick assigns `stripIndexForUnit[unit]` and
     auto-adds the plugin.
   - `updateChannel`/`updateVPOTs`: render per §4 table; VPOTs OFF when no
     selected track.
   - `m_lastVPOTChangeTime` → array per channel.
   - Remove `returnToStripSelection()` (re-pick is momentary now).
   - No-selected-track guard + `"You must select a single track."` on row 1.
4. **`CCSManager`**: B_VPOT_TRACK press → `m_selectionMode=true` + activate;
   release → `m_selectionMode=false`. Remove the re-press `returnToStripSelection`
   block. LED logic refined.
5. **`csurf_main.cpp`**: `TrackFX_FormatParamValue` optional.
6. **Build + deploy + user test.**

### D — First-time / "+" flow (folded into R)

- State-1 ("+") display: strip name + " +" when the strip is assigned but the
  plugin is not on the selected track. VPOT press auto-adds at insert pos.
- Auto-add on pick: when a unit picks a strip (State 0 or re-pick), add the
  plugin if missing.

### E — Persistence

- Global map save/load (16 strip files in the user location).
- Per-track per-unit assignments via `ProjectConfig` (READ/WRITE/FREE).
- Roundtrip survives a REAPER restart.

### F — Move/delete tracking — DONE (see §8 for details)

- `PlugMoveWatcher`: reorder → invalidate the slot cache for that track.
- FX delete / track remove → mark strip dangling, update display.
- Deliberately NOT implemented (decided with the maintainer): pruning
  `m_assignments`/`m_slotCache` entries of deleted tracks during a session.
  The stale entries are ~170 bytes per deleted track, bounded per project
  session (cleared on project READ/FREE) — not worth the extra hook.

### G — ALT shortcuts — DONE for ChannelStripMode (2026-08-28)

- ALT+VPOT-7 / ALT+VPOT-8: move the unit's strip FX up/down
  (`TrackFX_CopyToTrack`, `is_move=true`); no-op at chain edges; invalidates
  the track's slot cache afterwards.
- ALT+VPOT-1: open the floating FX window; ALT+VPOT-2: open the FX chain.
  The PlugMode window settings are deliberately IGNORED (maintainer decision,
  2026-08-28) — the commands always do exactly this, no option plumbing.
- ALT held (any button, NOT Name/Values specifically): the ALT-command
  legend appears in the per-VPOT row-1 fields (maintainer decision — plain
  ALT, no Name/Values button). Revised after the first user test: NOT a
  full-line legend — the labels sit directly in the VPOT fields
  (`changeField`, 6 chars) and ONLY on units whose strip is ACTIVE (assigned
  + plugin present, checked per unit — only then is the target FX known):
  VPOT-1 "Float", VPOT-2 "Chain", VPOT-7 "FXup", VPOT-8 "FXdown"; all other
  fields (and all fields of inactive units) stay empty. The commands
  themselves are likewise only active on units with an active strip.
- Deferred (maintainer decision): ALT+VPOT-7/8 in PlugMode (per `notes.org`) —
  not implemented in this pass.
- Implementation: `ChannelStripMode::vpotPressed` intercepts ALT at vpot
  0/1/6/7 (= hardware VPOT 1/2/7/8 — **vpot is 0-based**; first test failed
  due to an off-by-one, see §8), unshifted range only, other VPOTs fall
  through to normal behaviour; `openFxWindow(tr, slot, floating)` / `moveFx()`
  helpers; per-unit legend in `updateChannel()`; `m_lastAltState` refresh in
  `frameUpdate()`. `TrackFX_CopyToTrack` added to the mandatory `IMPAPI` list
  (csurf_main.cpp, since REAPER 4.0 — below the 6.37 floor) and to
  `vendor/csurf.h`.

## 7. Open items / notes

- **Insert-position semantics** — `instantiate <= -1000` encodes position
  (`-1000` = first/pos 0, `-1001` = pos 1, …); `LAST` = `-(1000 + chainLen)`.
  Code is correct; comments will be cleaned up.
- **`fx_ident`-Parm format** — verify at runtime (log) that
  `TrackFX_GetNamedConfigParm("fx_ident")` strings match `EnumInstalledFX`
  idents; keep the normalised-name fallback for older REAPER.
- **Duplicate-instance reuse** — when a strip's `fxIdent` matches an FX already
  on the track, reuse that instance; do not add another.
- Release hardening (three-platform build, ASan/leak pass, manual + AGENTS.md
  mode-table docs, `notes.org` → DONE) is tracked separately under the general
  pre-release checklist, not as feature-plan steps.

---

## 8. Current status (work log — read this first when resuming)

**Date of this snapshot:** see git log of the latest commit on the
`channel-strip` branch. **Branch:** `channel-strip`.

### What is done (builds + deploys, Linux)

- **Architecture fix (R):** `ChannelStripMode` now acts on the SELECTED track
  (`getSelectedSingleTrack`), not the per-channel bank track. Per-unit state is
  DERIVED from `(selected track?) × (unit has strip?) × (selection mode =
  B_VPOT_TRACK held)` — see §4. `MultiTrackMode` base kept; only VPOTs/row 1
  differ.
- **fxGUID removed from `ChannelStripMap`** (global map must be instance-
  agnostic). Live FX slot resolved at runtime via `ChannelStripAccess` slot
  cache keyed by `(trackGUID, stripIndex)` + GUID verify, invalidated by
  `PlugMoveWatcher`.
- **CCSManager wiring:** B_VPOT_TRACK press → `setSelectionMode(true)`+
  activate; release → `setSelectionMode(false)`. LED: BLINK active / ON if
  selected track has ≥1 assigned strip / OFF otherwise.
- **Optional APIs:** `TrackFX_FormatParamValue` and
  `TrackFX_GetNamedConfigParm` loaded via `rec->GetFunc` (not `IMPAPI`).
- **Editor fixes already applied:**
  - Column "#" → "VPOT" (wider, "Shift 1" readable) in both tables.
  - Column "Parameter" → "Edit Mapping"; the cell button is styled clearly as a
    button (light-grey fill + dark border, label "Edit N/16").
  - Both tables draw NO row-separator lines.
  - `ChannelStripMeterBridge` (`alsoOnDisplay() == false`) replaces
    MultiTrackMode's meter bridge so the LCD meter bars do NOT overwrite row 1
    (strip names) — this was why strips names were missing on unit 2+.
  - Unbound VPOT shows an empty field on the hardware display (not the plugin
    name).
- **Persistence (Step E) — implemented (single-file model):**
  - One file `~/.config/REAPER/MCU/ChannelStripMaps/channelstrips.xml`, one
    `<STRIP nr=.. fxident=.. name=.. inspos=..>` per assigned slot, containing
    header AND `<VPOT>` mapping together.
  - Loaded once in the `ChannelStripMode` ctor (`loadStripsFromFile`).
  - Saved when EITHER editor closes: main editor (`saveStripsToFile` in
    `removeEditor`) and mapping editor (`saveStripsToFile` in the
    `ChannelStripParamEditor` destructor).
  - Per-track per-unit assignments stored in the Reaper project via
    `ProjectConfig` (`projectChanged` WRITE/READ/FREE →
    `<CHANNELSTRIP_ASSIGNMENTS><ASSIGN track= unit= strip=/>`). Slot cache
    invalidated on READ/FREE.
- **`ChannelStripMap` XML:** combined `writeToXml(parent, nr)` /
  `readFromXml(pStrip)` (header + VPOT children in one element).
- **Diagnostic logging** is currently built IN (`MCU_DEBUG_LOG=ON`) and present
  in `findSlotByIdent`, `resolveSlot`, `addPlugin`, `vpotPressed` (prefix
  `CSA`/`CSM`). Log file:
  `~/.config/REAPER/mcu_klinke_debug.log` (truncated on each REAPER start via
  `MCU_LOG_INIT`). **Remember to turn `MCU_DEBUG_LOG` back OFF for a release
  build.**

### Open bugs (NOT yet fixed — resume here)

- **Bug A/B — ROOT CAUSE FOUND (2026-08-28, from `mcu_klinke_debug.log`),
  fix implemented, PENDING USER RE-TEST.** `findSlotByIdent` could never match
  VST/VST3/CLAP instances: the ident stored in the strip (from
  `EnumInstalledFX`) is a **file path** (e.g.
  `/home/.../reaeq.vst.so`), but pass 1 compares against
  `TrackFX_GetNamedConfigParm("fx_ident")` which returns the **config ident**
  (`VST: ReaEQ (Cockos)`), and pass 2 compares against the FX display name
  (`ReaEQ (Cockos)`). Both comparisons fail for path idents. Right after
  `addPlugin` everything worked only because the instance GUID was cached;
  after an FX reorder (`PlugMoveWatcher` → `invalidateTrack`) or project
  reload the re-resolve failed → `+` shown and the plugin re-added (Bug A =
  re-add on VPOT press; Bug B = duplicate insert). Fix: new
  `ChannelStripAccess::installedNameForIdent()` maps a path ident to its
  `EnumInstalledFX` display name (cached list) and pass 2 now also accepts
  that name. JS idents already matched via pass 1.
  Symptom seen by the user: `+` in front of every param name after moving the
  plugin; `+` in the pick list (new feature) for every strip.
- **Bug C — plugin column blank — ROOT CAUSE FOUND, fix pending re-test:**
  the text WAS set correctly (also on re-open; JUCE recreates the rows on
  `visibilityChanged`, so `setRowAndColumn` re-runs each time). The picked
  plugin name was simply drawn in WHITE: `CSTPluginCombo` is the only
  editable cell in the codebase that did not set explicit `TextEditor`
  colours, and inside the `TableListBox` the window's `KlinkeLookAndFeel`
  defaults are not resolved for that editor (default text colour = white
  on white background). Fix: set `TextEditor::textColourId` (black) and
  `backgroundColourId` (white) explicitly in the `CSTPluginCombo` ctor,
  same pattern as all other editable cells. **Confirmed fixed by the user.**
  The temporary `CST` diagnostic logging has been removed again.
  Note: `MCU_DEBUG_LOG` is only defined for Debug builds (CMake generator
  expression `$<$<CONFIG:Debug>:MCU_DEBUG_LOG>`), so logging builds are
  Debug builds, not "Release with logging".
- **Strip clearing ("delete") — implemented:** row 0 of the plugin
  autocomplete popup is always a "— (no plugin) —" sentinel; picking it
  clears the whole strip (fxIdent, abbrev, VPOT mapping AND per-VPOT
  names). Any plugin pick also refreshes the row's other cells (Abbrev
  label, Edit button) via `ChannelStripBindingTable::resetCells()`.
  Return key never picks the sentinel (first real match instead).
- **Abbrev refresh — implemented:** selecting a different plugin in the
  plugin cell now ALWAYS regenerates the shortName from the new plugin
  name (previously only auto-filled when empty). A user-customised
  abbrev is overwritten when the plugin itself changes.
- **Step G — ALT commands implemented (2026-08-28), PENDING USER TEST:**
  - ALT+VPOT-1: floating FX window of the unit's strip FX on the selected
    track (mirrors `PlugWindowManager::openFloating`, reads PlugMode's
    current option settings; `m_lastFloat` for the "only 1 MCU" rule).
  - ALT+VPOT-7/8: move the strip FX up/down in the chain
    (`TrackFX_CopyToTrack` is_move, new `IMPAPI` entry + `vendor/csurf.h`
    extern), no-op at chain edges, `invalidateTrack()` afterwards.
  - ALT held: row 1 shows the legend "1: FX window  7: FX up  8: FX down"
    (refreshed via `m_lastAltState` in `frameUpdate()`, same pattern as Shift).
  - Built + deployed (Linux .so to UserPlugins + dist/). REAPER restart needed.
- **Step G first user test (2026-08-28) → OFF-BY-ONE FOUND + FIXED:** the
  ALT branch checked `vpot == 1/7/8` but `vpot` is 0-BASED (hardware VPOT N
  = vpot N-1). So hardware ALT+VPOT-1 (vpot 0) was never intercepted (no FX
  window at all), and hardware ALT+VPOT-8 (vpot 7) hit the `vpot == 7`
  ("move up") branch — the observed "plugin moved the wrong way".
  Fix: commands now at vpot 0 (open window) / 6 (up) / 7 (down). Also:
  legend moved from the full line into the per-VPOT fields ("OpnWin",
  "FXup", "FXdown", 6 chars via `changeField`), shown ONLY on units whose
  strip is active (assigned + plugin present, per-unit `fxSlot >= 0`).
  Diagnostic `CSM ALT cmd` / `CSM openFxWindow` / `CSM moveFx` log lines
  added (Debug builds). Rebuilt (Debug, logging on) + deployed.
  **User re-test needed:** does ALT+VPOT-1 open the window now, and do
  7/8 move in the expected direction? (The `CSM` log lines in
  `~/.config/REAPER/mcu_klinke_debug.log` show unit/vpot/fxSlot for the
  next round.)
- **Step G 7.75+ change (2026-08-29): slot machinery REBUILT — slot_hint
  is the winner (0-based).** Second user test with the candidate list
  PROVED: `TrackFX_SetNamedConfigParm(tr, fx, "slot_hint", "<N>")` moves
  the FX to 0-BASED slot N (slot_hint="2" landed the FX at slot 2, i.e.
  one past the target), while the `TrackFX_CopyToTrack 0x800000` flag has
  NO effect at all (both numberings). `tryMoveToUiSlot` therefore tries
  exactly one mechanism: `slot_hint = targetSlot` (0-based, the "+1"
  candidates caused the observed "moves by two slots" behavior), verifies
  via chain_index_to_slot, returns 1/0/-1. `moveFx` and `addPlugin`
  POS2..POS8 use it; after a successful slot_hint move the GUI is refreshed
  with `TrackList_AdjustWindows(false)`, `CSurf_OnFXChange(tr, 1)` and
  `TrackList_UpdateAllExternalSurfaces()` because REAPER does not redraw the
  TCP/MCP FX list on its own (user-reported stale GUI). PENDING USER TEST.
- **Step G 7.75+ GUI refresh revision (2026-08-29):** `UpdateArrange()` /
  `UpdateTimeline()` / external-surface notification alone did not refresh
  the mixer FX list. After a successful `slot_hint` write, the code now calls
  the track-panel/layout API `TrackList_AdjustWindows(false)`, explicitly
  notifies the FX change via `CSurf_OnFXChange(tr, 1)`, and then calls
  `TrackList_UpdateAllExternalSurfaces()`. This follows the SDK note that
  track-panel attributes may require `TrackList_AdjustWindows` and the
  existing codebase's FX-change notification pattern. Built + deployed;
  PENDING USER TEST.
- **Occupied-neighbour behavior (2026-08-29, revised):** `ALT+VPOT-7/8`
  detects the adjacent UI slot via `chain_slot_to_index`. If occupied, it now
  deliberately uses the original dense `TrackFX_CopyToTrack(sourceIdx,
  targetIdx, is_move=true)` path, which exchanges adjacent FX without using
  `slot_hint` insertion semantics or shifting later FX. Empty targets continue
  to use the 7.75+ `slot_hint` path. The earlier temporary-slot
  `swapUiSlots()` experiment was removed after user testing showed it did not
  move the FX at all.
- **Additional Step G commands (2026-08-29):** ALT+VPOT-5 is now `Delete`.
- **Common fader-touch display (2026-08-29):** `MultiTrackMode` now owns the
  shared fader-touch overlay used by PanMode, CommandMode, and
  ChannelStripMode. While exactly one channel fader is touched, row 1 shows
  the current channel volume in dB, or pan in flip mode; for QCon ProX units
  the overlay is skipped because those values already use the dedicated
  display rows. Releasing the fader restores the active mode's normal
  display. The implementation runs after derived-mode display updates so
  their regular row-1 content is restored automatically when touch ends.
  A later test also found and fixed the floating action variable type: the
  `TrackFX_Show` action must be an `int` (3 was truncated to 1 when it was a
  `bool`).
  It deletes the assigned FX instance from the selected track, keeps the
  global strip mapping intact, refreshes TCP/MCP via the same mixer update
  sequence, and leaves the strip in the normal missing-plugin picker state.
  The floating-window action bug was fixed: its local `TrackFX_Show` action
  variable is now an `int`, so action 3 is no longer truncated to action 1
  (chain).
- **Step G 7.75+ change (2026-08-28): slot-targeted plugin add (SUPERSEDED
  by the 2026-08-29 entry above, kept for context).**
  User report: Insert Position 2 was ignored — on an empty track the plugin
  landed in slot 1. Root cause: the classic `instantiate <= -1000` dense
  position ("-1001 = second item in chain") cannot address a position BEYOND
  the current chain end — REAPER clamps it. Fix: for POS2..POS8 on 7.75+,
  `addPlugin` now adds at the end and moves the FX into the target UI slot
  via `TrackFX_CopyToTrack(dest = slot | 0x800000)` (unused-slot flag),
  leaving the earlier slots EMPTY. Verification: re-find the FX by GUID and
  compare `chain_index_to_slot` against the target; if not honored (empty-
  slot option off / unsupported) the add is UNDONE (`TrackFX_Delete`) and
  the classic dense insertion is used. FIRST/LAST stay classic (dense
  positions by definition). New helper `ChannelStripAccess::uiSlotForIndex()`
  (chain_index_to_slot). Logged as `CSA addPlugin SLOT-TARGET`. PENDING
  USER TEST (needs the REAPER 7.75 "allow empty slots" option enabled).
- **Step G 7.75+ change (2026-08-28): slot-aware FX move (empty FX slots).**
  REAPER 7.75 added empty FX slots: the user-visible SLOT (1-based, includes
  empties) can differ from the FX index (0-based, real FX only). `moveFx`
  now moves by SLOT on 7.75+ via `TrackFX_GetNamedConfigParm`
  `chain_index_to_slot` (index → slot) and `chain_slot_to_index` (slot →
  index, `"empty:x"` for empty slots); an empty target slot is addressed
  with the `0x800000` unused-slot flag of `TrackFX_CopyToTrack`, an
  occupied one as a plain dense index. REAPER < 7.75 (or any failed slot
  read, e.g. chain edge): fallback to dense index +/- 1 (no-op at real
  edges). All intermediate values are logged (`CSM moveFx slot-aware /
  FALLBACK / index-based`) — the exact call semantics (slot passed in the
  `fx` argument, `0x800000|slot` for empty targets) are SDK-documented but
  NOT yet user-verified. PENDING USER TEST on a track with empty slots.
- **Step G change (2026-08-28): PlugMode window settings ignored.**
  Maintainer decision: ALT+VPOT-1 TOGGLES the floating window (`TrackFX_Show`
  3 open / 2 close, checked via `TrackFX_GetFloatingWindow`) and NEW
  ALT+VPOT-2 TOGGLES the chain (1 open / 0 close, checked via
  `TrackFX_GetChainVisible != -1`) — no PlugMode option plumbing (the
  `m_lastFloat` "only 1 MCU" tracking and the PlugMode option reads were
  removed). Legend texts: "Float" / "Chain" / "FXup" / "FXdown". Rebuilt
  (Debug, logging) + deployed.
- **Stale "Goodbye" on row 0 at startup — fixed (2026-08-28):**
  `MultiTrackMode::updateDisplay` wrote only the 6-char channel fields, so
  the 7th (separator) column of each 7-column field was never touched — the
  "Goodbye" line written at shutdown (centered, columns 24-30) bled through
  its separator-column letters on the next start. Fix: clear the ENTIRE row
  0 via `changeTextFullLine(0, "")` at the start of `updateDisplay` (display
  line buffer → no extra SysEx while unchanged). Same pattern ChannelStrip
  mode already used for row 1. Rebuilt + deployed.

### Next concrete steps when resuming

1. ~~Confirm Bug A/B root cause from the log~~ — done (path-ident mismatch,
   see above).
2. ~~Fix `findSlotByIdent`~~ — done (`installedNameForIdent` name fallback).
   **User re-test needed:** assign strip on empty track, move the FX in the
   chain, verify no `+` and no duplicate add on VPOT press.
3. ~~Re-test Bug C~~ — done, confirmed fixed (white TextEditor colour, see
   above).
4. **Step E verification:** confirm `channelstrips.xml` is written/read correctly
   (one file, no per-Abbrev files left over) and assignments survive a project
   save/reload.
5. ~~Step F~~ — done (reorder invalidation via `plugMoved`; FX delete handled
   implicitly by per-access GUID verification + picker/"+" fallback, refreshed
   every frame; track-deletion pruning deliberately skipped — see §F).
   Next: **Step G** (ALT+VPOT-1 open FX window, ALT+VPOT-7/8 move FX,
   ALT+Name/Values labels).
6. Before release: build **Release** (logging is Debug-config only — see note
   in the Bug C entry) and remove the remaining A/B `CSA`/`CSM` diagnostic
   logging if it is no longer wanted.

### Build/deploy reminder

- Build with logging (current state): `cd build && cmake .. -DCMAKE_BUILD_TYPE=
  Release -DMCU_DEBUG_LOG=ON && cmake --build . -- -j$(nproc)`
- Deploy: `cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/`
- REAPER must be **fully restarted** to reload the `.so`.
- Start for testing: `GDK_BACKEND=x11 /home/fuerst/opt/REAPER/reaper`
