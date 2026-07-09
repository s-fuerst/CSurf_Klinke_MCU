# WP-E - Global Display and Transport Routing - Implementation Plan

> Implementation plan for work package WP-E of the extender-support effort.
> Master plan: `ai-docs/extender-support.md` (sections 5-7). WP-D reference:
> `ai-docs/extender-wp-d-impl-plan.md`.
>
> **This is a plan, not code.** No source changes have been made.
>
> **Prepared 2026-07-09.** This plan assumes WP-B style `SurfaceConfig`
> with up to 8 configured `HardwareUnit`s and the WP-C/WP-D direction where
> channel-strip width is a runtime logical quantity. WP-E deliberately avoids
> widening strip/VPOT/meter routing; that is WP-F.

## Goal

Route surface-global hardware output by **unit capability** instead of by
position. Any configured unit whose `UnitConfig::isMain` capability is true is
transport-capable and should receive the same global state:

- transport LEDs;
- SMPTE/beats LEDs and 7-segment time display;
- assignment 7-segment display;
- automation-mode LEDs;
- VPOT assignment LEDs, flip/global-view LEDs, zoom/scrub LEDs;
- drop/save/undo/metronome/global-solo LEDs;
- startup/reset global state.

After WP-E, global routing no longer means "unit 0" and no longer depends on
the legacy `m_is_mcuex` flag. Position continues to define channel-strip order
only.

N=1 behavior must remain byte-identical except where the implementation
replaces a direct `m_midiout` send with an equivalent `HardwareUnit` send.

---

## Core Decisions

### Transport Capability

For WP-E, a transport-capable unit is:

```cpp
unit->isMain()
```

This matches the existing config model: main units use device id `0x14` and
own the transport/timecode hardware; extender units use device id `0x15` and
are strips-only for global-routing purposes.

If a malformed config produces no transport-capable units, fall back to unit
0 for output so a broken config does not make the surface completely silent.
This is a defensive fallback only; the dialog should continue to require at
least one main-capable unit.

### Global vs Strip Routing

Do **not** make `CSurf_MCU::SendMidi()` broadcast. Existing callers include
VPOT LEDs and meter bridge output, which are strip-local and belong to WP-F.

Instead, introduce explicit global-routing helpers and move only known global
call sites to those helpers.

`SetLED()` may become a note-classifying router because the MCU note ranges
are clear:

| Range | Meaning | WP-E route |
|---|---|---|
| `0x00..0x07` | record-arm strip buttons | keep unit 0 until WP-F |
| `0x08..0x0f` | solo strip buttons | keep unit 0 until WP-F |
| `0x10..0x17` | mute strip buttons | keep unit 0 until WP-F |
| `0x18..0x1f` | select strip buttons | keep unit 0 until WP-F |
| `0x20..0x27` | VPOT push strip buttons | keep unit 0 until WP-F |
| `0x28..0x7f` | assignment/global/transport/function/modifier LEDs | all transport-capable units |

This gives global LEDs correct multi-main behavior without pretending that
channels 9+ already route correctly.

### ProX Policy

Mixed Mackie/QCon ProX setups remain only partially supported until per-mode
display and WP-F routing work removes the remaining global `CONFIG_FLAG_PROX`
dependencies. WP-E handles only the global quirks that are already local to
global routing:

- assignment 7-segment display is suppressed per ProX transport unit;
- LED blink emulation runs per `HardwareUnit`;
- `HardwareUnit::setLED()` continues to apply the per-unit ProX
  `LED_BLINK_BYPASSED` behavior.

Mode-level ProX row choices, VPOT ring encoding, and meter bridge differences
remain out of scope.

---

## In Scope

- Add capability-based helpers on `CSurf_MCU` for transport-unit iteration and
  global MIDI/LED sends.
- Replace direct `m_midiout->Send()` global call sites with those helpers.
- Route global LED notes (`0x28+`) to every transport-capable unit.
- Keep strip LED notes (`0x00..0x27`) on the current unit-0 path until WP-F.
- Replace `!m_is_mcuex` output gates with transport-unit iteration.
- Make assignment display suppression per transport unit's `isProX()`.
- Run blinking LED emulation over every constructed unit.
- Keep startup/reset global state coherent across all transport-capable units.
- Optionally allow global input events from secondary transport-capable units
  while continuing to drop strip input from secondary units.

## Out of Scope

- Channel 9+ strip output, including faders, rec/solo/mute/select LEDs, and
  VPOT push LEDs.
- VPOT LED ring routing (`VPOT_LED::updateLEDs()`).
- Meter bridge routing (`MeterBridge` and mode-specific meter bridges).
- `CCSManager` array widening and per-unit button state. That is WP-F.
- Full `MultiDisplay` field routing for mode LCDs.
- Per-mode decisions about whether extra units extend, mirror, or specialize
  a mode display.
- Dynamic "release extenders" activation.
- Removing legacy members such as `m_midiout`, `m_midiin`, or `m_is_mcuex`
  entirely. WP-E may reduce their use, but removal is a later cleanup.

---

## Current Code Hotspots

| Area | Current issue | File |
|---|---|---|
| Raw global sends | many global LEDs still call `m_midiout->Send()` directly | `src/core/csurf_mcu.cpp` |
| Generic send shim | `SendMidi()` sends only to unit 0 but is also used by strip VPOT/meter code | `src/core/csurf_mcu.cpp`, `src/hardware/VPOT_LED.cpp`, `src/hardware/MeterBridge.cpp` |
| LED router | `SetLED()` routes every note to unit 0 | `src/core/csurf_mcu.cpp:SetLED()` |
| Time display | SMPTE/beats LEDs and 7-seg digits are guarded by `m_midiout && !m_is_mcuex` | `src/core/csurf_mcu.cpp:Run()` |
| Transport state | play/pause/record/repeat output goes only to `m_midiout` | `src/core/csurf_mcu.cpp:SetPlayState()`, `SetRepeatState()` |
| Automation state | automation LEDs go only to `m_midiout` | `src/core/csurf_mcu.cpp:UpdateAutoModes()` |
| Assignment display | still checks global `CONFIG_FLAG_PROX` and sends via `SendMidi()` | `src/core/CCSManager.cpp:setAssignmentDisplay()` |
| Drop LED | `DropState` accepts `midi_Output*` and legacy extender flag | `src/core/csurf_mcu.h:DropState` |
| Save/undo/zoom/scrub | short-lived LED feedback goes only to unit 0 | `src/core/csurf_mcu.cpp` |
| Blink emulation | only unit 0 blinks | `src/core/csurf_mcu.cpp:EmulateBlinkingLEDs()` |
| Startup/reset | `MCUReset()` still performs global output through unit 0 | `src/core/csurf_mcu.cpp:MCUReset()` |
| Secondary input gate | unit 2+ input is dropped wholesale | `src/core/csurf_mcu.cpp:Run()` |

---

## Proposed `CSurf_MCU` Helpers

Add helpers that make the routing intent visible at every call site:

```cpp
bool hasTransportUnits() const;
HardwareUnit *firstTransportUnit() const;
bool isTransportUnit(const HardwareUnit *unit) const;

void sendMidiToTransportUnits(unsigned char status, unsigned char d1,
                              unsigned char d2, int frameOffset);
void setLEDOnTransportUnits(int button, int state);
void sendAssignmentDisplayToTransportUnits(const char text[2]);
bool anyUnitNeedsBlinkEmulation() const;
```

Keep these helpers in `CSurf_MCU` rather than `HardwareUnit`: they express a
surface-level policy over a collection of units.

For per-unit filtering, use the unit object:

```cpp
if (!unit->isProX())
  unit->sendMidi(0xB0, 0x40 + 11, text[0], -1);
```

Do not introduce a helper named simply `broadcastMidi()`. The codebase has
both global and strip-local MIDI sends; helper names must say which surface
domain they affect.

---

## Steps

### Step 1 - Add transport-unit iteration helpers

**Goal:** centralize the capability rule and defensive fallback.

- **Files:** `src/core/csurf_mcu.h`, `src/core/csurf_mcu.cpp`.
- Add `isTransportUnit()`, `hasTransportUnits()`, and `firstTransportUnit()`.
- Add `sendMidiToTransportUnits()` and `setLEDOnTransportUnits()`.
- Iteration order should be `m_units` order so repeated global state reaches
  hardware in the same physical left-to-right order as configuration.
- If no unit has `isMain()`, route to `m_units[0]` if present and log a debug
  warning once.
- Keep `SendMidi()` unchanged as a unit-0 compatibility shim.
- Keep `SendMsg()` unchanged unless a specific global SysEx caller needs a
  new explicit helper.

### Step 2 - Make `SetLED()` classify global vs strip notes

**Goal:** global LED state reaches every transport-capable unit without
changing strip behavior.

- **File:** `src/core/csurf_mcu.cpp`.
- Add small local predicates or private helpers:
  - `isStripLedNote(int note)` for `0x00..0x27`;
  - `isGlobalLedNote(int note)` for `0x28..0x7f`.
- Route global notes through `setLEDOnTransportUnits()`.
- Route strip notes through the current unit-0 path until WP-F.
- Preserve per-unit LED dedup and ProX blink behavior in
  `HardwareUnit::setLED()`.
- Verify that these callers now broadcast automatically:
  - `CCSManager::updateVPOTLeds()`;
  - `CCSManager::updateFlipLED()`;
  - `CCSManager::updateGlobalViewLED()`;
  - `Transport::updateLeds()`;
  - `UpdateGlobalSoloLED()`;
  - `UpdateMetronomLED()`.

### Step 3 - Replace direct global LED sends in `CSurf_MCU`

**Goal:** remove the remaining unit-0 shortcuts for global LEDs.

- **File:** `src/core/csurf_mcu.cpp`.
- Replace direct `m_midiout->Send()` calls with `SetLED()` or
  `sendMidiToTransportUnits()` as appropriate in:
  - `MCUReset()` zoom/scrub state;
  - `ClearSaveLed()` and `OnSave()`;
  - `ClearUndoLed()` and `OnUndo()`;
  - `OnZoom()`;
  - `OnScrub()`;
  - `SetPlayState()`;
  - `SetRepeatState()`;
  - `UpdateAutoModes()`;
  - the SMPTE/beats mode LEDs in `Run()`.
- Prefer `SetLED(button, state)` when the message is a note LED (`0x90`).
- Use `sendMidiToTransportUnits()` for 7-seg control-change output.
- Delete only the local `!m_is_mcuex` output gates that become redundant.
  Do not remove `m_is_mcuex` itself in WP-E.

### Step 4 - Refactor `DropState` output

**Goal:** make drop-state LED routing use the same global LED path as the
rest of the surface.

- **Files:** `src/core/csurf_mcu.h`, `src/core/csurf_mcu.cpp`.
- Change `DropState` so it no longer needs `bool is_mcuex` or
  `midi_Output*` for MCU output.
- Recommended shape:
  - keep `toggleState()` and `updateReaper()`;
  - add `int ledState() const`;
  - let `CSurf_MCU` call `SetLED(B_DROP, m_dropstate.ledState())`.
- Update `MCUReset()` and `OnDropButton()` accordingly.
- N=1 checkpoint: the three drop states still produce off/on/blink exactly as
  before.

### Step 5 - Route assignment display per transport unit

**Goal:** assignment digits are global, but ProX suppression is per unit.

- **Files:** `src/core/csurf_mcu.h`, `src/core/csurf_mcu.cpp`,
  `src/core/CCSManager.cpp`.
- Add `CSurf_MCU::sendAssignmentDisplayToTransportUnits(const char text[2])`.
- In that helper, for each transport-capable unit:
  - skip the unit if `unit->isProX()`;
  - send `0xB0, 0x40 + 11, text[0]`;
  - send `0xB0, 0x40 + 10, text[1]`.
- Replace `CCSManager::setAssignmentDisplay()`'s global
  `CONFIG_FLAG_PROX` check and `SendMidi()` calls with this helper.
- Keep `CCSManager`'s `m_stateAssignmentDisplay` as a single surface-level
  cache. The same two-character assignment is intended on every non-ProX
  transport unit.

### Step 6 - Route the time display to every transport-capable unit

**Goal:** every main-capable unit shows identical SMPTE/beats state.

- **File:** `src/core/csurf_mcu.cpp`.
- Keep the current time formatting logic in `Run()` for the first pass; only
  change the output path.
- Replace:
  - SMPTE/beats mode LED direct sends with `SetLED(0x71, ...)` and
    `SetLED(0x72, ...)`;
  - 7-seg digit direct sends with `sendMidiToTransportUnits(0xB0, 0x40 + x,
    bla[idx], -1)`.
- The existing `m_mackie_lasttime` cache can remain surface-level because all
  transport units receive the same digits. If later hot-plug or dynamic unit
  activation is added, that future flow must force a resend.
- Keep the existing two-second forced resend.

### Step 7 - Blink emulation across units

**Goal:** per-unit LED state can blink on every configured hardware unit.

- **Files:** `src/core/csurf_mcu.h`, `src/core/csurf_mcu.cpp`.
- Change `EmulateBlinkingLEDs(now)` to iterate all `m_units`.
- Replace the `Run()` condition:

  ```cpp
  if (IsFlagSet(CONFIG_FLAG_PROX) || IsFlagSet(CONFIG_FLAG_EMULATING_BLINKING))
  ```

  with a helper based on current units:

  ```cpp
  if (anyUnitNeedsBlinkEmulation())
  ```

- `anyUnitNeedsBlinkEmulation()` should return true when:
  - `CONFIG_FLAG_EMULATING_BLINKING` is set; or
  - any constructed unit `isProX()`.
- This is safe before WP-F because units with no blinking state simply have
  nothing to emit.

### Step 8 - Startup, reset, and shutdown global state

**Goal:** lifecycle paths leave all transport-capable units in a coherent
global state.

- **Files:** `src/core/csurf_mcu.cpp`.
- In `MCUReset()`:
  - keep the surface-level state resets exactly once;
  - route drop/flip/global-view/zoom/scrub/assignment output through the new
    helpers;
  - keep splash LCD behavior unit-0-only unless Step 9 is included.
- Constructor reset order can stay:
  1. construct units;
  2. `MCUReset()`;
  3. start inputs;
  4. `forceAllLEDsOff()` on every unit;
  5. `Transport::updateLeds()`.
- Consider whether `forceAllLEDsOff()` after `MCUReset()` is currently
  clearing freshly set global LEDs. If it is, do not fix it with more
  broadcasts; instead document or adjust the order deliberately.
- In the destructor, keep strip fader-bottom and meter-off behavior unit-0
  until WP-F. Global LED-off can use `forceAllLEDsOff()` on every unit.

### Step 9 - Decide the minimal LCD behavior for global displays

**Goal:** avoid accidentally making `MultiDisplay` a WP-E dependency.

WP-E does not need full mode LCD routing. For this milestone, choose one of
these two options and document it in the implementation notes:

1. **Minimal option:** keep splash/action/mode LCDs on the existing
   `getDisplayHandler()` path. This preserves N=1 behavior and leaves
   composite LCD routing to the per-mode/WP-F display work.
2. **Global mirror option:** create explicit global display helpers for
   startup/splash/goodbye only, and mirror those whole-line messages to every
   constructed unit's own `DisplayHandler`.

Do **not** switch mode displays through `DisplayHandler::switchTo()` with a
`MultiDisplay`. `MultiDisplay::switchToAll()` exists because every child must
be active on its own handler.

Recommendation: choose the minimal option for WP-E. The real user-visible
multi-unit LCD work depends on channel 9+ mode rendering and should stay with
WP-F/per-mode work.

### Step 10 - Secondary transport-unit input gate

**Goal:** decide whether a second main-capable unit's global buttons should be
accepted before WP-F.

The current WP-B gate drops all input from unit index `> 0`. WP-E can either:

- leave that gate unchanged and be an output-routing milestone only; or
- allow global input events from secondary transport-capable units while still
  dropping strip-local events.

Recommended conservative implementation: allow only clearly global events
from secondary transport-capable units:

- note events `0x90` / normalized `0x80` with note `>= 0x28`;
- jog wheel control change `0xB0 0x3c`;
- pedal messages already handled by `OnPedalMove()` if the unit is
  transport-capable.

Continue to drop:

- faders `0xE0..0xE8`;
- VPOT moves `0xB0 0x10..0x17`;
- strip notes `0x00..0x27`;
- any ambiguous message.

This lets transport/global controls on an additional main-capable unit work
without widening `CCSManager` channel state. The remaining caveat is that
`ButtonManager`'s double-click/hold arrays are still surface-level and keyed
by raw note. That is acceptable for global controls in WP-E but must be
revisited in WP-F.

If this input split feels too risky during implementation, keep the wholesale
gate and record secondary global input as deferred to WP-F. Do not partially
allow strip events.

---

## Verification

**Build:**

```bash
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)")
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

Fully restart REAPER after copying the plugin.

**N=1 regression:**

- Startup splash appears as before.
- Play/pause/record/repeat LEDs behave as before.
- Time display updates in every time mode.
- SMPTE and beats LEDs track the current time mode.
- Assignment display updates in MultiTrack, Send, Receive, Command, and Plug
  modes.
- Save and undo LEDs flash and clear on schedule.
- Zoom and scrub LEDs toggle correctly.
- Drop cycles off/on/blink exactly as before.
- Metronome and global solo LEDs still follow REAPER state.
- VPOT rings and meter bridge output are unchanged.

**N=2 output checks with one main + one extender:**

- Main unit receives all global LEDs/time display.
- Extender does not receive global transport/timecode output.
- Extender still receives reset and force-all-LEDs-off lifecycle output.
- Strip LED/fader/VPOT/meter behavior remains limited to unit 0 until WP-F.
- No assertions fire from `CCSManager`.

**N=2 output checks with two transport-capable units:**

- Both main-capable units receive play/pause/record/repeat LEDs.
- Both receive SMPTE/beats LEDs and 7-seg time digits.
- Both receive automation, flip/global-view, zoom/scrub, save/undo, drop,
  metronome, and global-solo LEDs.
- A ProX main unit does not receive assignment digits; a Mackie main unit in
  the same config does.
- Blink emulation works independently per unit.

**Optional secondary input checks if Step 10 is implemented:**

- Transport buttons on a secondary main-capable unit control REAPER.
- SMPTE/beats, automation, function, modifier, jog, marker/nudge, save/undo,
  and global-view buttons are accepted from the secondary main-capable unit.
- Faders, VPOT moves, and strip buttons from unit 2+ are still dropped until
  WP-F.
- Pressing the same global button on two units at the same time may share
  double-click/hold state; document this as a WP-F limitation if observed.

---

## Exit Criteria

- Direct global `m_midiout->Send()` call sites in `CSurf_MCU` are replaced by
  capability-based routing helpers.
- `CSurf_MCU::SendMidi()` remains a unit-0 compatibility shim and is not used
  for new global broadcasts.
- `SetLED()` broadcasts note LEDs `0x28+` to every transport-capable unit.
- `SetLED()` keeps note LEDs `0x00..0x27` on the existing unit-0 route until
  WP-F.
- Transport, repeat, automation, SMPTE/beats, time digits, save/undo,
  zoom/scrub, drop, metronome, global-solo, assignment, flip, and global-view
  outputs no longer depend on `!m_is_mcuex`.
- Assignment-display ProX suppression is per transport unit, not global
  `CONFIG_FLAG_PROX`.
- Blink emulation iterates all constructed units when any unit requires it or
  emulated blinking is enabled.
- N=1 behavior is unchanged.
- N>1 global output behavior is capability-based, independent of unit
  position.
- No channel 9+ strip/VPOT/meter routing is introduced by WP-E.

---

## Review Notes for Implementation

- Search for every remaining `m_midiout->Send()` after implementation. Any
  remaining call should be either strip-local, lifecycle-only, or explicitly
  documented.
- Search for every `SendMidi()` caller. VPOT and meter callers should remain
  unchanged until WP-F; global callers should move to explicit global helpers.
- Search for `CONFIG_FLAG_PROX`. WP-E should remove only the assignment/blink
  global-routing dependency. Mode display, VPOT, and meter dependencies are
  not WP-E work.
- Keep helper names precise. The main future maintenance risk is hiding
  strip-local MIDI behind a broadcast-looking API.
