# WP-A — `HardwareUnit` Abstraction (design draft)

> Part of the extender-support effort (see `extender-support.md`). Draft whose
> six open design questions (former §9) were **resolved 2026-07-07** (see §9);
> interface proposals now reflect those decisions but are still pre-implementation.

## 1. Goal

Extract one physical MCU unit's hardware concerns out of `CSurf_MCU` into a
new `HardwareUnit` class. `CSurf_MCU` then owns `N` `HardwareUnit`s
(`N = 1..8`) and becomes the single *logical* surface that multiplexes between
global channel indices (`1..N*8` strips + master) and per-unit local indices.

This is the foundation: every other work package (config dialog, Tracks
refactor, display routing, CCSManager sizing) builds on it.

## 2. What a `HardwareUnit` owns

```cpp
enum DeviceModel { Mackie, QConProX };  // QConProX ⇒ 2 LCD panels + VPOT/meter/blink/assignment quirks

struct UnitConfig {
    int   midiInDev;
    int   midiOutDev;
    bool  isMain;        // main ⇒ device id 0x14 + transport; extender ⇒ 0x15, strips only
    DeviceModel model;   // Mackie | QConProX  (lcdPanels = QConProX?2:1; drives all ~20 ex-CONFIG_FLAG_PROX quirks)
};

class HardwareUnit {
    int             m_unitIndex;     // 0..N-1  → strip base = m_unitIndex*8
    UnitConfig      m_cfg;
    unsigned char   m_deviceId;      // derived: isMain ? 0x14 : 0x15
    midi_Output*    m_midiout;
    midi_Input*     m_midiin;
    DisplayHandler* m_display;       // THIS unit's center LCD(s) — owned here, not in CSurf_MCU

    // per-unit hardware-state caches (moved OUT of CCSManager's [9] arrays):
    int  m_faderPos[9];              // local 0..7 strips + [8]=master
    int  m_ledState[...];            // select/mute/solo/recarm per local strip
    ...
};
```

Lifecycle mirrors today's `CSurf_MCU` ctor/dtor: open MIDI (with the JACK
`usleep` workaround), `MCUReset()` SysEx + host-query handshake on construct,
reset + close on destruct.

## 3. Interface — outgoing (logical surface → hardware), LOCAL indices

`CSurf_MCU` translates a global channel to `(unitIndex, local)` and calls these.
All local: strips `0..7`, master handled by the dedicated master method.

```cpp
void setStripFader(int local, int value);   // MIDI 0xE0 + local
void setMasterFader(int value);             // MIDI 0xE8
void setVPOT(int local, const VPOT_LED&);   // CC 0x30 + local
void setSelectLED(int local, int state);    // note 0x18 + local
void setMuteLED  (int local, int state);    // note 0x10 + local
void setSoloLED  (int local, int state);    // note 0x08 + local
void setRecLED   (int local, int state);    // note 0x00 + local
void setMeter    (int local, int level);    // CC 0xD0 channel-meter
void writeLCD    (int row, int pos, const char*, int len); // via m_display
void reset();                               // F0 00 00 66 <devId> 09 / 63 ...
// main-only (no-op on extender):
void setGlobalLED(int buttonId, int state); // transport/F-key/modifier LEDs
void setAssignmentDisplay(const char text[2]);
void setSMPTE(...);  void setBeats(...);
```

## 4. Interface — incoming (hardware → logical surface), GLOBAL indices

The `HardwareUnit` parses raw MIDI from its `m_midiin` (today's
`OnFaderMove`/`OnRotaryEncoder`/`OnButtonPress` logic, now per-unit) and emits
**global** events via a listener/callback owned by `CSurf_MCU`. The unit adds
its `m_unitIndex*8` offset itself, so the surface never deals with local
indices on the input side.

```cpp
// listener (CSurf_MCU implements):
void stripFaderMoved (int globalChannel /*1..N*8*/, int value);
void masterFaderMoved(int unit, int value);          // per-unit master slot
void vpotMoved       (int globalChannel, int delta);
void vpotTouched     (int globalChannel, bool touched);
void stripButton     (StripButton /*SELECT|MUTE|SOLO|RECARM*/,
                      int globalChannel, ButtonEvent /*press|DC|long|release*/);
// main-only — emitted only by units with isMain:
void globalButton    (int buttonId, ButtonEvent);    // transport, F1-F8, modifiers, flip, ...
void jogWheel        (int delta);
void pedalMoved      (int value);
```

**Master-slot convention (linked mode, see §6):** any unit's master move → the
mode sees `masterFaderMoved(unit, …)`; the surface treats master as a single
logical "channel 0" and broadcasts back to all units (see §6).

## 5. What moves out of `CSurf_MCU` → into `HardwareUnit`

| Today in `CSurf_MCU` | Moves to `HardwareUnit` | Notes |
|---|---|---|
| `m_midiout`, `m_midiin`, open/close | ✓ | incl. JACK `usleep` workaround |
| `MCUReset()`, reset/handshake SysEx | ✓ | uses unit's `m_deviceId` |
| `SendMidi()` / `SendMsg()` raw send | ✓ | to this unit's port |
| `OnFaderMove/OnRotaryEncoder/OnButtonPress/OnJogWheel/OnPedalMove/OnTouch` parsing | ✓ (becomes the unit's MIDI-in parser) | emits global events (§4) |
| `UpdateMackieDisplay()` | ✓ (via `m_display`) | per-unit LCD |
| `m_is_mcuex`, `m_offset`, `m_size` | replaced by `m_unitIndex` + `UnitConfig` | offset now implicit |
| `SetLED()`, fader-position caches | ✓ | per-unit hardware state |
| `DisplayHandler` (single) | one per unit (`m_display`) | see §7 |
| transport/SMPTE/assignment/automode/global-LED code | stays in `CSurf_MCU` | routed to all main units |

`CSurf_MCU` keeps: `Tracks`, `CCSManager` + modes, `Transport`, global state,
and the N `HardwareUnit*`. `g_mcu_list`, the dead extender scaffolding, and the
`FIXID` macro are deleted.

## 6. Global ↔ local translation & the master slot

- **Strips:** global channel `g` (1..N*8) → `unit = (g-1)/8`, `local = (g-1)%8`.
  Mode loops change from `for (ch=1; ch<9; ++ch)` to `for (ch=1; ch<=N*8; ++ch)` —
  the body is otherwise unchanged.
- **Master = logical channel 0 (linked mode):** the modes keep calling
  `setFader(0, …)` / receiving `fader(0, …)`. `CCSManager::setFader(0)` broadcasts
  to **all** units' `setMasterFader()`; any unit's `masterFaderMoved` is reported
  to the mode as `fader(channel=0)`. So all N master faders mirror each other —
  exactly "all master slots coincide in linked mode". (Per-unit master meaning is
  future independent-mode work; the `HardwareUnit` already exposes it per-unit.)
- Channel 0 is fader-only — no VPOT/select/solo/mute/recarm (matches the
  fader-only master slot).

## 7. Display/`DisplayHandler` becomes per-unit (the tricky bit)

Today there is **one** `DisplayHandler` (held by `CSurf_MCU`); modes write
7-char *fields* to it. With N units each having its own center LCD, the field for
global strip `g` must land on unit `(g-1)/8`'s display at local field `(g-1)%8`.

**Proposal:** each `HardwareUnit` owns its `DisplayHandler` (its MIDI port +
device id + the 1-or-2-panel row handling). The mode-facing `Display` becomes a
**virtual multiplexer**: `changeField(row, field, text)` for `field` in `0..N*8-1`
routes internally to `units[field/8]->writeLCD(...)` at local field `field%8`.
Modes then keep their `changeField(row, f, …)` calls unchanged, only the field
range grows to `N*8`.

**Resolved (§9.4, 2026-07-07):** composite `MultiDisplay` owned by the mode
(IS-A `Display`, holds N real per-unit child `Display`s); rows 2-3 silently
dropped on non-ProX units.

## 8. `CCSManager` consequences (touches WP-F, listed for context)

- `VPOT_LED m_pVPOTS[9]` → sized for `N*8` (master has no VPOT).
- `setFader/setVPOT/setSelectLED/...` take a **global** channel and route via
  `CSurf_MCU` to the right `HardwareUnit`; `setFader(0)` → broadcast master.
- The strip/master hardware-state caches (`m_faderPos[9]`, `m_stateRec[9]`, …)
  **move into `HardwareUnit`** (each unit owns its 8+master state). `CCSManager`
  keeps only logical/mode-level state.
- `CHECKMODEANDCHANNEL` channel bound changes from `0..8` to `0..N*8`.

## 9. Resolved design decisions (2026-07-07)

All six open questions from the draft are now decided:

1. **Input-side indexing → GLOBAL.** `HardwareUnit` knows its `m_unitIndex`,
   adds the offset itself, and emits **global** channel indices (`1..N*8`) on
   the input side. The surface/modes never deal with local indices on input.
   (Symmetric with the output translation, which stays global→local in
   `CSurf_MCU`.)

2. **Master slot → logical channel 0, BROADCAST (linked mode).** `setFader(0)`
   fans out to **all** units' `setMasterFader()`; any unit's
   `masterFaderMoved` is reported to the mode as `fader(channel=0)`, so all N
   master faders mirror each other. Per-unit master meaning stays available on
   the `HardwareUnit` API for a future independent mode but is **not** exposed
   to linked-mode modes now.

3. **Hardware-state caches → MOVE INTO `HardwareUnit`.** `faderPos[9]`,
   `ledState`, `stateRec`, and the other per-strip hardware caches move out of
   `CCSManager`'s `[9]` arrays into `HardwareUnit` (each unit owns its 8+master
   state). `CCSManager` keeps only logical/mode-level state.

4. **Display multiplexer → COMPOSITE `MultiDisplay`.** The mode holds a
   composite `Display` (IS-A `Display`) that owns **N real per-unit child
   `Display`s**, one per `HardwareUnit`. `changeField(row, globalField)` routes
   to `child[(globalField-1)/8].changeField(row, (globalField-1)%8 + 1)`;
   `switchTo()` / `clear()` / `resendAllRows()` broadcast to all children.
   **Rows 2-3 (ProX 2nd panel) are silently dropped on non-ProX units** (a
   Mackie extender simply has no 2nd panel). Per-unit `DisplayHandler` /
   `Display` stay **byte-identical to today**; modes change only their field
   range (`1..8` → `1..N*8`) and construction site (`new Display(h,4)` →
   `createMultiDisplay(h, N)`).

5. **Multiple main units → ACCEPT REDUNDANCY.** All transport-capable units'
   global buttons fire; actions are idempotent (`SetLED` sets state, play/stop
   are toggles). No "primary main" concept — consistent with the orthogonality
   rule (config position ≠ global routing).

6. **MIDI-open / JACK sequencing → OPEN ALL, ONE 200 ms PAUSE, RESET ALL,
   START ALL.** Open all N outputs first, then a single 200 ms `usleep`, then
   reset all units, then start all inputs. Start latency stays 200 ms total
   (independent of N). **Caveat:** the PipeWire-JACK buffer-allocation race
   (MEMD 2026-06-27) was only ever observed with a single port; the N-port
   variant is **unverified and must be tested on real multi-unit hardware**
   during the WP-A integration pass. Fallback if it misbehaves: per-port 200 ms
   (200 ms × N).

## 10. Out of scope for WP-A (later WPs)

- Config dialog + config string (WP-B).
- `Tracks` singleton refactor to `N*8` (WP-C) — the big one.
- Bank/scroll behaviour (WP-D).
- Any mode behaviour change (modes come after all scaffolding).
