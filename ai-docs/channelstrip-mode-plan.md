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

### G — ALT shortcuts

- ALT+VPOT-7 / ALT+VPOT-8: move FX up/down (`TrackFX_CopyToTrack`, `is_move=true`).
- ALT+VPOT-1: open floating FX window (PlugMode window-count rules duplicated).
- ALT+Name/Values: show command labels in the display.
- ALT+VPOT-7/8 also in PlugMode (per `notes.org`).

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
