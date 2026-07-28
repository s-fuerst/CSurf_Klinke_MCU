# Channel Strip Mode — Implementation Plan

> Date: 2026-07-28
> Status: **PLANNING** (no code changes yet)
> Origin: `notes.org` → "ChannelStripe Idee (Sketch)"

## 0. Executive Summary

A new MCU mode — **Channel Strip Mode** — that exposes the most-used FX
parameters of the selected track's FX chain as a flat surface of 8 VPOTs
(16 with Shift). The classic mixing-console idea: EQ + dynamics + gain on one
channel, without opening each plugin.

It is activated by the **TRACK button** (`B_VPOT_TRACK`, `0x28`) in the VPOT
assign section — the only assign button not yet bound to a mode
(`EQ`=Action, `SEND`=Send/Receive, `PAN`=Pan, `PLUG`=Plug). It stays active
until another assign button is pressed (same behaviour as Plug/Pan/Action,
not the momentary switch-back of Send/Receive).

The mode reuses the established architectural patterns of **PlugMode** (the
closest existing mode) almost wholesale — only the parameter-mapping model is
new, and it is deliberately simpler (flat 16 slots vs. PlugMode's
8×8×8 bank/page/fader/vpot cube).

### Confirmed design decisions

| Decision | Choice | Rationale |
|---|---|---|
| Slot model | **Flat: 1 VPOT = 1 parameter** | A curated list of the 16 most important knobs across the whole FX chain. Simpler than reusing PlugMap; no bank/page navigation. |
| Persistence key | **Per-track + per-unit** `(trackGUID, unitIndex)` | Matches `notes.org` ("the track remembers the assignment of each unit to the plugins"). Each 8-channel unit has its own strip for the same track. |
| Activation behaviour | **Stay active** (like Plug/Pan/Action) | The mode is most similar to PlugMode; no switch-back. |
| Shift / Multi-Unit | **Baked in from step 1** | Not deferred to a late phase. The 16-slot map is shift-aware by construction; per-track+per-unit storage is multi-unit aware by construction. |
| Stock-plugin maps | **No catalog code** | Ship a few example map files into the user save location (same dir `ChannelStripMapManager` loads from). No dedicated "recommended params" table. |

---

## 1. Feature (from `notes.org`)

- A **Channel Strip Map** = exactly 16 parameter bindings (VPOTs 1–8 normal,
  9–16 via Shift). Operated via VPOTs. Base is a Track Mode.
- **First-time selection**: if no channel-strip plugin is assigned to this
  unit for the selected track yet, the 8 names appear in the display and can
  be selected via VPOT. If the track does not yet have the plugin, a `+`
  appears before the name; selecting it adds the plugin to the track.
- **Insert position** (per binding): first, n-th (2–8), or last — controls
  where `+` inserts the plugin into the chain.
- **Duplicates allowed**: the same plugin may occupy several slots to edit
  different aspects (e.g. EQ bands once, compressor once). Selecting it again
  does **not** add a second instance — it reuses the existing one.
- **VPOT press**: no separate press mapping. A press sets the parameter to 1
  if the current value != 1, otherwise to 0 (toggle).
- **Track remembers per-unit plugin assignments** and saves them in the
  project.
- **On-screen editor**: opened by **ALT+TRACK** (the assign button, like
  Plug/Pan/Action). The editor is a 16-row table —
  `Number | Plugin | 5-char display abbreviation | Insert Pos | Open Map`.
- **ALT+VPOT-1**: opens the floating FX window of the plugin controlled by
  that slot's binding (respecting PlugMode's open-window-count rules;
  duplicate that option into this mode).
- **ALT+VPOT-7 / ALT+VPOT-8**: move the FX up / down in the chain. These
  shortcuts are also implemented in PlugMode.
- **ALT+Name/Values**: shows these commands in the display.
- Modifier choice (ALT vs CONTROL) to be finalised during implementation;
  PlugMode already uses ALT+SELECT for plugin windows, so ALT is the default.

---

## 2. How it maps onto the existing architecture

PlugMode is the template. The new mode reuses its infrastructure directly:

| Concept (notes.org) | Existing pattern to reuse |
|---|---|
| New CCSMode, "Track Mode" base | `CCSMode` subclass, mirroring `PlugMode` (signal wiring in ctor, `activate`/`updateDisplay`) |
| Activation via TRACK button | `B_VPOT_TRACK` (`0x28`) — free — added as a `case` in `CCSManager::buttonVPOTassign` |
| "+ adds plugin to track" | `TrackFX_AddByName` (insert pos via `instantiate <= -1000`); ident list from `EnumInstalledFX` |
| Read/write parameters | `TrackFX_GetParam` / `TrackFX_SetParam`; names from `TrackFX_GetParamName` (a `PlugAccess`-style wrapper) |
| Track plugin move/delete | `PlugMoveWatcher` singleton, `connectPlugMoveSignal` — exactly as PlugMode uses it |
| Per-track persistence in project | `ProjectConfig::connect2ProjectChangeSignal` + XML (like `TrackState` / `PlugMapManager::writeLocalMapsToProjectConfig`) |
| Map save/load to file | `PlugMapManager`-style installed/user map locations under `GetResourcePath()` |
| VPOT LEDs, assignment display | `updateVPOTLeds`, `setAssignmentDisplay` in `CCSManager` |
| On-screen JUCE editor | `createEditorComponent`, mirroring `PlugModeComponent` |

### REAPER API surface used

- `EnumInstalledFX(index, &name, &ident)` — enumerate installed FX for the
  editor's plugin combo box; `ident` carries the type prefix
  (`VST3:`, `VST:`, `AU:`, `JS:`, `DX:`, `CLAP:`).
- `TrackFX_AddByName(track, ident, recFX, instantiate)` — the "+" flow;
  `instantiate <= -1000` encodes the insert position.
- `TrackFX_GetCount`, `TrackFX_GetFXName`, `TrackFX_GetFXGUID` —
  `fxIdent → fxGUID` resolution by name match.
- `TrackFX_GetNumParams`, `TrackFX_GetParamName`, `TrackFX_GetParam`,
  `TrackFX_SetParam` — parameter enumeration, display, and control.
- `TrackFX_CopyToTrack(src, srcFx, dst, dstFx, is_move)` — ALT+VPOT-7/8
  reorder; `src == dst`, `is_move = true` reorders in place.
- `TrackFX_Delete` — kept available for future "remove binding" actions.

---

## 3. Data model

```cpp
// One VPOT slot (1..16). 16 = 8 normal + 8 Shift.
struct ChannelStripBinding {
    String fxIdent;                 // EnumInstalledFX ident, stable across tracks
                                    //   -> used to find/add the plugin on the track
    GUID    fxGUID;                 // TrackFX_GetFXGUID of the instance (runtime)
                                    //   -> used to address it; survives reordering
    int     paramIndex;             // parameter index within that FX
    String  shortName;              // <=5 chars for the display
    enum InsertPos { FIRST, P2, P3, P4, P5, P6, P7, P8, LAST };
    InsertPos insertPos;            // where "+" inserts the plugin
};

class ChannelStripMap {             // exactly 16 bindings, flat
    ChannelStripBinding slots[16];
    String creator;
    String info;
    void writeToXml(XmlElement*);
    bool readFromXml(XmlElement*);
};
```

**Two-stage plugin identification** (important):
- `fxIdent` (stable, track-independent) drives the "+" logic via
  `TrackFX_AddByName`.
- `fxGUID` (per-instance) drives runtime access and survives reordering.

On activation the mode resolves `fxIdent → fxGUID` by scanning the track's FX
chain (`TrackFX_GetFXName` match). If the plugin is missing the binding is
shown in "+" state and is not auto-added until the user selects it.

### Persistence (key = trackGUID, unitIndex)

```xml
<CHANNELSTRIP>
  <UNIT index="0">
    <SLOT nr="1" fxident="VST3:ReaEQ (Cockos)" fxguid="{..}" param="3" name="EQG1" inspos="last"/>
    ...up to 16...
  </UNIT>
  <UNIT index="1"/>
  ...
</CHANNELSTRIP>
```

Stored via `ProjectConfig::connect2ProjectChangeSignal`, handled for
`READ` / `WRITE` / `FREE` exactly like the existing per-track state.

---

## 4. Module structure

New directory `src/modes/channelstrip/`, mirrored on `src/modes/plugin/`:

```
src/modes/channelstrip/
├── ChannelStripMode.{cpp,h}          # CCSMode subclass, wired to B_VPOT_TRACK; stays active
├── ChannelStripBinding.{cpp,h}       # one slot: fxIdent/fxGUID/param/shortName/insertPos
├── ChannelStripMap.{cpp,h}           # 16 bindings + XML (flat)
├── ChannelStripAccess.{cpp,h}        # TrackFX_* wrapper; fxIdent<->fxGUID resolution
├── ChannelStripTrackState.{cpp,h}    # per-(trackGUID,unitIndex) assignment + ProjectConfig
└── editor/
    ├── ChannelStripComponent.{cpp,h}        # main editor (unit selector + table)
    ├── ChannelStripBindingTable.{cpp,h}     # 16 rows: # | Plugin | abbrev | InsPos | PickParam
    └── ChannelStripParamPicker.{cpp,h}      # parameter list per plugin
```

### Wiring changes to existing files

- **`src/core/CCSManager.{h,cpp}`** — add `ChannelStripMode* m_pChannelStripMode`;
  construct/free it; add `case B_VPOT_TRACK: pNewMode = m_pChannelStripMode;`
  in the pressed branch of `buttonVPOTassign`; open its editor in the ALT
  branch (`setMainComponent(m_pChannelStripMode, true)`); add B_VPOT_TRACK
  LED logic in `updateVPOTLeds` (blink if active / on if the selected track
  has a strip / off otherwise — mirror the PLUG logic); add
  `getChannelStripMode()`.
- **`src/hardware/mcu_button_defines.h`** — `B_VPOT_TRACK` (`0x28`) already
  defined; no change.
- **`CMakeLists.txt`** — add the new sources.

---

## 5. Implementation steps (ordered)

19 steps across 7 milestones. Each step is a buildable, testable increment;
dependencies resolve downward. Shift and multi-unit are handled inline from
the first runtime step, not as a late phase.

### A — Reachable skeleton

1. **Directory + skeleton**: `src/modes/channelstrip/` with an empty
   `ChannelStripMode` (`CCSMode` subclass); add to `CMakeLists.txt`. Multi-unit
   from the start: the mode knows about all units.
2. **CCSManager wiring**: member, ctor/dtor, `case B_VPOT_TRACK` (stays
   active), LED stub in `updateVPOTLeds`, `getChannelStripMode()`.
   → pressing TRACK switches to an empty mode.

### B — Data + access (drivable from hardware; Shift + multi-unit inline)

3. **`ChannelStripBinding` + `ChannelStripMap`**: 16 slots (= 8 + 8 Shift),
   XML read/write.
4. **`ChannelStripAccess`**: `fxIdent → fxGUID` per unit, `TrackFX_GetParam`/
   `SetParam`, `TrackFX_GetParamName`, dangling detection.
5. **Runtime path**: the runtime entry points (`vpotMoved`/`vpotPressed`/
   `updateDisplay`/`updateVPOTs`) read the per-unit map and drive
   `ChannelStripAccess`. No throwaway test map — the editor (Step C) is the
   real configuration surface and thus the end-to-end test.

### C — JUCE editor (manual configuration)

6. **`ChannelStripComponent`** + active-unit selector.
7. **`ChannelStripBindingTable`**: 16 rows — # | Plugin (combo from
   `EnumInstalledFX`) | abbreviation (max 5) | InsertPos.
8. **`ChannelStripParamPicker`**: parameter list per plugin
   (`TrackFX_GetParamName`).
9. **Hook up + file I/O**: `createEditorComponent`; the editor is opened by
   **ALT+TRACK** (added to the ALT branch of `CCSManager::buttonVPOTassign`);
   edits write back to the per-unit map and refresh the display. Map
   save/load to file in the user location — **ship a few example maps there**
   (no catalog code).
   The runtime entry points (`vpotMoved`/`vpotPressed`/`updateDisplay`/
   `updateVPOTs`) read the per-unit map and drive `ChannelStripAccess`, so the
   editor is the real end-to-end test surface (no throwaway test map).

### D — First-time / "+" flow

10. **"+" state**: plugin missing → `+` before the name in the display.
11. **Auto-add**: selection inserts via `TrackFX_AddByName(fxIdent, insPos)`,
    captures the new `fxGUID`.

### E — Persistence

12. **`ChannelStripTrackState`**: per-`(trackGUID, unitIndex)` map +
    `ProjectConfig` signal connect/disconnect + XML. `READ`/`WRITE`/`FREE`.
13. **Roundtrip**: load on project open, write on change, survives a REAPER
    restart — now testable because the editor and "+" flow exist.

### F — Move/delete tracking

14. **`PlugMoveWatcher`**: reorder → re-resolve `fxGUID`.
15. **FX delete / track remove**: mark binding dangling, update display.

### G — ALT shortcuts

16. **ALT+VPOT-7 / ALT+VPOT-8**: move the FX up/down in the chain
    (`TrackFX_CopyToTrack`, `is_move = true`).
17. **ALT+VPOT-1**: open the floating FX window of the plugin controlled by
    that slot's binding (window-count option reused/duplicated from PlugMode's
    `PlugWindowManager`).
18. **ALT+Name/Values**: show the command labels in the display.
19. **ALT+VPOT-7/8 also in PlugMode** (per `notes.org`: "also implemented in
    PlugMode").

---

## 6. Notes / open items

- **Modifier choice** (ALT vs CONTROL) — default ALT for consistency with
  PlugMode (ALT+SELECT opens plugin windows); confirm during implementation.
- **Insert-position semantics** — `instantiate <= -1000` encodes position
  (`-1000` = first, `-1001` = second, …); `LAST` is resolved at add time as
  the current chain length.
- **Duplicate-instance reuse** — when a binding's `fxIdent` matches an FX
  already on the track, reuse that instance's `fxGUID`; do not add another.
- **Per-unit cost** — up to 8 units × 16 slots = 128 bindings per track in the
  project XML; acceptable.
- Release hardening (three-platform build, ASan/leak pass, manual + AGENTS.md
  mode-table docs, `notes.org` → DONE) is tracked separately under the general
  pre-release checklist, not as feature-plan steps.
