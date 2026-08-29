# Modifier+Control Command Scheme — Architecture

Status: IMPLEMENTED (2026-08-29), all phases (0–3) done.
Open questions Q1–Q3 decided (all yes) 2026-08-29; Q3a and the PlugMode
legend row decided 2026-08-29 (see §6); PlugMode VPOT-3 therefore not
registered.
Trigger: the ChannelStripMode CTRL commands (Step G, see
`channelstrip-mode-plan.md`) work and are to be extended to other modes —
notably PlugMode, which shares the same FX-chain commands (open floating
window, open/close FX chain, move FX up/down).

## 1. Goal

A small, reusable scheme so that any CCSMode can offer
"modifier key + hardware control → command" bindings with:

- interception of the control event (VPOT press today; buttons tomorrow),
- a per-mode display legend while the modifier is held,
- display refresh on modifier press/release,
- **shared command implementations** where several modes act on the same
  target type (here: an FX slot on a track).

Design constraint: the existing architecture "the mode owns its events and
its display rows" stays intact. CCSManager/CSurf_MCU stay event plumbers,
not command owners.

## 2. Current state (as of Phase 0)

- `ChannelStripMode::vpotPressed` intercepts CONTROL inline:
  `if (isModifierPressed(VK_CONTROL) && vpot < 8 && (vpot == 0 || ...))`
  with an if-chain per command; preconditions (assigned strip + plugin
  present) resolved per unit.
- Command bodies live in `ChannelStripMode`:
  - `openFxWindow(tr, fxSlot, floating)` — pure REAPER API
    (`TrackFX_GetFloatingWindow` / `TrackFX_GetChainVisible` / `TrackFX_Show`),
    plus `updateEverything()`.
  - `moveFx(tr, fxSlot, dir)` — REAPER 7.75+ slot-aware move via
    `ChannelStripAccess::uiSlotForIndex` / `tryMoveToUiSlot`, dense-index
    fallback; mode-specific parts: `m_pAccess->invalidateTrack(tr)`
    (ChannelStrip slot cache) + `updateEverything()`.
  - remove (VPOT-5) — inline in `vpotPressed`: `TrackFX_Delete` +
    `TrackList_AdjustWindows` + `CSurf_OnFXChange` +
    `TrackList_UpdateAllExternalSurfaces` + `invalidateTrack` +
    `updateEverything`.
- Legend: `updateChannel()` rewrites the per-VPOT row-1 fields while
  CONTROL is held; `frameUpdate()` compares `m_lastCtrlState` to poll the
  live key state.
- Every mode that needs this today would copy that pattern, including its
  own `m_lastXxxState` member.

### Modifier plumbing (hardware + host)

- `CSurf_MCU::OnKeyModifier`: Mackie modifier CCs 0x46–0x49 (70–73) set bits
  1..8 of `s_mackie_modifiers` (70=SHIFT, 71=OPTION, 72=CONTROL, 73=ALT).
- `CSurf_MCU::IsModifierPressed(VK_*)` ORs the MIDI bit with the host
  keyboard (`IsKeyboardPressed`), so all four modifiers work from the
  computer keyboard too. `VK_CONTROL` is already used elsewhere
  (MultiTrackMode SELECT multi-select toggle; PlugMode SOLO/MUTE/REC
  bank/page/preset) — all of that is BUTTON-based, none of it VPOT-based.
- SHIFT is reserved for ADDRESSING (e.g. VPOT bank extension, `slotFor`),
  not for commands — keep it that way.

## 3. Proposal — three layers

### Layer 1: `FxSlotCommands` — shared, mode-agnostic command bodies

New file `src/core/FxSlotCommands.h/.cpp`. Static functions, pure REAPER
API, no mode/csurf dependencies. Post-processing (cache invalidation,
`updateEverything`) stays in the CALLING MODE — the helper does the FX
operation and returns success.

```cpp
class FxSlotCommands {
public:
  // Toggle the floating FX window of the slot (open or close).
  static void toggleFloatingWindow(MediaTrack *tr, int fxSlot);
  // Toggle the track's FX chain window (open or close).
  static void toggleFxChain(MediaTrack *tr);
  // Move the FX by dir (+1/-1) in the chain, slot-aware on REAPER >= 7.75,
  // dense-index fallback below. false = refused (chain edge / moved
  // unexpectedly).
  static bool moveFx(MediaTrack *tr, int fxSlot, int dir);
  // Remove the FX slot and notify the surfaces. false = refused.
  // (Q1 decided 2026-08-29: shared with PlugMode.)
  static bool removeFx(MediaTrack *tr, int fxSlot);
};
```

Moves out of `ChannelStripMode`: the whole body of `openFxWindow` (minus
`updateEverything`), the body of `moveFx` (it already only uses the static
`ChannelStripAccess::uiSlotForIndex` / `tryMoveToUiSlot` helpers — no
instance state), and the remove block. `ChannelStripMode::moveFx/
openFxWindow` become thin delegators (or are deleted and the handlers call
`FxSlotCommands` directly).

Behaviour is byte-identical after the move — this is a pure refactor.

### Layer 2: `ModifierCommands` — per-mode dispatch table

New file `src/core/ModifierCommands.h/.cpp`. One instance per mode,
populated in the mode's constructor:

```cpp
class ModifierCommands {
public:
  // channel: global 1-based channel; return true = event consumed.
  using Handler = std::function<bool(int channel)>;

  void add(int modifier, int control, Handler handler);
  // Try to dispatch; false = no command matched (fall through to normal
  // behaviour of the control).
  bool dispatch(int modifier, int control, int channel);
  // For legends: does this modifier have any commands at all?
  bool hasCommands(int modifier) const;
};
```

Usage in a mode (`control` = the mode-local position, e.g. the unshifted
0..7 VPOT index):

```cpp
// constructor
m_ctrlCommands.add(VK_CONTROL, 0,
    [this](int ch) { return ctrlOpenFloating(ch); });
m_ctrlCommands.add(VK_CONTROL, 1,
    [this](int ch) { return ctrlToggleChain(ch); });
// ...

// vpotPressed (top of the function)
if (m_ctrlCommands.dispatch(VK_CONTROL, localCh, channel))
  return true;
```

Properties:

- Precondition logic (strip assignment, plugin present, PlugMode's
  accessed-plugin check) stays inside the mode's HANDLER — the table only
  routes.
- Adding/removing a command is one line; no if-chains in event handlers.
- The table is trivially testable (dispatch with fake handlers) and makes
  each mode's command set self-documenting at the construction site.
- Deliberately NOT a global registry in CCSManager: the manager would have
  to know per-mode control semantics, and the unit/channel context is
  already resolved inside the mode.

### Layer 3: modifier-state tracking in the `CCSMode` base

Today every mode that shows a legend keeps its own `m_lastCtrlState` /
`m_lastAltState` / `m_lastShiftState` member plus a poll in
`frameUpdate()`. Move the pattern into the base:

```cpp
// CCSMode
protected:
  // Poll the live modifier state; returns true only on the press/release
  // edge since the last call. Modes call this in frameUpdate() and refresh
  // their display on true.
  bool modifierStateChanged(int modifier);
private:
  int m_lastModifierBits;
```

`ChannelStripMode::m_lastCtrlState` disappears (SHIFT tracking can move to
the base later where modes use it for the same reason — not required now).

## 4. Command sets per mode (as implemented)

Same VPOT positions, same legend labels, in every mode that adopts the
scheme — the user muscle memory transfers. Only the (track, fxSlot)
RESOLUTION differs per mode:

| CTRL+VPOT | Command | ChannelStripMode target | PlugMode target |
|---|---|---|---|
| 1 | `Float` | strip FX of the unit on the selected track | the accessed plugin (`PlugAccess::getPlugTrack/getPlugSlot`) |
| 2 | `Chain` | selected track's FX chain | same track's FX chain |
| 3 | mode switch | switch to PlugMode + select strip FX ("PlMode") | **not registered** (Q3a: omitted for now) |
| 5 | `Remove` | strip FX | the accessed plugin |
| 7 / 8 | `FXup` / `FXdown` | strip FX | the accessed plugin |

Implementation notes (deviations / additions to the original proposal):

- `FxSlotCommands::toggleFxChain` keeps the `fxSlot` parameter (the
  proposal's signature had none) because `TrackFX_Show` requires a slot
  argument even for chain show/hide.
- `FxSlotCommands` owns `findSlotByGUID` / `uiSlotForIndex` /
  `tryMoveToUiSlot` (moved from `ChannelStripAccess`, which now delegates
  to them) so Layer 1 stays free of mode dependencies.
- `ModifierCommands` gained `hasCommand(modifier, control)` beyond the
  proposed API: a MATCHED-but-inactive command must consume the event
  (no fall-through to the control's normal behaviour), which
  `dispatch()` alone cannot express.
- `CCSMode::modifierStateChanged()` is used by ChannelStripMode (legend
  edge refresh). PlugMode does not need it: it redraws its display every
  frame (`frameUpdate` → `updateEverything`), so `updateCtrlLegend()`
  simply follows the live modifier state.
- PlugMode re-establishes the accessed plugin after its own mutations:
  move → `accessPlugin(tr, newSlot)` by GUID; remove → re-access whatever
  now occupies the slot, or deselect (`accessPlugin(tr, -1)`) if the
  chain shortened. With `PMO_LIMIT_FLOATING` set to "only chain", the
  `Float` command is INACTIVE in PlugMode (no legend entry, press is a
  no-op): the per-frame `checkFloatWindows()` would otherwise
  immediately convert the floating window into the chain.
- Legend row: row 1 on ALL PlugMode layouts (maintainer decision
  2026-08-29) — on an MCU 2-row unit that overwrites the fader names,
  exactly like ChannelStripMode.

### PanMode command set (Phase 4)

PanMode uses the same Layer-2 table but a DIFFERENT command set — the
commands act on the TRACK on the pressed channel, not on an FX slot, so
`FxSlotCommands` (Layer 1) is not involved. Target =
`getMediaTrackForChannel(channel)`. All commands need a track on the
channel except `Insert`, which inserts at position 0 on an empty channel
(and therefore keeps its legend entry there).

| CTRL+VPOT | Legend | Action (target: the channel's track) |
|---|---|---|
| 1 | `Insert` | insert a new track directly AFTER the channel's track (`InsertTrackAtIndex`); position 0 when the channel is empty |
| 2 | `Duplic` | duplicate the track WITH items and envelopes via its RPPXML state chunk: `GetSetObjectState(tr, "")`, drop the top-level `GUID` line, apply to a fresh track |
| 3 | `Clear` | delete EVERY item (`GetTrackMediaItem`/`DeleteTrackMediaItem` loop) and EVERY envelope (strip every `<ENV>` section from the track chunk) |
| 4 | `Remove` | remove the track (`DeleteTrack`) |

Implementation notes:

- New REAPER APIs resolved in `csurf_main.cpp` (all REAPER ≤ 4.0 era, well
  below the 6.37 load floor — the floor is unchanged): `CountTracks`,
  `GetTrack`, `InsertTrackAtIndex`, `DeleteTrack`, `GetTrackMediaItem`,
  `DeleteTrackMediaItem`. Externs declared in `vendor/csurf.h`.
- REAPER has NO "duplicate track" and NO "delete whole envelope" API. (An
  earlier attempt used `TrackList_*`/`Track_*`/`TrackEnvelope_*` names —
  none of those exist in REAPER, the extension silently refused to load,
  and the names were replaced with the real SDK-header ones.) The two
  commands therefore work on the RPPXML state chunk (`GetSetObjectState`,
  already resolved):
  • Duplic = whole track chunk (items, envelopes, FX, settings) with the
    track's own top-level `GUID` line removed (`withoutTopLevelGUID`),
    applied to a fresh track — guarantees the copy gets a new GUID (the
    extension tracks tracks by GUID).
  • Clear = items via API, then every `<ENV>` section stripped from the
    chunk (`stripEnvelopeSections`, brace-depth-aware; the write-back is
    skipped unless the surgery left the chunk balanced and removed
    something).
  Every command is wrapped in one `Undo_BeginBlock`/`Undo_EndBlock` pair
  → exactly one undo point per command.
- PanMode has no normal VPOT-press behaviour, so a non-matching CTRL press
  is a plain no-op (fall-through returns false).
- Legend: row 1, same convention as the other modes. PanMode redraws its
display every frame (`updateDisplay`), so `updateCtrlLegend()` follows the
  live modifier state — no edge tracking, no `modifierStateChanged()`.
- Label is `Duplic` (not `Copy`) per maintainer, matching REAPER's
  "Duplicate" vocabulary (2026-09).

Notes:

- **PlugMode target semantics (Q2, confirmed 2026-08-29):** PlugMode's
  accessed plugin is MODE-level (one at a time), not per unit. The command
  therefore acts on the accessed plugin regardless of which unit's VPOT is
  pressed (the existing `vpotPressed` already calls `setActiveUnit(unit)`
  first, which is fine). If per-unit targets were wanted later, the handler
  resolves them — the table layer does not care.
- **PlugMode has CONTROL+VPOT free today** (its CONTROL semantics are on
  SOLO/MUTE/REC buttons). No conflict.
- **ChannelStripMode** keeps its current set unchanged (Phase 0).

## 5. Phases

- **Phase 0 — DONE (2026-08-29):** ChannelStripMode commands on CONTROL
  (was ALT), incl. PlMode (VPOT-3). Ad-hoc inline implementation.
- **Phase 1 — DONE (2026-08-29):** `FxSlotCommands` extracted to
  `src/core/FxSlotCommands.h/.cpp`; `ChannelStripMode` delegates to it.
  Pure refactor, no behaviour change. Smoke test of the four FX commands
  still owed by the user.
- **Phase 2 — DONE (2026-08-29):** `ModifierCommands`
  (`src/core/ModifierCommands.h/.cpp`) + `CCSMode::modifierStateChanged`
  introduced; `ChannelStripMode` refactored onto both (its
  `m_lastCtrlState` is gone). No behaviour change.
- **Phase 3 — DONE (2026-08-29):** PlugMode adopts the shared set (VPOT
  1/2/5/7/8, target = accessed plugin; VPOT-3 omitted per Q3a), legend on
  PlugMode's row 1 (all layouts). User test owed.
  (Supersedes the old "deferred: ALT+VPOT-7/8 in PlugMode" note in
  `channelstrip-mode-plan.md`.)

Each phase is independently shippable and testable; Phase 3 needs nothing
from Phases 1–2 to function (it could even be written ad-hoc first), but
doing 1–2 first keeps the shared code where it belongs.
- **Phase 4 — DONE (2026-09):** PanMode adopts the Layer-2 table with its
  OWN track-manipulation command set (CTRL+VPOT 1/2/3/4 = Insert/Duplic/
  Clear/Remove, target = the channel's track). Duplicate and envelope
  removal work via the RPPXML state chunk (no native REAPER APIs exist
  for them); the other operations use newly resolved track/item APIs in
  `csurf_main.cpp`. User smoke test owed.

## 6. Open questions (maintainer decision)

Decided 2026-08-29 (all yes), implemented the same day:

- **Q1 (yes):** `Remove` (VPOT-5) goes to PlugMode as well.
- **Q2 (yes):** PlugMode command target = the mode-level accessed plugin.
- **Q3 (yes):** the mode-switch slot (VPOT-3, "PlMode" in ChannelStripMode)
  also exists in the other modes as a generic "switch to mode X" slot.

- **Q3a (decided 2026-08-29):** OMIT PlugMode's VPOT-3 for now — the
  mode-switch slot is only registered in ChannelStripMode ("PlMode" →
  PlugMode). Revisit when a concrete target mode / last-mode behaviour is
  wanted.
- **Q4:** Buttons as controls: the table's `control` index is positional
  today (VPOT 0..7). If button commands are ever wanted, the entry key
  becomes a small (EElement, position) pair — noted here so the interface
  does not need to be re-broken later; not needed now.
