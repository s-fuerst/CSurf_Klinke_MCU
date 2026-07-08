# Extender Support — Planning Document

> Status: **PLANNING** (no code changes yet). This document collects findings,
> decisions, and the roadmap for re-introducing multi-unit (extender) support.
> Companion to the per-mode design docs that will follow.

## 1. Goal

Combine multiple MCU-protocol hardware units into **one logical control surface**
so the user can drive more than 8 channels (faders/VPOTs/select-mute-solo-rec)
at once.

- **Hardware target:** up to **8 units = 64 channels** (1 main + up to 7 extenders).
- **Test setup:** 1 main + 2 extenders = 24 channels.
- **Scope of this effort:** the *scaffolding*; each mode (multitrack, plugin,
  commands, sends) gets its own design pass afterwards.

## 2. Current state (what the code does today)

The extender support was **deliberately removed in v0.8**. Concrete evidence:

| Location | Evidence |
|---|---|
| `src/core/csurf_mcu.cpp` `parseParms()` | Forces `parms[0]=0` (offset) and `parms[1]=8` (banksize) regardless of config, with comment *"since v0.8 we don't support extender"*. |
| `src/core/csurf_mcu.cpp` `createFunc()` | Refuses to construct a 2nd `CSurf_MCU`: MessageBox *"only a single instance can be used"*, returns `NULL`. |
| `src/csurf_main.cpp:395` | `rec->Register("csurf", &csurf_mcuex_modified_reg)` is **commented out**; the matching `extern` at `:11` too. |
| `src/core/csurf_mcu.h` | `EXT_ID = "unused"` (was the type-string token that identified an extender). `createFunc`'s `!strcmp(type_string, EXT_ID)` can therefore never be true. |

**Dead scaffolding still present** (useful, but currently dormant):

- `static WDL_PtrList<CSurf_MCU> g_mcu_list` — populated in ctor, removed in dtor.
- `CSurf_MCU`: `m_is_mcuex`, `m_offset`, `m_size`, `IsExtender()`, `GetOffset()`, `GetNumMCUs()`.
- `reaper_csurf_reg_t csurf_mcuex_modified_reg` struct still defined (`csurf_mcu.cpp:1755`).
- `Tracks::adjust(g_mcu_list.GetSize() * 8)` already scales channel count by MCU count — runs with `1*8` today.
- `DisplayHandler::EnumMCUType { MCU, MCU_EX, PROX }` — extender display type known.
- `FIXID` macro in `csurf_mcu.h` maps a Reaper track to a channel via `m_offset + Tracks::globalOffset`.

**The hard architectural blocker** (called out in the source): `Tracks` is a
**singleton holding a single `m_pMCU` pointer**. Comment in `Tracks.h`:
*"extreme ugly hack … This can't work with the extender (like all the other
m_pMCU access in the Tracks singleton)"*. Every mode reaches the MCU through
`Tracks::instance()`, i.e. through exactly one MCU. Plus ~92 hard-coded `8`/`9`
across the modes and hardware layer (`VPOT_LED[9]`, channel displays, etc.).

## 3. How the references do it

- **Cockos original** (`reaper-sdk/reaper-plugins/reaper_csurf/csurf_mcu.cpp`):
  two separate registrations (`csurf_mcu_reg` + `csurf_mcuex_reg`). Each unit is
  its own `CSurf_MCU` instance with its own MIDI device + offset (0/8/16…).
  Coordination via static `m_mcu_list` and static globals
  (`m_allmcus_bank_offset`, `g_csurf_mcpmode`). The main unit owns the global
  displays / transport LEDs (gated by `!m_is_mcuex`); extenders are "mute"
  channel strips.
- **CSI / DrivenByMoss:** treat the combined hardware as **one logical surface**
  of N×8 channels, parameterised by device count. Cleaner — no single-pointer
  singleton.

## 4. Decisions (from the kickoff Q&A)

1. **Architecture = Option B (one logical surface owns N hardware units).**
   The user adds **one** surface in Reaper prefs; our config dialog lists the
   units (main + up to 7 extenders), each with its own MIDI in/out. `CSurf_MCU`
   is a single instance owning `HardwareUnit[0..N]` and driving `N*8` channels.
   This dissolves the `Tracks` single-`m_pMCU` problem cleanly (parameterise by
   total channel count instead of one pointer).

2. **Mode coupling = linked first, but keep the door open.** All units follow the
   main unit's active mode (e.g. main=mixer + extender=mixer ⇒ 16 channels of
   mixer). The architecture must **not** rule out per-mode-independent operation
   later (possible future "pro mode") — do not hard-couple in an irreversible way.

3. **Sequence = scaffolding first, modes after.** Build the infrastructure
   (HardwareUnit abstraction, config dialog, Tracks refactor, offset/bank logic,
   global-display ownership) before touching any mode's behaviour.

## 5. Target architecture sketch (Option B)

```
Reaper prefs: ONE surface  "Mackie Control Protocol (Klinke)"
   └─ config dialog:
         Unit 0 (main):  MIDI-In  [..]  MIDI-Out [..]
         Unit 1 (ext):   MIDI-In  [..]  MIDI-Out [..]
         ... up to Unit 7
         (offset is implicit = unitIndex * 8; no manual offset field)

CSurf_MCU  (single instance)
 ├─ HardwareUnit* m_units[1..8]      ← NEW: encapsulates one physical MCU
 │    each owns: midi_Output*, midi_Input*, MCUReset/SysEx, 8 faders,
 │               8 VPOT-LEDs, 8 channel-displays, select/mute/solo/rec LEDs
 ├─ DisplayHandler* m_mainDisplay     ← the 2x55 assignment/timecode display
 │                                      (lives on the main unit only)
 ├─ Tracks*  (refactored: N*8 channels, no single m_pMCU)
 ├─ CCSManager / modes  (linked across units first)
 └─ global state: bank/globalOffset, selected track, automation mode, ...
```

Key idea: the physical-unit concerns (MIDI I/O, SysEx reset, per-channel
hardware addressing) move **down** into a new `HardwareUnit`; everything
"logical" (tracks, modes, global state) stays in `CSurf_MCU` but is
parameterised by the **total** channel count instead of assuming 8.

### 5.1 Channel & hardware model (resolved)

Each physical MCU unit exposes **9 fader slots**: 8 *full* channel strips
(fader + VPOT ring + select/mute/solo/recarm buttons) plus 1 *fader-only* slot
(the "master" position: fader + touch only — **no** VPOT, no
select/solo/mute/recarm). Separately a unit may own a **center LCD** (0, 1, or
2 panels of 2×56 chars; the strips appear as 7-char *fields* on it) and, for
main-type units, a transport section + 7-segment timecode/assignment displays.

- **Strips are addressed flat, `1 .. N*8`.** Unit `u` owns strips `u*8+1 .. u*8+8`.
  Mode loops generalise from `for (channel=1; channel<9; ++channel)` to
  `for (channel=1; channel<=N*8; ++channel)` with no other change.
- **The master slot is per-unit and mode-defined, NOT architecturally special.**
  It is *not* hardcoded to the Reaper master track — this is already true today:
  the active mode's `fader(channel=0, value)` decides what the master fader does
  (MultiTrackMode → Reaper master volume, PlugMode → plugin dry/wet,
  Send/Receive → selected-track volume). So each unit's master slot is simply a
  9th fader position whose meaning the mode assigns.
  - The `FIXID` macro in `csurf_mcu.h` is **dead code** (never expanded) —
    ignore it.
  - The only core-level master reference left is `Tracks::m_channelTracks[0] =
    CSurf_TrackFromID(0)` (the mixer-oriented default that makes channel 0 map
    to the Reaper master *in the mixer view*). That is benign and stays.
  - `OnFaderMove`'s `tid==8 → channel 0` is just the physical→logical fader
    mapping (9th fader = master slot), not a Reaper-master binding.
  - For N units: expose one master slot per unit; modes already decide meaning.
- **Offset is implicit:** unit position = `unitIndex * 8`; no manual offset
  field in the config dialog.
- **Bank scroll is bank-wide:** Channel Up/Down and track-selection-follow
  shift the entire `N*8` strip window together via `globalOffset` (Cockos-style,
  consistent with linked modes).

### 5.2 Per-unit capability model & global routing

Because the extension will be published, the config must cover the realistic
hardware space. Scope is narrowed to the **MCU protocol family only** — **HUI
(`0x05`) and Logic Control (`0x10/0x11`) are explicitly out** (HUI is a
different protocol; Logic Control was never supported and won't be). That
leaves device ids `0x14` (main) and `0x15` (extender) only, so the **device id
is fully determined by the main/extender role** and is no longer an independent
knob. Each unit is described by two independent capabilities:

| Capability | Values | Drives |
|---|---|---|
| MIDI-In / MIDI-Out | device id | the MIDI ports |
| Main role (transport + 7-seg) | yes / no | main ⇒ device id `0x14` + transport/SMPTE routing; extender ⇒ `0x15`, 8 strips only |
| Device model | `Mackie` / `QConProX` | a cluster of protocol quirks (VPOT-ring encoding, meter-bridge master path, LED-blink handling, assignment-display suppression) **and** #LCD-panels (Mackie⇒1, QConProX⇒2; the 2nd panel uses the `0x67 0x15` path). lcdPanels is **derived** from the model, not stored. |

The dialog offers **four named presets = the 2×2 grid** of {main, extender} ×
{Mackie, QConProX}: *Mackie Main*, *Mackie Extender*, *QCon ProX*, *QCon ProX
extender*. Internally we store the two capabilities (so adding a preset is
trivial), and every unit gets its own MIDI in/out. The ~20 existing
`CONFIG_FLAG_PROX` checks all become per-unit `unit.isProX()` (= `model==QConProX`).

**Two orthogonal axes — do not confuse them (corrected after review):**
- **Config position** of a unit ⇒ its **channel-strip order only** (pos 0 →
  ch 1-8, pos 1 → ch 9-16, …). Mirrors physical left-to-right.
- **Global routing** (transport LEDs, SMPTE/7-seg timecode, assignment display,
  automation-mode LEDs, flip/global-view, drop, save/undo, metronome) ⇒ sent to
  **every unit whose capabilities include "transport + 7-seg"**, regardless of
  its config position. There is **no "topmost = main" rule**. A unit with
  transport can sit at any position; you simply need ≥1 transport-capable unit.

### 5.3 Persistence

Two levels must not be confused:

**(a) Surface config (`configString`)** — Reaper-global, NOT in the project.
The format changes from the legacy 5-int string to `N × (in, out, isMain,
model) + flags`. Legacy 5-int strings parse as a single Mackie-Main unit
(backward-compat in WP-B).

**(b) Project state** — per-project, the `<MCU_KLINKE` XML chunk inside the
`.rpp` (GUID-keyed per track + global Options/PlugMaps/CommandMode/Actions).
- The only unit-count-sensitive persisted values are the TrackState fields
  `anchor` and `q_channel`, stored as **absolute surface slots `1..N*8`**
  (0 = none). They generalise with N: anchors pin a track to a fixed slot and
  the free-slot calc becomes `availableChannels() - numActiveAnchors()`
  (see §5.4). Everything else (CommandMode VPOT index 0-7, PlugMode
  slots/banks/pages, PlugMap param offsets) is relative-within-an-8-block or
  FX-param-relative ⇒ unit-independent.
- **Out-of-range handling is required, both on load and at runtime:** because N
  lives in the surface config (not the project) and the active set can shrink
  (§5.4), an anchor/q-channel beyond the current active range is simply
  **inactive/hidden, not lost** — it reappears when the range covers it again.
  Never clamp-and-destroy.
- Add a `version="1"` attribute to the XML root for future migration safety
  (the current format has none).
- No per-unit project state is needed for linked mode (one global mode);
  per-unit state only arises with future independent modes.
- **Caveat:** "only anchor/q_channel are unit-dependent" holds for *today's*
  code; when each mode is designed for N units (deferred), re-check whether new
  unit-dependent values get persisted.

### 5.4 Dynamic unit activation ("release the extenders")

**Requirement:** by pressing a fixed MCU button on the main unit, toggle a mode
in which the extension reacts to / sends to **only the main unit**, ignoring the
extenders — so the user can drive another app with the extender hardware
temporarily. Decisions:
- **Trigger:** a fixed MCU button (which one TBD — must be conflict-free).
- **Scope:** binary — "main only" vs "main + all extenders".
- **Persistence:** ephemeral / session (default on load = all units active).
- **Ports:** keep open (multi-client) + suppress I/O on inactive units (do not
  close/reopen).

**Architectural impact (cross-cutting):**
- `availableChannels()` becomes a **runtime** quantity = sum of *active* units'
  strips, used by `Tracks::adjust`, bank scroll, and the anchor free-slot calc.
- `HardwareUnit` gains an `active` flag (WP-A): inactive ⇒ ignore input events
  AND suppress all output (no LED/fader/display/meter to that unit). MIDI ports
  stay open.
- `numActiveAnchors()` counts only anchors whose slot is within the active
  range → with extenders released, only main-unit anchors count (matches the
  `availableChannels() - numActiveAnchors()` calc).
- On toggle: trigger a reflow (`adjust`) so the previously-on-extender channels
  collapse into the 8 main slots (main anchors stay fixed); the surface then
  behaves like an 8-channel surface until extenders are re-enabled.
- Touches WP-A (active flag + I/O suppression), WP-C (`availableChannels()` /
  `numActiveAnchors()` runtime), WP-D (bank/scroll uses runtime available).

## 6. Scaffolding work packages

Each package is a unit of design/implementation; open questions are called out.

### WP-A — `HardwareUnit` abstraction
Extract one physical MCU's hardware concerns out of `CSurf_MCU`:
- `midi_Output*`, `midi_Input*`, open/close, the JACK/`usleep` workaround.
- `MCUReset()` / the `F0 00 00 66 dd …` SysEx, where `dd` is the unit's
  configured device id (`0x14` or `0x15`).
- `SendMidi()` / `SendMsg()` routing.
- Per-unit addressing of the 9 faders (`0xE0..0xE8`), the 8 VPOT LEDs, the
  center LCD (0-2 panels), and the 8 select/mute/solo/recarm LEDs.
- Resolved: each unit has a fader-only master slot (`0xE8`); its meaning is
  mode-defined (see §5.1). A unit whose hardware has no physical master fader
  simply leaves that slot inert.
- `active` flag for dynamic unit activation (§5.4): inactive ⇒ ignore input
  events and suppress all output; MIDI ports stay open.

### WP-B — Config dialog + config string
- Move from the 5-int config string to a structure that carries, per unit:
  (MIDI-In, MIDI-Out, isMain {bool}, DeviceModel {Mackie|QConProX}). Device id
  (main⇒0x14, extender⇒0x15) and #LCD-panels (QConProX⇒2 else 1) are derived.
  Backward-compatible parse of legacy single-unit strings.
- Dialog (`res.rc` / `res_linux.cpp` / `dlgProc`) gains a repeatable unit row
  whose type is one of the four presets (Mackie Main / Extender / QCon ProX /
  QCon ProX extender). Unit row order = physical left-to-right (§5.2).
- No manual offset field (implicit = index × 8). No “main = unit 0” rule —
  transport routing is by capability, not position.

### WP-C — `Tracks` singleton refactor (the big one)
Remove the single `m_pMCU` coupling; parameterise for `totalChannels = N*8`:
- `m_channelTracks` vector, `getMediaTrackForChannel` / `getChannelForMediaTrack`
  operate over `N*8 + master`.
- `globalOffset` semantics (see WP-D).
- Anchors & quick-jump generalise to `N*8` slots: free-slot calc becomes
  `availableChannels() - numActiveAnchors()` where `availableChannels()` is a
  RUNTIME quantity (sum of active units' strips, §5.4) — not static `N*8`. The
  `8 - numAnchors` hardcode (Tracks.cpp:448) must go.
- VU-active, the `TrackStatesTableComponent` hack.
- `FIXID` macro in `csurf_mcu.h` is **dead code** — delete it, don't adapt it.

### WP-D — Offset / bank / scroll behaviour
With N units fixed-mapped to consecutive 8-channel blocks, define what
Channel-Up/Down and track-selection-follow do:
- **Candidate:** the whole N×8 bank scrolls together (matches Cockos original
  and linked modes). globalOffset then shifts the entire bank.
- Bank size = runtime `availableChannels()` (§5.4): shrinks to 8 when extenders
  are released, grows back when re-enabled.
- **Open:** confirm “scroll the whole bank together” is the desired default.

### WP-E — Global-display / transport routing
Global hardware (transport LEDs, SMPTE/beats + assignment 7-seg, automation-mode
LEDs, flip/global-view, drop, save/undo, metronome) is routed to **every
transport-capable unit** (§5.2), not to a single "main". Mirrors the
`!m_is_mcuex` gating in the Cockos original but keyed on the per-unit
"transport + 7-seg" capability instead of a fixed main slot.

### WP-F — `CCSManager` / `VPOT_LED` sizing
`VPOT_LED[9]` → sized for `N*8` (+ master handling). `ButtonManager` channel
routing (select/mute/solo/recarm/VPOT/fader) extended to `N*8`. Touch-state maps
already keyed by `MediaTrack*`, so mostly fine; verify fader/pan lastpos caches.

## 7. Per-mode design (AFTER scaffolding) — TBD

One design doc each, in this suggested order (easiest → hardest):
1. **MultiTrackMode** (mixer) — pilot; extender = next 8 channels.
2. **Send/Receive** — more channels' sends, or more sends per channel.
3. **CommandMode** (actions) — more action banks, or mirrored.
4. **PlugMode** (FX) — hardest: banks/pages/PlugMap/PlugAccess across units.

Open question for each: does the extender (a) extend the same view (more
channels / more params) or (b) mirror the main? Linked-mode default ⇒ (a).

## 8. Resolved channel-model questions (kickoff Q&A)

1. **Offset field** → dropped; position implicit = `unitIndex * 8`.
2. **Master fader** → one per unit, but **mode-defined** (not architecturally
   the Reaper master). In linked mode all master slots coincide.
3. **Bank scroll** → the whole `N*8` bank scrolls together (Cockos-style).

4. **Unit config** → general capability model (device id, #LCD-panels,
   has-transport, MIDI in/out) per unit, NOT a fixed type enum — the extension
   is published and must support every hardware combination. `EnumMCUType`
   survives only as dialog presets.
5. **Position vs. routing** → orthogonal. Config position = channel order only;
   global routing goes to all transport-capable units regardless of position.
   (No "topmost unit is the main" rule.)

Next open thread: confirm the capability enumeration is complete, then start
the detailed design of WP-A (`HardwareUnit`).
