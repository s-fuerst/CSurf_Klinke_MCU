# Extender Support WP-A — Critical Review

> Review date: 2026-07-08.
> Scope: `ai-docs/extender-support.md`,
> `ai-docs/extender-wp-a-hardware-unit.md`, and especially
> `ai-docs/extender-wp-a-impl-plan.md`, checked against the current source tree.

## Summary

The overall direction, a single logical `CSurf_MCU` owning multiple physical
hardware units, is sound. The implementation plan is too optimistic in two
places:

- It promises an internally N-generic WP-A while explicitly deferring several
  pieces that are required for even basic multi-unit routing.
- It deletes legacy extender scaffolding before all current call sites have
  replacement APIs.

WP-A should either be described honestly as an N=1 extraction only, or it must
pull a small but real slice of WP-F/WP-C work forward.

## Findings

### 1. WP-A is not actually N-generic

The plan says WP-A delivers a core structure where WP-B can later feed `N`
units without further core surgery. That does not match the code.

`CCSManager` still hard-limits channels to `0..8` via assertions and fixed
arrays:

- `src/core/CCSManager.h`: `CHECKMODEANDCHANNEL` asserts `channel < 9`.
- `src/core/CCSManager.h`: `m_stateRec[9]`, `m_stateSolo[9]`,
  `m_stateMute[9]`, `m_stateSelect[9]`, `m_faderPos[9]`,
  `m_faderTouched[9]`, `m_vpotTouched[9]`.
- `src/core/CCSManager.cpp`: button/fader/VPOT entry points assert
  `channel <= 8`.

If a second `HardwareUnit` emits global channel 9 or above, it will assert or
index outside the current fixed arrays. Therefore WP-A cannot both defer WP-F
and claim the core is ready for N-unit input.

Recommended correction: state that WP-A is an N=1-only structural extraction,
or move the minimal `CCSManager` channel-limit work into WP-A:

- dynamic touch arrays,
- dynamic VPOT array or explicit N=1 guard,
- `availableChannels()`-based assertions,
- safe routing stubs for channels above 8.

### 2. Step 8 would break compilation

The plan deletes `g_mcu_list`, `GetOffset()`, and `GetSize()` in Step 8. These
symbols are still used outside `CSurf_MCU`:

- `src/modes/multitrack/MultiTrackMode.cpp` uses `GetSize()` for bank movement.
- `src/modes/multitrack/MultiTrackMode.cpp` iterates `g_mcu_list` and calls
  `GetOffset()`.
- `src/core/Tracks.cpp` uses `m_pMCU->GetOffset()` in
  `Tracks::findMediaTrackForChannel()`.

Since the plan defers the `Tracks` refactor to WP-C, removing these APIs in
WP-A is premature.

Recommended correction: keep compatibility shims through WP-A:

- `GetOffset()` returns `0`.
- `GetSize()` returns `availableChannels()` or `8` for strict N=1 WP-A.
- replace `g_mcu_list.GetSize() * 8` with `availableChannels()` only where no
  external dependency remains.

Delete the old symbols only after `Tracks` and `MultiTrackMode` no longer
depend on them.

### 3. Per-unit `DisplayHandler` cannot use the current `CSurf_MCU::SendMsg`
shim

Step 3 proposes a per-unit `DisplayHandler`, while keeping
`DisplayHandler::sendToHardware()` routed through `CSurf_MCU::SendMsg()`.
The Step 2 shim sends to `m_units[0]`.

That means every per-unit display handler would still send display SysEx to
unit 0. This breaks the main reason for making `DisplayHandler` per-unit.

Recommended correction: give `DisplayHandler` a unit-local output path. Options:

- pass `HardwareUnit*` or a small `MidiSender` interface to `DisplayHandler`,
- let `DisplayHandler` call `HardwareUnit::sendMsg()` directly,
- or keep `DisplayHandler` in `CSurf_MCU` until the send path can carry unit
  context.

Do not rely on `CSurf_MCU::SendMsg()` for per-unit display output unless the
message itself carries enough routing context, which it currently does not.

### 4. `OnRotaryEncoderPush` is incorrectly classified as global/main-only

The plan leaves `OnRotaryEncoderPush` in `CSurf_MCU` with the global handlers.
That is incorrect for extender support.

The MCU note range `0x20..0x27` is the per-strip VPOT push range. The current
implementation maps it to channels `1..8`. Extender VPOT pushes must therefore
receive the unit offset just like rec/solo/mute/select and VPOT rotation.

Recommended correction: move `OnRotaryEncoderPush` into the per-unit strip
input path, or provide a unit-aware wrapper that emits global channels.

### 5. `ButtonManager` state collides across units

`ButtonManager` tracks button pressed, double-click, hold, and last-button
state by raw note ID only. With multiple units, the same note ID can be active
on more than one unit.

This affects at least:

- select long press,
- select/solo double click,
- modifier/global button state if multiple main-capable units are allowed.

The implementation plan marks ownership as an open integration point, but the
N-generic claim requires an answer.

Recommended correction: for WP-A N=1, keep `ButtonManager` as-is. For any N>1
enablement, make button state per unit or attach a unit index to dispatched
button events before they touch shared state.

### 6. `MultiDisplay` is under-specified

The plan correctly identifies that `DisplayHandler::sendDifferences()` only
sends for the active display. However, several direct single-handler paths
remain:

- `ActionsDisplay::switchTo()` calls `m_pDisplayHandler->switchTo(this)`.
- `CSurf_MCU::MCUReset()` switches directly to the splash display.
- several modes still call `getDisplayHandler()->switchTo(...)` directly.

The composite display approach must also avoid being treated as a real display
buffer. `Display::getText()` is not virtual, and the base constructor allocates
buffers based on `getRowLength()`. A `MultiDisplay` that inherits `Display` must
override every buffer-touching virtual method and must never be passed to a
handler as the active display.

Recommended correction:

- introduce an explicit display-switching service on `CSurf_MCU` or
  `CCSManager`,
- route splash and `ActionsDisplay` through that service,
- make `MultiDisplay` a narrow composite with child displays as the only active
  displays on real handlers,
- document that `DisplayHandler::switchTo(multiDisplay)` is invalid.

### 7. Per-unit ProX support is larger than the display handler change

The plan says the global `CONFIG_FLAG_PROX` display path becomes per-unit
`isProX()`. That is necessary but not sufficient.

Several modes decide their own row usage based on the global flag:

- `MultiTrackMode` writes rows 2/3 only under `CONFIG_FLAG_PROX`.
- `SendReceiveModeBase` chooses row 2 vs row 0 based on `CONFIG_FLAG_PROX`.
- `CommandMode`, `PlugMode`, `PanMode`, meter bridge code, and `VPOT_LED` also
  check the global flag.

Mixed Mackie/QCon configurations will not work just by making
`DisplayHandler` per-unit ProX-aware. The mode-facing display abstraction must
decide what a logical row means per child display, or mixed models must remain
out of scope until a later WP.

Recommended correction: explicitly state one of these policies:

- WP-A supports only one model globally, preserving current behavior.
- Mixed models are accepted but only rows supported by each unit are rendered.
- Full mixed-model display behavior is deferred and WP-B must prevent mixed
  configurations until then.

### 8. Meter and VPOT routing remain global-to-unit ambiguous

The plan defers `VPOT_LED[9]` sizing and meter bridge routing to WP-F, but WP-A
also claims global/local translation and per-unit hardware state.

Current behavior:

- `VPOT_LED` sends `0xB0, 0x2F + m_track` through `CSurf_MCU::SendMidi()`.
- `MeterBridge::sendToHardware()` sends `0xD0`/`0xD1` through
  `CSurf_MCU::SendMidi()`.

With more than one unit, these calls need unit context. For strict N=1 WP-A this
is fine. For an N-generic core, it is not.

Recommended correction: keep this explicitly N=1 in WP-A, or add routing
methods such as `sendVPOT(globalChannel, value)` and
`sendMeter(globalChannel, value)` before enabling more units.

### 9. The build/verify snippet has the wrong copy path

The plan says:

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)"
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

After `cd build`, the copy path resolves to
`build/build/reaper_csurf_mcu_klinke.so`.

Recommended correction:

```bash
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)")
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

or:

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(nproc)"
cp reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

## Suggested WP-A Boundary

A safer WP-A boundary:

1. Add `HardwareUnit` and move MIDI I/O/reset/raw send for N=1 only.
2. Keep `CSurf_MCU` shims stable.
3. Keep `GetOffset()`, `GetSize()`, and any needed legacy compatibility APIs
   until WP-C replaces their users.
4. Do not claim that WP-B can enable N>1 immediately.
5. Add explicit TODOs or disabled code paths for N>1 routing.

If WP-A should really prepare N>1 structurally, pull these minimum items into
WP-A:

1. dynamic `CCSManager` channel bounds,
2. unit-aware display send path,
3. unit-aware strip button dispatch, including VPOT push,
4. per-unit button state or a documented main-only/global-button policy,
5. routing APIs for VPOT LEDs, faders, meters, and strip LEDs.

## Bottom Line

The design direction is good. The plan needs stricter staging. The current
version mixes an N=1 refactor with N-generic promises, and it deletes old
scaffolding before the current code has replacement paths. Tightening that
boundary will make the work much safer to implement and review.
