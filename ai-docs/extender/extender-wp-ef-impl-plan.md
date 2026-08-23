# WP-EF — Routing Correctness + Global Multi-Main Output

> Implementation plan for work package WP-EF of the extender-support effort.
> Master plan: `ai-docs/extender-support.md` (sections 5-7).
>
> **Revised 2026-07-12** after critical review (`extender-wp-ef-critical-review.md`).
> The first draft claimed to enable channels 9+; that was self-contradictory
> (modes are not widened) and contained concrete code bugs (F3/F4/F8). This
> revision narrows the scope honestly, fixes the bugs, and adds a topology
> contract (WP-EF-0).
>
> **This is a plan, not code.** No source changes have been made beyond the
> Step 1b dialog fixes already applied (unit-0-not-disabled, stale comment).
>
> Assumes WP-A (HardwareUnit), WP-B (SurfaceConfig/KLINKE2), WP-C (Tracks
> N*8), WP-D (banking semantics) are complete.

## What this work package delivers (honest scope)

**Two real things:**

1. **Routing correctness.** Output no longer hardcodes unit 0 or gates on
   `m_is_mcuex`. It routes by capability: global output → every transport-
   capable unit; strip output → the owning unit by global channel. This is
   the prerequisite for ALL future N>1 work.

2. **Global multi-main output.** A config with two main-capable units (e.g.
   a Mackie Main plus a QCon ProX main, or a main sitting at a non-zero
   position) shows transport LEDs, SMPTE/beats, time digits, assignment,
   automation, flip/global-view, zoom/scrub, drop, save/undo, and metronome
   on **every** main unit. This is real, testable, user-visible value.

**What it does NOT deliver (deferred to per-mode design, §7 of master plan):**

- Working channel strips 9..N*8. Modes still iterate channels 1-8. The strip
  **routing plumbing** is made correct and ready, but modes do not emit to
  channels 9+ yet.
- VPOT_LED array resize, MeterBridge strip widening, ButtonManager per-unit
  state, input gate removal.
- Mode-level `CONFIG_FLAG_PROX` checks.
- Full legacy shim removal (`m_midiout`, `m_is_mcuex` retained as shims).

**Why keep WP-E and WP-F merged if strip widening is deferred?** The two
share the same `SetLED`/`SendMidi` call sites, the same ProX-quirk sites, and
the same topology prerequisite. Migrating the routing layer once (capability-
based helpers, per-unit ProX) is cleaner than two passes. The merge delivers
"routing correctness + global multi-main output"; the per-mode WPs later
deliver "channels 9+".

N=1 behavior is preserved at the **protocol-state level**: same observable
hardware state. Not byte-identical MIDI traffic (dedup changes the wire
format — see F10).

---

## WP-EF-0 — Topology and Scope Contract

**Do this FIRST.** It defines invariants the routing steps depend on.

### 0a. Dense-unit topology invariant

**Problem (F1):** `unitForChannel(g)` returns `m_units[(g-1)/8]` — it indexes
the **compact** constructed vector. The constructor skips disabled units:

```cpp
// csurf_mcu.cpp ctor
for (int i = 0; i < MAX_SURFACE_UNITS; i++) {
  bool hasDevice = (cfg.units[i].midiInDev != -1 || cfg.units[i].midiOutDev != -1);
  bool isUnit1 = (i == 0);
  if (!hasDevice && !isUnit1)
    continue;                       // <-- compacts the vector
  m_units.push_back(new HardwareUnit(i, cfg.units[i], this, errStats));
}
```

A config with Unit 0 enabled, Unit 1 disabled, Unit 2 enabled produces
`m_units = [unit0, unit2]`. Then `unitForChannel(9)` returns `m_units[1]` =
unit2, but `unit2->unitIndex()` = 2 (`stripBase()` = 16) — it thinks it owns
channels 17-24. Channels 9-16 have no owner. Routing silently corrupts.

**Invariant chosen: DENSE.** Active (non-Disabled) units must be contiguous
from position 0. Once a unit is Disabled, all later units must be Disabled.
Under this rule, config index == vector index, so `unitForChannel((g-1)/8)`
is correct and `HardwareUnit::unitIndex()` matches the vector position.

**Enforcement (dialog, parser, and construction):**

- Export `bool isUnitConfigDisabled(const UnitConfig&)` from
  `SurfaceConfig.h`; it is the one definition of Disabled used by the dialog,
  parser, and constructor.
- Export `bool hasDenseUnitTopology(const SurfaceConfig&)`. It returns false
  when unit 0 is Disabled or when a non-Disabled unit follows a Disabled one.
  It deliberately does **not** require any `isMain()` unit.
- **Dialog save:** before serializing in the `WM_USER + 1024` handler, reject
  a non-dense configuration and retain the dialog state. Message: *"Unit
  positions must be contiguous from Unit 1. Disable Unit N only if all units
  after it are also disabled."*
- **`createFunc`:** immediately after parsing, validate the same topology.
  A hand-edited or stale KLINKE2 string that is non-dense must be logged and
  replaced with `makeDefaultSurfaceConfig()` before constructing `CSurf_MCU`.
  This is a safe recovery for malformed topology, not a Main-unit validation.
- **Unit 0** cannot be Disabled in the dialog (`s_validTypesUnit0`), but the
  parser-level check remains mandatory because UI restrictions can be bypassed.
- **Optional dialog assist:** when the user sets unit `i` to Disabled, grey
  out the type combos of units `i+1..7` (they inherit Disabled).

**Constructor hardening:** assert `hasDenseUnitTopology(m_surfaceConfig)`
before building units. This is a diagnostic only; the parser/create path is
the release-safe enforcement point.

### 0b. Channel scope decision (F2)

**Decision: channels 9+ are NOT enabled in WP-EF.** Modes remain 1-8. The
input gate for strip events from units > 0 STAYS. Specifically:

- Strip input (faders, VPOT, rec/solo/mute/select, VPOT-push) from unit 1+
  is still dropped (current WP-B behavior).
- Global input from unit 1+ — see Step 2d for the single-owner policy. At
  most one transport-capable unit can dispatch global input, so ButtonManager
  state never receives overlapping presses from two physical units.

This means WP-EF's N>1 value is **global output only**. Two mains both show
transport; an extender still shows nothing until per-mode work widens its
strips. This is honest and testable.

### 0c. ProX-flag compatibility policy (F5)

`#define CONFIG_FLAG_PROX 16` is **retained**. It is still used by 8 mode
files (PanMode, MultiTrackMode ×3, PlugMode ×2, CommandMode, SendReceiveMode
×2) which are out of scope. WP-EF removes only **routing-layer** uses
(`csurf_mcu.cpp`, `CCSManager.cpp`, `VPOT_LED.cpp`, `MeterBridge.cpp`,
`SendReceiveMeterBridge.cpp`). The define, its derivation in `SurfaceConfig`,
and its dialog handling stay until the per-mode migration deletes them.

The final verification greps for routing-layer call sites, **not** for the
define's existence.

### 0d. N=1 compatibility definition (F10)

**Promise: protocol-state equivalence, not byte-identical MIDI.** Replacing
direct `m_midiout->Send()` with `SetLED()` (which deduplicates) changes the
wire traffic but preserves observable hardware state. The N=1 checklist
verifies state, not byte traces. State equivalence is the testable claim.

### 0e. Test seam (F11, medium priority)

Add a thin virtual send interface so routing helpers can be exercised with a
fake MIDI sink in a future test harness. Not a blocker for WP-EF — the
project has no test harness today — but the helpers should be written so a
seam can be inserted later (no static state, unit-iteration observable).

**Exit WP-EF-0:** topology invariant documented and enforced; scope decision
recorded; ProX policy settled; the remaining steps can assume a dense config
(zero or more main units allowed).

---

## Core routing model (two-axis, made explicit at every call site)

| Axis | Notes | Route |
|---|---|---|
| **Strip note** (`0x00..0x27`) | note encodes LOCAL channel (0-7), not unit. Caller MUST supply global channel. | `setStripLED(globalChannel, localNote, state)` → owning unit. No fallback. |
| **Strip CC** (VPOT ring `0x30+`, fader `0xE0+`, meter `0xD0/0xD1`) | per-strip, owning unit. | `sendStripCC/sendStripFader/sendStripMeter(globalChannel, ...)` → owning unit. |
| **Global note** (`0x28..0x7f`) | transport/assignment/automation/function/modifier LEDs. | `setGlobalLED(note, state)` → every transport-capable unit. |
| **Global CC/SysEx** (7-seg `0x40+`, timecode) | per main unit. | `sendMidiToTransportUnits(...)` → every transport-capable unit. |
| **Master fader** | linked-mode coincidence. | `broadcastMasterFader(value)` → every unit. |
| **Master meter** (ProX) | per-unit ProX quirk. | `updateMasterMeters()` → every ProX unit, via `0xD1`. |

**API contract (F6):** `SetLED(button, state)` becomes global-only (broadcasts
to transport units). Any strip note reaching it is a migration bug →
`ASSERT(isGlobalLedNote(button))` in debug. Strip LEDs MUST go through
`setStripLED(globalChannel, localNote, state)`.

---

## Steps

### Step 1 — Capability queries and routing helpers

**Goal:** centralize the capability rule; provide helpers whose names say
which surface domain they affect.

**Files:** `src/core/csurf_mcu.h`, `src/core/csurf_mcu.cpp`.

```cpp
// capability
bool isTransportUnit(const HardwareUnit *u) const { return u->isMain(); }
bool hasTransportUnits() const;
HardwareUnit *firstTransportUnit() const;       // NULL if no main unit exists

// global broadcast
void setGlobalLED(int note, int state);         // transport-capable units
void sendMidiToTransportUnits(unsigned char status, unsigned char d1,
                              unsigned char d2, int frameOffset);
void sendMidiToAllUnits(unsigned char status, unsigned char d1,
                        unsigned char d2, int frameOffset);
void setLEDOnAllUnits(int note, int state);

// strip routing (global channel → owning unit)
HardwareUnit *unitForChannel(int g) const;      // already exists; add range ASSERT
void setStripLED(int globalChannel, int localNote, int state);
void sendStripCC(int globalChannel, unsigned char cc, unsigned char value,
                 int frameOffset);
void sendStripFader(int globalChannel, int value);   // wraps unit->sendStripFader
void sendStripMeter(int globalChannel, short meter); // 0xD0, owning unit
```

`unitForChannel` gets `ASSERT(g >= 1 && g <= availableChannels())` and returns
NULL for out-of-range (release). Strip helpers no-op on NULL unit.

**No silent fallback** when `hasTransportUnits()` is false: helpers no-op
(the empty loop does nothing). This is a valid configuration when all units
are extenders; it is not an error and must not route global output to unit 0.

**Checkpoint:** helpers compile, unused. N=1 unchanged.

---

### Step 2 — Global output migration

**Goal:** every global output goes through the new helpers; no `m_is_mcuex`
gate, no direct `m_midiout->Send()` for global state.

**Files:** `src/core/csurf_mcu.cpp` (primary), `src/core/CCSManager.cpp`.

#### 2a. Make SetLED() global-only

Rework `SetLED(button_nr, led_state)`:

```cpp
void CSurf_MCU::SetLED(int button_nr, int led_state) {
  ASSERT(isGlobalLedNote(button_nr));   // catch stray strip callers
  if (isGlobalLedNote(button_nr))
    setGlobalLED(button_nr, led_state);
}
```

`isGlobalLedNote`: `note >= 0x28 && note <= 0x7f`.
`isStripLedNote`: `note >= 0x00 && note <= 0x27`.

Existing global callers of `SetLED` (`CCSManager::updateFlipLED`,
`updateGlobalViewLED`, `updateVPOTLeds` for VPOT-assign notes `0x28+`,
`Transport::updateLeds`, `UpdateGlobalSoloLED`, `UpdateMetronomLED`) keep
working — they pass global notes.

#### 2b. Replace direct global sends

In `csurf_mcu.cpp`, replace `m_midiout->Send(...)` for global state with
`SetLED()` (for note LEDs) or `sendMidiToTransportUnits()` (for CC):

| Location | Change |
|---|---|
| `MCUReset()` zoom/scrub | `SetLED(B_ZOOM/B_SCRUB, ...)` |
| `OnSave`/`ClearSaveLed` | `SetLED(B_SAVE, ...)` |
| `OnUndo`/`ClearUndoLed` | `SetLED(B_UNDO, ...)` |
| `OnZoom`/`OnScrub` | `SetLED(B_ZOOM/B_SCRUB, ...)` |
| `SetPlayState` | `SetLED(B_RECORD/B_PLAY/B_PAUSE, ...)` (3 sends) |
| `SetRepeatState` | `SetLED(B_CYCLE, ...)` |
| `UpdateAutoModes` | `SetLED(0x4A..0x4E, ...)` (5 sends) |
| `Run()` SMPTE/beats LEDs | `SetLED(0x71/0x72, ...)` |
| `Run()` 7-seg time digits | `sendMidiToTransportUnits(0xB0, 0x40+x, digit, -1)` |

Delete the `!m_is_mcuex` gates at these sites. Do **not** delete `m_is_mcuex`
itself (still a shim; removed only when all uses are gone — out of scope).

#### 2c. Assignment display per transport unit

`CSurf_MCU::sendAssignmentDisplayToTransportUnits(const char text[2])`:
iterate transport units, skip `isProX()` units, send `0xB0 0x40+11`/`0x40+10`.
Replace `CCSManager::setAssignmentDisplay`'s `IsExtender()`/`CONFIG_FLAG_PROX`
gate with this call.

#### 2d. Global input policy (F7)

`ButtonManager` remains surface-level in WP-EF, so it must receive global
input from **one physical source only**. Add a `m_globalInputUnitIndex` chosen
at construction time:

- if unit 0 is transport-capable, it is the owner (preserves current N=1
  behaviour exactly);
- otherwise use the first transport-capable unit in dense configuration order;
- if there is no transport-capable unit, set the index to `-1` and accept no
  global input.

In `Run()`, keep unit 0's existing full input path when it is the owner. For a
non-zero owner, admit only a conservative `isGlobalInputEvent(evt)` whitelist
(transport, F-keys, modifiers, jog, marker/nudge, automation, save/undo,
global-view, and SMPTE/beats). Drop fader status messages, VPOT CCs, touch
notes, and strip notes `0x00..0x27` from that unit. In that configuration,
unit 0 continues to dispatch only strip-local input and drops global events.
Drop every event from all other non-zero units.

This preserves channels 1–8 on unit 0, permits useful global control when the
first main-capable unit sits later in the dense chain, and prevents overlapping
press/release state from multiple main units. Per-unit/global-button aggregation
remains future work; it is not safe to describe the old shared Boolean state as
an acceptable limitation.

**Checkpoint:** transport/repeat/automation/SMPTE/time/save-undo/zoom-scrub/
drop/metronome/global-solo LEDs and time digits reach every main unit. N=1:
same observable state (state-equivalent, not byte-identical).

---

### Step 3 — DropState, blink emulation

**Goal:** drop LED via the global path; blink emulation per unit.

**Files:** `src/core/csurf_mcu.h`, `src/core/csurf_mcu.cpp`.

#### 3a. DropState → pure state holder (F8 partial)

```cpp
class DropState {
public:
  DropState() : m_state(0) {}
  void toggleState();
  void updateReaper();
  int ledState() const { return m_state; }   // 0/1/2
private:
  int m_state;
};
```

Remove `toggleStateAndUpdate(bool, midi_Output*)` and `updateMCU(bool, midi_Output*)`.
`OnDropButton` calls `toggleState()`; `updateReaper()`; `SetLED(B_DROP, ledState())`.
`MCUReset` calls `SetLED(B_DROP, m_dropstate.ledState())`.

#### 3b. Blink emulation across units

`anyUnitNeedsBlinkEmulation()`: true if `CONFIG_FLAG_EMULATING_BLINKING` set
OR any constructed unit `isProX()`. `Run()` uses it. `EmulateBlinkingLEDs(now)`
iterates all `m_units`, calls `unit->emulateBlinkingLEDs(now)`.

**Checkpoint:** drop cycles off/on/blink on every main unit. Blink works
independently per unit.

---

### Step 4 — Strip LED output: setStripLED migration (channels 1-8)

**Goal:** strip LEDs route through the owning unit via global channel. Modes
still emit channels 1-8, but the routing is now capability-correct and ready
for widening.

**Files:** `src/core/CCSManager.cpp`, `src/core/csurf_mcu.cpp`.

Migrate the four CCSManager strip-LED setters to `setStripLED`:

```cpp
void CCSManager::setRecLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODE
  m_pMCU->setStripLED(channel, channel - 1, state);      // rec notes 0x00..0x07
}
void CCSManager::setSoloLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODE
  m_pMCU->setStripLED(channel, 0x07 + channel, state);   // solo 0x08..0x0f
}
void CCSManager::setMuteLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODE
  m_pMCU->setStripLED(channel, 0x0f + channel, state);   // mute 0x10..0x17
}
void CCSManager::setSelectLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODE
  m_pMCU->setStripLED(channel, 0x17 + channel, state);   // select 0x18..0x1f
}
```

For N=1 these all map to unit 0 (correct). For N>1 they would route correctly
if modes emitted channels > 8 — but they don't yet, so this is plumbing only.

`CHECKMODEANDCHANNEL` stays `channel < 9` for now (modes are 1-8). Add a
comment: `// per-mode WP: widen to availableChannels()`.

**Checkpoint:** strip LEDs for channels 1-8 route through setStripLED (unit 0
for N=1). The `SetLED()` global-only ASSERT catches any stray direct strip
caller during migration.

#### 4a. Shutdown must not call global-only SetLED for strip notes

`CSurf_MCU::~CSurf_MCU()` currently loops over notes `0..127` and calls
`SetLED(i, LED_OFF)`. After Step 2, notes `0x00..0x27` are intentionally
invalid for that API and would assert in debug builds.

Replace that loop with per-unit shutdown output:

```cpp
for (int ui = 0; ui < numUnits(); ++ui)
  m_units[ui]->forceAllLEDsOff();
```

The shutdown fader reset belongs to Step 7d. Do not retain a strip-note
fallback in `SetLED()` merely to support the old destructor.

---

### Step 5 — VPOT_LED: per-unit ProX + correct routing

**Goal:** VPOT ring CCs route to the owning unit; ProX byte3=0 quirk is
per-VPOT (per its unit's model), not global.

**Files:** `src/hardware/VPOT_LED.h`, `src/hardware/VPOT_LED.cpp`,
`src/core/CCSManager.cpp`, `src/core/csurf_mcu.cpp`.

**Array stays `[9]`** (index 0 unused, 1-8). Modes use 1-8; no widening yet.

#### 5a. VPOT_LED knows its unit's ProX flag

```cpp
void VPOT_LED::init(CSurf_MCU *pMCU, int globalTrack, bool isProX) {
  m_pMCU = pMCU;
  m_track = globalTrack;     // global channel (1-based)
  m_isProX = isProX;
}
```

Add `bool m_isProX` to `VPOT_LED` and initialize it to `false` in the
constructor.

In `updateLEDs()`:
```cpp
int byte3 = m_bottom ? 1 << 6 : 0;
if (m_isProX) byte3 = 0;    // per-unit, replaces global CONFIG_FLAG_PROX
```

`m_pVPOTS[0]` is the unused master placeholder and has no owning strip unit:

```cpp
m_pVPOTS[0].init(getMCU(), 0, false);
for (int i = 1; i <= 8; ++i)
  m_pVPOTS[i].init(getMCU(), i,
                   getMCU()->unitForChannel(i)->isProX());
```

Never call `unitForChannel(0)`: it is invalid by contract. For channels 1–8
with a dense config, the helper returns the correct unit; for N=1, unit 0.

#### 5b. VPOT ring CC routes via sendStripCC

```cpp
// VPOT_LED::updateLEDs
m_pMCU->sendStripCC(m_track, 0x2F + CSurf_MCU::localOf(m_track),
                    byte3 + m_value, -1);
```

`sendStripCC` calls `unitForChannel(m_track)->sendMidi(0xB0, cc, value, ...)`.
`CSurf_MCU::localOf(g) = (g-1)%8 + 1`. For channels 1-8 on a dense config,
owning unit and local channel are correct.

This removes the last global `CONFIG_FLAG_PROX` check from the hardware layer.

**Checkpoint:** VPOT rings on channels 1-8 work as before; ProX byte3 quirk
now follows the owning unit's model. N=1 state-equivalent.

---

### Step 6 — MeterBridge: domain split + per-unit master meters (F3)

**Goal:** separate strip-meter state from master-meter state; fix the `% 8`
logic bug; route master meters to every ProX unit.

**Files:** `src/hardware/MeterBridge.h`, `src/hardware/MeterBridge.cpp`,
`src/modes/sends/SendReceiveMeterBridge.cpp`.

**Bug in first draft:** `int localPos = globalPos % 8; ... localPos < 8 ?
0xD0 : 0xD1` — `% 8` is always 0-7, so `0xD1` is unreachable. Master meters
cannot be sent. Also `m_mcu_meterpos[10]` (0-7 strip, 8-9 master) collides
once strip positions go global.

#### 6a. Split the stored state

```cpp
class MeterBridge {
protected:
  std::vector<double> m_stripMeterPos;   // sized availableChannels(), strip state
  double m_masterMeterPos[2];            // L/R master, independent of strips
  // ...
};
```

Strip state is dynamically sized (for N=1, 8 entries). Master state is always
exactly 2 values (L/R), never indexed by strip position.

`MeterBridge` is constructed before it has a per-frame surface argument, so
the plan must specify initialization rather than merely declare a vector:

```cpp
void MeterBridge::ensureStripMeterState(int channelCount) {
  if ((int)m_stripMeterPos.size() != channelCount)
    m_stripMeterPos.assign(channelCount, -100000.0);
}
```

Initialize `m_masterMeterPos[0..1]` to the same sentinel in the constructor.
Call `ensureStripMeterState(pMCU->availableChannels())` before the first index
in `updateMeter()` (or unconditionally at the start of every
`updateMeterBridge()` implementation). This supports a later active-unit count
change without indexing an empty or stale vector.

#### 6b. Two send paths

```cpp
// Strip meter: global channel → owning unit, always 0xD0
void CSurf_MCU::sendStripMeter(int globalChannel, short meter) {
  HardwareUnit *u = unitForChannel(globalChannel);
  if (!u) return;
  int local = (globalChannel - 1) % 8;           // 0..7
  u->sendMidi(0xD0, (local << 4) | meter, 0, -1);
}

// Master meter: every ProX unit, always 0xD1 (ProX-only feature)
void CSurf_MCU::sendMasterMetersToProXUnits(short left, short right) {
  for (int i = 0; i < numUnits(); ++i) {
    HardwareUnit *u = m_units[i];
    if (!u->isProX()) continue;
    u->sendMidi(0xD1, (0 << 4) | left,  0, -1);
    u->sendMidi(0xD1, (1 << 4) | right, 0, -1);
  }
}
```

`MeterBridge::sendToHardware` is split accordingly: strip path calls
`sendStripMeter`; master path calls `sendMasterMetersToProXUnits`.

The existing `updateMeter()` derives a zero-based storage index with
`int x = iChannel - 1`. Keep that index only for
`m_stripMeterPos[x]`; the routing call is one-based:

```cpp
ASSERT(iChannel >= 1 && iChannel <= pMCU->availableChannels());
int x = iChannel - 1;
// update m_stripMeterPos[x]
pMCU->sendStripMeter(iChannel, meter);
```

Do not pass `x` to `sendStripMeter()`, or physical strip 1 becomes invalid
channel 0.

#### 6c. updateMasterLEDs per ProX unit

`MeterBridge::updateMasterLEDs` and `SendReceiveMeterBridge::updateMasterLEDs`
drop the global `CONFIG_FLAG_PROX` gate and instead iterate ProX units (the
`sendMasterMetersToProXUnits` helper does the filtering). Master meter values
are computed once (from master track / selected track) and sent to every ProX
unit.

**Strip meter loops in modes stay capped at 1-8** (per WP-EF-0b). The
`m_stripMeterPos` vector is sized to `availableChannels()` but only positions
0-7 are written until per-mode work. No out-of-bounds access.

**Checkpoint:** strip meters 1-8 unchanged; master meters route to every ProX
unit; no array collision. N=1 state-equivalent.

---

### Step 7 — Lifecycle: cache invalidation + reset order (F8)

**Goal:** reset/shutdown leave all units coherent; fader/LED caches do not
stale-skip sends after a hardware reset.

**Files:** `src/core/csurf_mcu.cpp`, `src/hardware/HardwareUnit.h`,
`src/hardware/HardwareUnit.cpp`.

#### 7a. The cache-stale problem

`HardwareUnit::sendStripFader(local, v)` and `setLED()` deduplicate against
cached values. After `reset()` (SysEx), the hardware state is reset but the
cache still holds pre-reset values. A subsequent `sendStripFader(local, 0)`
when the cache is already 0 sends **nothing** — but the hardware was just
reset and needs the value. This is a likely contributor to the open
fader-zero-on-startup bug (MEMD 2026-07-09).

#### 7b. Cache invalidation API

Add to `HardwareUnit`:
```cpp
void invalidateFaderCache();   // set m_faderPos[0..8] to -1 (sentinel)
void invalidateLEDCache();     // set m_led_state[128] to LED_UNKNOWN
```

`MCUReset()` is also called during construction, before `m_pTransport` exists.
It must therefore reset surface state and publish only output that does not
dereference `m_pTransport`. The constructor owns the first transport LED
publication after it has allocated the object.

Call order in the constructor:
```cpp
for (each unit) {
  unit->reset();                 // SysEx reset
  unit->invalidateFaderCache();  // caches no longer reflect hardware
  unit->invalidateLEDCache();
}
// ... surface-level state resets (once) ...
for (each unit) {
  unit->forceAllLEDsOff();       // now actually sends (cache was invalidated)
}
MCUReset();                       // surface state only; no m_pTransport access
// publish initial state via SetLED / setStripLED / setStripFader
// (these now send because caches are fresh)
m_pTransport = new Transport(this);
m_pTransport->updateLeds();     // member, NOT Transport::instance()
for (each unit) unit->startInput();
```

For a later hardware reset, call `m_pTransport->updateLeds()` only after the
surface-level reset if `m_pTransport != NULL`. This preserves the constructor
ordering while republishing Marker/Nudge state after device reset.

#### 7c. Transport call fix

The first draft wrote `Transport::instance()->updateLeds()`. There is no such
singleton. The surface owns `Transport *m_pTransport`; it may be used only
after construction, as specified above.

#### 7d. Shutdown output

After Step 4a removes the old `SetLED(0..127)` loop, make shutdown explicitly
per-unit:

```cpp
for (int ui = 0; ui < numUnits(); ++ui) {
  HardwareUnit *u = m_units[ui];
  u->invalidateFaderCache();
  for (int local = 0; local < 8; ++local)
    u->sendStripFader(local, 0);
  u->setMasterFader(0);
  u->forceAllLEDsOff();
}
```

This replaces the unit-0 `SendMidi(0xE0+i, ...)` loop as well as the invalid
strip calls through global-only `SetLED()`.

**Checkpoint:** after reset, faders and LEDs reach the hardware even when
their cached values were already at the target. This may incidentally improve
the fader-zero-on-startup symptom — but WP-EF does not claim to fix that bug
(needs its own root-cause pass).

---

### Step 8 — Verification

**Build (all three platforms must be clean):**
```bash
# Linux
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)")
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
# Windows: scripts/build-windows.sh
# macOS: cmake + clang
```

**N=1 protocol-state regression (state-equivalent, not byte-identical):**
- Startup splash, play/pause/record/repeat, time display, SMPTE/beats,
  assignment, save/undo, zoom/scrub, drop, metronome, global-solo all behave
  as before at the hardware level.
- VPOT rings, strip meters, strip LEDs (rec/solo/mute/select) for channels
  1-8 correct.
- Faders respond and report touch; rec/solo/mute/select track state.
- Constructing, resetting, and destroying the surface does not trigger the
  global-only `SetLED()` strip-note ASSERT.

**N=2 with one main + one extender:**
- Main receives all global LEDs, time display, assignment.
- Extender does NOT receive global transport/timecode (not transport-capable).
- Extender still receives reset + forceAllLEDsOff lifecycle output.
- Strip output still limited to unit 0 channels 1-8 (modes not widened —
  expected, documented).
- No assertions fire.

**N=2 with two main-capable units (the headline WP-EF value):**
- Both mains receive play/pause/record/repeat, SMPTE/beats, time digits,
  automation, flip, global-view, zoom, scrub, save, undo, drop, metronome,
  global-solo LEDs.
- A ProX main does NOT receive assignment digits; a Mackie main does.
- Blink emulation runs independently per unit.
- Global button presses are accepted only from the chosen global-input owner;
  presses on the other main are ignored. Simultaneous presses therefore cannot
  corrupt surface-level ButtonManager hold/release state.

**Topology validation:**
- Dialog rejects a config with a gap (enabled unit after a disabled one).
- A hand-edited non-dense KLINKE2 string is detected by `createFunc`, logged,
  and replaced with the safe default before constructing the surface.
- Configs with zero main units are allowed and require no special validation.
- Unit 0 cannot be set to Disabled.
- Global output is routed to every main unit; if there are zero main units,
  there are no global-output targets and the helpers safely no-op.

**Grep verification (routing layer only):**
```bash
# These should be ZERO after WP-EF:
grep -rn "m_midiout->Send" src/core/csurf_mcu.cpp          # direct global sends gone
grep -rn "m_is_mcuex" src/core/csurf_mcu.cpp               # output gates gone
grep -rn "CONFIG_FLAG_PROX" src/core/CCSManager.cpp src/hardware/   # routing-layer uses gone
grep -rn "IsExtender" src/core/CCSManager.cpp              # gating gone
# These are EXPECTED to remain (deferred):
grep -rn "CONFIG_FLAG_PROX" src/modes/                     # mode-level, out of scope
grep -rn "m_is_mcuex" src/core/csurf_mcu.h                 # shim retained
grep -rn "m_midiout" src/core/csurf_mcu.h                  # shim retained
```

---

## Exit Criteria (adopted + amended from the critical review)

1. **Topology:** a config with disabled rows has defined, tested behaviour
   (dense invariant enforced on dialog save and in `createFunc`). Zero main
   units remain valid. No routing helper indexes `m_units` without validating
   its argument. (F1)
2. **Channel scope:** the plan makes ONE unambiguous promise — channels 9+
   are NOT enabled; input gate for strip events retained; global output is
   the N>1 deliverable. (F2)
3. **MeterBridge:** strip-meter state is separate from master-meter state;
   master meters route to every ProX unit; no array collision; the `% 8`
   bug is gone. (F3)
4. **SetLED contract:** `SetLED()` is global-only with a debug ASSERT;
   `setStripLED(globalChannel, ...)` is mandatory for strip notes with no
   silent fallback. (F6)
5. **DropState** is a pure state holder; LED output via `SetLED`. (F8)
6. **Blink emulation** iterates all units; gated on
   `anyUnitNeedsBlinkEmulation()`.
7. **Lifecycle:** reset invalidates per-unit fader/LED caches before
   publishing initial state; `m_pTransport` is used only after construction;
   shutdown resets LEDs/faders per unit without strip calls to `SetLED`. (F8)
8. **CONFIG_FLAG_PROX** retained as a documented legacy flag; zero
   routing-layer call sites; mode-level uses untouched. (F5)
9. **N=1 compatibility:** protocol-state equivalence (not byte-identical),
   verified by the manual state checklist. (F10)
10. **No `!m_is_mcuex` output gates** remain in the routing layer.
11. **Zero compiler warnings** on Linux, Windows, and macOS.
12. **Global input ownership:** only the configured global-input owner reaches
    `ButtonManager` for global events; all other secondary-unit global input
    is ignored. (F7)

---

## Deferred to per-mode work (§7 of master plan) — NOT WP-EF

- Enabling channels 9..N*8 in MultiTrack/Send/Receive/Command/Plug/Pan modes.
- VPOT_LED array resize to `availableChannels()+1`.
- MeterBridge strip-state vector written beyond positions 0-7.
- ButtonManager per-unit strip state (double-click/hold isolation).
- Input gate removal for strip events.
- Mode-level `CONFIG_FLAG_PROX` migration (~8 files).
- MultiDisplay field routing for mode LCDs (channels 9+).
- Full legacy shim removal (`m_midiout`, `m_midiin`, `m_is_mcuex`, `m_offset`,
  `m_size`, `g_mcu_list`, `IsExtender`/`GetOffset`/`GetSize`, `SendMidi`
  unit-0 shim).
- Dynamic "release the extenders" activation.

---

## Risks

| # | Risk | Mitigation |
|---|---|---|
| R1 | Dense invariant rejected by a user who wants a gap (dead unit in the middle) | Document; gaps are not supported. Reorder units physically or in config. |
| R2 | Multiple main units could overlap global button press/release state | Select exactly one global-input owner in Step 2d; do not dispatch global input from other secondary units. |
| R3 | Cache invalidation changes N=1 startup timing and surfaces the latent fader bug | Do not claim to fix the fader bug; log if behavior changes; separate investigation. |
| R4 | `setStripLED` migration misses a caller → ASSERT fires in debug | The ASSERT is the safety net; fix and rebuild. |
| R5 | VPOT_LED ProX flag set at init but config changes at runtime (dialog re-open) | Constructor rebuilds units on config change (existing behavior); re-init reads new model. |
| R6 | Test seam (WP-EF-0e) not added → routing bugs only caught on hardware | Accept for now; write helpers testably for future seam insertion. |
