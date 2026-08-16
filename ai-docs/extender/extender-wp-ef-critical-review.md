# Extender Support WP-EF — Critical Review

> Review date: 2026-07-12.
> Scope: `ai-docs/extender-wp-ef-impl-plan.md`, checked against the current
> `CSurf_MCU`, `HardwareUnit`, `CCSManager`, `ButtonManager`, `MeterBridge`,
> display, configuration, and mode source files.

## Summary

WP-EF has the right high-level destination: one logical surface, explicit
global versus strip routing, and per-unit ProX behaviour. Combining the former
WP-E and WP-F is also sensible because routing cannot be validated properly
while strip output is still pinned to unit 0.

The detailed plan is **not ready to implement as written**. Five issues are
hard blockers:

1. the configured-unit topology is not defined for disabled rows and currently
   has no safe global-channel mapping invariant;
2. the plan both enables channels `9..N*8` and explicitly defers the mode work
   required to make those channels safe;
3. the proposed MeterBridge index conversion collides strip meter state with
   master meter state and can index beyond its fixed array;
4. input routing assumes a `HardwareEventListener` path that the current code
   does not use, and the proposed fader decoding reads the wrong MIDI byte;
5. deleting `CONFIG_FLAG_PROX` cannot compile while its documented deferred
   mode-level users remain.

The plan should be revised before coding. In particular, it needs a small
topology/invariant step before routing, a firm decision about whether mode
widening belongs to this work package, and a separate meter-domain API.

## Findings

### F1 — Unit topology and channel ownership are undefined for disabled rows (hard blocker)

The configuration has eight fixed rows and the dialog permits a disabled row
at any position. The constructor, however, only appends units with real MIDI
ports (except for row 0, which is always constructed):

```cpp
for (int i = 0; i < MAX_SURFACE_UNITS; i++) {
  bool hasDevice = (cfg.units[i].midiInDev != -1 || cfg.units[i].midiOutDev != -1);
  bool isUnit1 = (i == 0);
  if (!hasDevice && !isUnit1)
    continue;
  m_units.push_back(new HardwareUnit(i, cfg.units[i], this, errStats));
}
```

`unitForChannel()` then indexes that compact vector with `(globalChannel - 1) /
8`. A configuration with Unit 1 enabled, Unit 2 disabled, and Unit 3 enabled
therefore has two conflicting interpretations:

- configuration position says Unit 3 owns channels 17–24;
- compact-vector routing says its second vector entry owns channels 9–16;
- `HardwareUnit::unitIndex()` remains `2`, so any future input code using the
  unit's index would again emit channels 17–24.

The same configuration can have no transport-capable unit, or a disabled row 0
while a later row is main-capable. Current display ownership still selects
`m_units[0]`, so that case also has no usable main display/output path.

The plan's original fallback of sending global output to `m_units[0]` when no main unit
exists was deemed unsafe at review time. **Decision (2026-07-12):** configs with
zero main units are explicitly allowed. When there are no main units, global
output has no target and the transport-unit helpers are no-ops.

All routing helpers should return `NULL`/failure for an invalid channel. There
should be no output fallback for “no transport unit”; only malformed topology
belongs at the configuration-validation boundary.

### F2 — The scope boundary conflicts with enabled N>1 input and output (hard blocker)

Steps 7–11 widen `CCSManager` state, meter loops, fader routing, button
handling, and input dispatch to `availableChannels()`. The exit criteria then
require input events from every unit to carry a global channel.

The plan's scope boundary and its N=2 test state the opposite: modes remain at
channels 1–8 and channels 9+ are intentionally not enabled until a later
per-mode milestone. Both statements cannot be true at runtime.

The current code contains hard gates well beyond the arrays explicitly listed
in Step 7:

- `CCSManager` asserts channels 1–8 in every button, fader, and VPOT entry
  point;
- `CCSMode` default update loops stop at 8;
- MultiTrack, Send/Receive, Command, Plug, and Pan modes have fixed loops,
  local state, and assumptions based on eight strips;
- `MultiTrackMode::trackVolume()` and `trackPan()` explicitly discard IDs
  above 8.

After Step 11 maps an extender input to channel 9, it will reach code that is
either still asserting, still ignores the channel, or has not had its
mode-specific semantics designed. Merely making the `CCSManager` arrays
dynamic does not make the modes safe.

**Required decision:**

1. If WP-EF is genuinely the first usable N>1 hardware-routing milestone,
   include the necessary per-mode widening and mode-specific design in its
   scope. This is larger than the stated work package.
2. If per-mode work remains deferred, retain the input gate for unit 1+ and do
   not claim working strip faders, buttons, VPOTs, or meters for channels 9+.
   WP-EF can still complete and test global output plus routing infrastructure.

The current mixed approach is unsafe and makes the N=2 acceptance test
internally contradictory.

### F3 — MeterBridge needs a new data model, not only a new route (hard blocker)

The current meter state is a fixed `m_mcu_meterpos[10]`: indexes 0–7 represent
strip meters and 8–9 represent the two master meter values. Step 8 changes
strip positions so index 8 represents global strip channel 9. That directly
collides with the old first master position.

The proposed `sendToHardware()` implementation also contains a logic error:

```cpp
int localPos = globalPos % 8;
u->sendMidi(localPos < 8 ? 0xD0 : 0xD1, ...);
```

`globalPos % 8` is always 0–7, therefore this implementation can never select
`0xD1`. It cannot represent a master meter at all. Widening a mode loop without
resizing the meter-state store would additionally read and write past the
10-element array.

**Required revision:** split the two domains in the API and in stored state.

- `updateStripMeter(globalChannel, ...)` owns a dynamically sized strip-state
  vector of `availableChannels()` values and maps that channel to one unit plus
  a local strip position 0–7. It always sends `0xD0`.
- `updateMasterMeters(...)` owns two independent master-state values and sends
  them through a dedicated per-unit helper to ProX-capable units using `0xD1`.
- Do not overload one integer `pos` to mean both a global strip position and a
  master position.

This work also depends on the F2 scope decision: each mode's source and meter
meaning for channels 9+ is mode design, not a transport-only change.

### F4 — The documented input path does not exist and the proposed mapping is incomplete (hard blocker)

The plan states that WP-A already emits strip events through
`HardwareEventListener`. In the current tree, `CSurf_MCU` does not implement
that interface, `HardwareUnit::setListener()` is never called, and
`HardwareUnit` does not parse input events. Its `onMCUReset()` method is still
a stub.

Actual input is read in `CSurf_MCU::Run()`, passed as a raw `MIDI_event_t` to
`OnMIDIEvent()`, and then dispatched through `ButtonManager`. Unit 1+ input is
currently dropped deliberately.

The Step 11 fader example uses:

```cpp
int localCh = evt->midi_message[1] - 0xE0;
```

This is wrong for the protocol and for the present implementation. Fader
identity is in the MIDI status byte, `evt->midi_message[0]`, where `0xE0..0xE8`
are the fader channels. `midi_message[1]` and `[2]` are the 14-bit fader
value.

An input-context design must cover all strip-local paths, not only faders:

- fader move and fader touch (including the per-unit master slot);
- VPOT rotate and VPOT push;
- rec, solo, mute, and select press/double-click;
- select long-press, which is generated later by `ButtonManager::frame()` and
  no longer has the original event available;
- per-unit reset/handshake SysEx.

The `m_currentInputUnit` alternative can work only if it is guarded with an
RAII-style scoped reset and every strip handler applies the same translation.
Passing an explicit `unitIndex`/`globalChannel` through the strip dispatch
path is less implicit and easier to test. Whichever approach is selected, the
synthetic `Actions.cpp` calls to `dispatchMidiEvent()` need an explicit
synthetic/default source so their fader-touch actions keep their N=1 meaning.

### F5 — Removing `CONFIG_FLAG_PROX` contradicts the deferred scope (hard blocker)

Step 13 deletes `#define CONFIG_FLAG_PROX 16` while the plan explicitly leaves
mode-level `CONFIG_FLAG_PROX` checks out of scope. Those modes include the
header that defines the macro, so deletion causes a compile failure.

`SurfaceConfig` also still derives the compatibility flag from unit 0's model
and the configuration dialog writes it. That cannot be removed independently
without deciding how legacy mode code receives its global ProX assumption.

**Required revision:** retain the legacy compatibility flag until the final
mode-level migration. WP-EF may remove all *routing-layer* uses in
`csurf_mcu`, `CCSManager`, `VPOT_LED`, and meter code, but it must not delete
the define or its parser/dialog compatibility handling yet. The final
verification should instead require zero routing-layer call sites, not zero
mentions under `src/core/` where the flag definition and configuration code
live.

### F6 — `SetLED()` cannot be the central strip router with its current contract

The note number identifies a strip's **local** channel, not its physical unit.
For example, note `0x00` means record-arm strip 1 on every unit. Therefore
`SetLED(button, state)` has insufficient information to route a strip LED for
global channel 9.

The plan recognizes this later by adding `setStripLED(globalChannel, note,
state)`, but still describes `SetLED()` as the central router and leaves a
unit-0 fallback for direct strip callers. That fallback makes missed migrations
look superficially functional while lighting the wrong physical strip.

**Recommended API contract:**

- `setGlobalLED(note, state)` or the existing `SetLED()` is for global notes
  only and broadcasts to transport-capable units;
- `setStripLED(globalChannel, localNote, state)` is mandatory for every strip
  note, validates the channel, and has no fallback;
- assertions or a debug error expose any direct strip use of `SetLED()` during
  migration.

This also makes the desired two-axis routing model visible in every call site.

### F7 — Per-unit ButtonManager state must include last-button state and a global-input policy

Step 10 moves the four note-indexed arrays into `UnitButtonState`, but
double-click detection also depends on `m_button_last` and
`m_button_last_time`. Leaving those surface-level still lets a press on unit 0
affect the double-click classification of the same local note on unit 1.

`ButtonManager::frame()` must also iterate unit-local pressed/hold state for
strip notes and translate the long-press channel with that unit's strip base.

For global buttons, “surface-level state” needs a policy, not merely shared
arrays. If two main-capable units can issue global input, press on A, press on
B, then release A leaves a single shared Boolean false while B remains held.
That breaks hold/release semantics. Choose one of:

- accept global input only from a designated primary transport unit;
- maintain per-unit global state and aggregate it (for example, pressed if any
  unit holds the note);
- explicitly defer all secondary-main global input and discard it before
  `ButtonManager`.

The last option matches the present scope boundary most closely. “Accepted but
shared” is not a safe documented limitation.

### F8 — Reset/lifecycle code needs cache semantics and a compilable ownership path

The Step 12 sketch calls `Transport::instance()->updateLeds()`. No such
singleton exists; the current surface owns `m_pTransport` and calls
`m_pTransport->updateLeds()` after construction.

More importantly, `HardwareUnit::sendStripFader()` and `setMasterFader()`
deduplicate against cached values. After a device reset, emitting a reset SysEx
followed by `sendStripFader(local, 0)` may send nothing if the cached value is
already zero, even though the hardware state was just reset.

The lifecycle plan needs an explicit cache rule:

- invalidate all fader and LED caches immediately after unit reset; or
- provide forced fader sends used by reset/shutdown paths.

Do not use the generic raw-send helper for this just to bypass the cache; that
would leave the cache stale for subsequent regular updates. The required order
of reset, cache invalidation, fader reset, LED reset, input start, and initial
state publication should be documented and tested for N=1.

### F9 — Display claims do not match the current implementation or the declared scope

The plan correctly defers MultiDisplay field routing, but the scope text says
that `createDisplay()` returns a `MultiDisplay`. It currently returns a plain
`Display` bound to `getDisplayHandler()`, which always selects `m_units[0]`.
The splash display, action display, per-frame display resend, and mode display
switches all use that same unit-0 handler.

Consequently, WP-EF cannot claim that per-strip displays, startup splash, or
all display lifecycle paths are coherent on every configured unit. That is
acceptable if display routing remains out of scope, but the Step 12 checkpoint
and verification wording should say so explicitly. The N=2 tests should only
assert the output domains actually migrated in WP-EF.

### F10 — “N=1 byte-identical” is not verified and is likely false for LED traffic

Several direct `m_midiout->Send()` calls, such as `SetPlayState()` and
`UpdateAutoModes()`, currently send on every invocation. Replacing them with
`SetLED()` reaches `HardwareUnit::setLED()`, which deduplicates by note and
therefore omits repeated same-state messages.

That is usually an improvement and preserves observable hardware state, but it
is not byte-identical MIDI output. The manual N=1 checklist cannot prove the
stronger statement either.

The plan should choose one precise promise:

- preserve **functional/protocol-state equivalence** for N=1, allowing fewer
  redundant MIDI messages; or
- preserve byte-for-byte event traces, requiring migration helpers that retain
  the former send/dedup behaviour and automated captured-MIDI trace tests.

The first promise is more realistic, but it should be stated honestly.

### F11 — Verification lacks deterministic routing tests

The manual hardware checklist is useful but cannot catch most of the risks
above. It also says strip channels 9+ remain limited to unit 0 while Step 11
requires processing input from every unit, which is precisely the F2 conflict.

Before hardware testing, add a small capture/fake MIDI-output seam around
`HardwareUnit` or the routing helpers. It should verify at least:

- channels 1, 8, 9, and `N*8`, plus invalid channels 0 and `N*8+1`;
- one main plus one extender, two mains, and mixed Mackie/ProX mains;
- disabled/gapped configuration rows according to the F1 topology rule;
- local VPOT CC and strip LED numbers after global-to-local translation;
- independent strip double-click and long-press state on two units;
- master-fader and ProX master-meter broadcast rules;
- N=1 output traces or state equivalence, according to the F10 decision.

The build section should also name the supported build matrix, not only Linux,
if “zero compiler warnings on all platforms” remains an exit criterion.

## Recommended prerequisite and ordering

Add a short **WP-EF-0 — topology and scope contract** before the current Step
1. It should define:

1. enabled-unit density/gap handling and the required transport-unit
   configuration;
2. global-channel to physical-unit mapping and invalid-channel behaviour;
3. whether channels 9+ are usable in this package or remain gated pending
   per-mode work;
4. the legacy ProX-flag compatibility policy;
5. a deterministic routing-test seam.

After that decision, a safer sequence is:

| Order | Work |
|---|---|
| 1 | WP-EF-0 topology, compatibility, and test seam |
| 2 | Capability queries and explicit global/strip routing helpers |
| 3 | Global output migration, assignment display, DropState, blink policy |
| 4 | Strip LED, fader, and VPOT output migration within the decided channel scope |
| 5 | MeterBridge data-model replacement and per-unit master-meter routing |
| 6 | Input dispatch and ButtonManager state, only if channels 9+ are enabled |
| 7 | Lifecycle/cache invalidation and legacy-shim removal |
| 8 | Automated routing tests, then manual hardware verification |

## Exit criteria — required amendments

The revised plan should add these criteria before claiming completion:

1. A configuration with disabled rows has defined, tested behaviour; no output
   helper indexes `m_units` without validating its argument.
2. Strip meter state is dynamically sized and cannot collide with master meter
   state.
3. Every strip-local input handler has an explicit, tested unit-to-global
   channel translation, including touch and long-press paths.
4. `CONFIG_FLAG_PROX` either remains as a documented legacy compatibility
   flag, or every remaining user has been migrated in the same change.
5. The chosen N=1 compatibility definition has an automated test.
6. The scope makes one unambiguous promise about channels 9+; acceptance tests
   do not claim both enabled input and deferred mode handling.
7. Reset invalidates or force-updates per-unit fader caches before publishing
   initial state.

## Bottom line

WP-EF should not start as a direct implementation of the current 14 steps.
The routing model itself is sound, but the plan currently crosses unresolved
boundaries between unit topology, mode semantics, meter storage, and input
dispatch. Address F1–F5 first, then decide the F2 scope explicitly. With those
changes, the remaining findings are manageable implementation and verification
work rather than hidden N>1 correctness failures.
