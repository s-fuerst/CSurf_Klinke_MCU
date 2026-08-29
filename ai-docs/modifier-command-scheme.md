# Modifier+Control Command Scheme — Architecture Proposal

Status: PROPOSAL (2026-08-29), not yet implemented except Phase 0.
Open questions Q1–Q3 decided (all yes) 2026-08-29; implementation on hold
per maintainer.
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

## 4. Command sets per mode (target state)

Same VPOT positions, same legend labels, in every mode that adopts the
scheme — the user muscle memory transfers. Only the (track, fxSlot)
RESOLUTION differs per mode:

| CTRL+VPOT | Command | ChannelStripMode target | PlugMode target |
|---|---|---|---|
| 1 | `Float` | strip FX of the unit on the selected track | the accessed plugin (`PlugAccess::getPlugTrack/getPlugSlot`) |
| 2 | `Chain` | selected track's FX chain | same track's FX chain |
| 3 | mode switch | switch to PlugMode + select strip FX ("PlMode") | generic mode-switch slot (target mode: open design point, Q3=yes) |
| 5 | `Remove` | strip FX | the accessed plugin |
| 7 / 8 | `FXup` / `FXdown` | strip FX | the accessed plugin |

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
- **Phase 1:** extract `FxSlotCommands`; `ChannelStripMode` delegates to it.
  Pure refactor, no behaviour change. Build + user smoke test of the four
  FX commands.
- **Phase 2:** introduce `ModifierCommands` + `CCSMode::modifierStateChanged`;
  refactor `ChannelStripMode` onto both. No behaviour change.
- **Phase 3:** PlugMode adopts the shared set (VPOT 1/2/5/7/8, target =
  accessed plugin; VPOT-3 = mode-switch slot per Q3), legend on PlugMode's
  row-1 layout. User test.
  (Supersedes the old "deferred: ALT+VPOT-7/8 in PlugMode" note in
  `channelstrip-mode-plan.md`.)

Each phase is independently shippable and testable; Phase 3 needs nothing
from Phases 1–2 to function (it could even be written ad-hoc first), but
doing 1–2 first keeps the shared code where it belongs.

## 6. Open questions (maintainer decision)

Decided 2026-08-29 (all yes), implementation on hold:

- **Q1 (yes):** `Remove` (VPOT-5) goes to PlugMode as well.
- **Q2 (yes):** PlugMode command target = the mode-level accessed plugin.
- **Q3 (yes):** the mode-switch slot (VPOT-3, "PlMode" in ChannelStripMode)
  also exists in the other modes as a generic "switch to mode X" slot.

- **Q3a (still open, design point for Phase 3+):** which target mode each
  mode's VPOT-3 switches to (e.g. PlugMode → ChannelStripMode? or a
  rotating/last-mode behaviour?). Decide before implementing that slot.
- **Q4:** Buttons as controls: the table's `control` index is positional
  today (VPOT 0..7). If button commands are ever wanted, the entry key
  becomes a small (EElement, position) pair — noted here so the interface
  does not need to be re-broken later; not needed now.
