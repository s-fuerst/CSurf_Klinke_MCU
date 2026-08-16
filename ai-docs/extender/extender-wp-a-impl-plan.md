# WP-A — `HardwareUnit` Abstraction — Implementation Plan

> Implementation plan for work package WP-A of the extender-support effort.
> Design authority: `ai-docs/extender-wp-a-hardware-unit.md` (§9 = resolved
> decisions 2026-07-07). Master plan: `ai-docs/extender-support.md` (§5–§6).
>
> **This is a plan, not code.** No source changes have been made. All file
> paths and line numbers below are against the current tree
> (`src/core/csurf_mcu.{h,cpp}`, `src/core/CCSManager.{h,cpp}`,
> `src/hardware/*`, `src/csurf_main.cpp`, `CMakeLists.txt`).
>
> **Revised 2026-07-08** per `ai-docs/extender-wp-a-critical-review.md`
> (9 findings). Headline changes: the N-generic promise is tightened to an
> honest **N=1 extraction milestone** (hard `numUnits()==1` invariant; N>1
> needs WP-C+WP-F+per-mode, not just WP-B); legacy `GetOffset/GetSize/
> g_mcu_list` are **retained as shims** through WP-A (still used by
> `MultiTrackMode` + `Tracks`); the `DisplayHandler` send path is made
> **per-unit** (not via the `CSurf_MCU::SendMsg` shim); `OnRotaryEncoderPush`
> is moved to the **per-unit strip path**; the display-switching surface
> covers **all** `switchTo` callsites; and the build/verify snippet path is
> fixed. Inline marks **(CR-Fx)** point at each change.

## Goal

Extract one physical MCU unit's hardware concerns (MIDI I/O, SysEx reset, raw
send, per-strip LED + fader state, per-unit `DisplayHandler`, MIDI-in parsing)
out of `CSurf_MCU` into a new `HardwareUnit` class, so that `CSurf_MCU` owns
`N` units and becomes the single *logical* surface. **WP-A is an N=1
extraction milestone (CR-F1):** exactly ONE main `HardwareUnit` is constructed
from the legacy 5-int config string, and the surface behaves identically to
today. The *translation plumbing* (`unitForChannel`, `localOf`,
`broadcastMasterFader`, master-slot-as-channel-0) is written N-generic, but
the N>1 *consumers* are deliberately NOT widened here — `CCSManager` channel
bounds, `VPOT_LED[9]`, `MeterBridge` routing, `ButtonManager` per-unit state,
and mode ProX-row logic all stay 1..8 / global (see WP boundary). A hard
**`numUnits()==1` invariant** guards WP-A, and
`ASSERT(globalChannel <= availableChannels())` documents the N>1 edge.
Enabling N>1 is a later, separate milestone (WP-B → WP-C → WP-F → per-mode →
hardware test), **not** something WP-B alone can switch on. At the end of
every step the project builds clean and behaves identically to today for
`N=1`.

---

## Golden thread (read before any step)

**`CSurf_MCU` keeps `SetLED`, `SendMidi`, `SendMsg`, `GetMidiOutput`,
`GetDisplayHandler`, `OnMIDIEvent` as forwarding shims** that route to the
unit(s). This is what makes the migration safe: every existing caller
(`CCSManager`, `Transport`, `ButtonManager`, `VPOT_LED`, `MeterBridge`, the
modes) already calls these CSurf_MCU methods, so they need NOT change in the
early steps. Only the *bodies* of the shims change (direct member → unit
routing). The first step that touches callers is Step 7 (display construction)
and Step 8 (dead-scaffolding).

**Strip vs. global LED notes** (drives the `SetLED` router in Step 4):
- Strip notes `0x00`–`0x1F` (rec `0x00-07`, solo `0x08-0F`, mute `0x10-17`,
  select `0x18-1F`) → route to the unit owning that channel.
- Global notes `0x28`+ (assignment, transport, F-keys, modifiers, automode,
  drop, save/undo, metronome, flip/GV, zoom/scrub, name/value, smpte/beats)
  → broadcast to all main units.
For `N=1` both paths hit unit 0 → identity.

---

## Build / verify contract (every step)

```bash
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)")
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```
**(CR-F9)** The `cd build` is wrapped in a subshell so the `cp` still
resolves `build/reaper…so` relative to the repo root — without the subshell,
a bare `cd build && …` followed by `cp build/…so` would look for
`build/build/reaper…so`.
Then fully restart REAPER, add/select **"Mackie Control Protocol (Klinke)"**,
and eyeball the `N=1` regression (see checkpoints). `MCU_DEBUG_LOG` is ON by
default — grep `mcu_klinke_debug.log` for anomalies. Windows/macOS builds are
not required for WP-A verification but must still compile (the new files are
platform-agnostic; the `_WIN32` `usleep`/`Sleep` split is preserved).

---

## Steps

### Step 1 — Add the new types (compile-only, no wiring)
**Goal:** introduce `HardwareUnit`, `UnitConfig`, `DeviceModel`, `MultiDisplay`,
and the input-event listener interface as compiling-but-unused stubs.
- **New files:**
  - `src/hardware/HardwareUnit.h` — `enum DeviceModel`, `struct UnitConfig`,
    `class HardwareUnit` (members + the §3 outgoing-local / §4 incoming-global
    interface decls), `class HardwareEventListener` (the listener `CSurf_MCU`
    implements; see skeletons below).
  - `src/hardware/HardwareUnit.cpp` — empty/stub implementations.
  - `src/hardware/display/MultiDisplay.h` / `.cpp` — composite `Display`
    subclass (skeleton).
- **Changes:** none to existing files. CMake auto-globs `.cpp`
  (`file(GLOB_RECURSE MCU_SOURCES CONFIGURE_DEPENDS src/*.cpp)`, CMakeLists
  ~"Source files"); include dirs `src/hardware` and `src/hardware/display` are
  already in `target_include_directories`. **No CMakeLists edit needed.**
- **Verify:** clean build; new TUs compile.
- **N=1 checkpoint:** nothing wired → identical behavior.

### Step 2 — `HardwareUnit` owns MIDI I/O + reset + raw send
**Goal:** MIDI open/close, the JACK `usleep(200ms)` workaround, `MCUReset`
SysEx, and raw `SendMidi`/`SendMsg` move into `HardwareUnit`; `CSurf_MCU`
holds one unit and forwards.
- **Files:** `src/core/csurf_mcu.{h,cpp}`, `src/hardware/HardwareUnit.{h,cpp}`.
- **Move (ctor `csurf_mcu.cpp:830-873`):** `CreateMIDIInput`/`CreateMIDIOutput`
  (+`CreateThreadedMIDIOutput`), the `usleep(200000)` block (858-867),
  `MCUReset()` (244-290) SysEx/handshake part, `m_midiin->start()` (872-873)
  → `HardwareUnit` ctor. The per-device-id reset byte
  (`m_is_mcuex ? 0x15 : 0x14`, used in `OnMCUReset:306` + dtor:907) becomes the
  unit's derived `m_deviceId` (`isMain?0x14:0x15`).
- **CSurf_MCU ctor:** build `UnitConfig{indev, outdev, isMain=true,
  model=IsFlagSet(CONFIG_FLAG_PROX)?QConProX:Mackie}` from the parsed parms
  (`isMain=true` because the extender reg is unregistered today), construct
  ONE `HardwareUnit`, store in `std::vector<HardwareUnit*> m_units` (or
  `WDL_PtrList`). Remove members `m_midiout`, `m_midiin`, `m_midi_in_dev`,
  `m_midi_out_dev` (keep `m_midi_in_dev`/`m_midi_out_dev` only if needed for
  `GetDescString`/`GetConfigString`; otherwise read from `m_units[0]->cfg()`).
- **Shims:** `CSurf_MCU::SendMidi` (1225) / `SendMsg` (1437) →
  `m_units[0]->sendMidi/sendMsg`. `GetMidiOutput()` →
  `m_units[0]->midiOutput()`.
- **Direct `m_midiout->Send()` callsites → `SendMidi()` shim:**
  `SetPlayState` (1303-1316), `SetRepeatState` (1311-1317), `OnZoom` (645-655),
  `OnScrub` (657-663), `ClearSaveLed` (609-612), `OnSave` (614-624),
  `ClearUndoLed` (626-629), `OnUndo` (631-638), the `Run()` SMPTE/7-seg block
  (~1057-1170), and the dtor goodbye/meter-off loops (894-940).
- **`MCUReset` orchestrator** stays on `CSurf_MCU`: calls `m_units[0]->reset()`
  then the global-reset bits (`m_dropstate`, flip/GV LEDs, zoom/scrub LEDs,
  channel-display digits `0xB0 0x40+10/11`, splash). The global bits route via
  the main unit (N=1: unit 0).
- **Verify:** build; REAPER: splash + reset handshake, transport/timecode work.
- **N=1 checkpoint:** faders/VPOT/LEDs/meters/display all behave as before.

### Step 3 — `HardwareUnit` owns `DisplayHandler`; per-unit `isProX`
**Goal:** the `DisplayHandler` becomes per-unit; the global `CONFIG_FLAG_PROX`
flag is replaced by per-unit `isProX()` on the display path.
- **Files:** `src/hardware/HardwareUnit.{h,cpp}`,
  `src/hardware/display/DisplayHandler.{h,cpp}`, `src/core/csurf_mcu.{h,cpp}`.
- **Move:** `m_pDisplayHandler` (`csurf_mcu.h:296`) → `HardwareUnit::m_display`,
  constructed in the unit ctor with `EnumMCUType` derived from `UnitConfig`
  (`isMain?MCU:MCU_EX`; PROX is signaled by model, see below).
- **`DisplayHandler` per-unit ProX + per-unit send (CR-F3):** add `bool
  m_isProX` (ctor param from the owning unit's `model==QConProX`). Replace
  `m_pMCU->IsFlagSet(CONFIG_FLAG_PROX)` in `sendToHardware`
  (`DisplayHandler.cpp:134`) with `m_isProX`. **The send path must be
  per-unit, NOT via `CSurf_MCU::SendMsg`:** if `DisplayHandler` kept calling
  the `CSurf_MCU::SendMsg` shim (which routes to `m_units[0]`), every
  per-unit display would emit its SysEx on unit 0's port — defeating the
  whole point of a per-unit handler. So **drop `DisplayHandler::m_pMCU`** and
  give it a back-pointer to its owning `HardwareUnit*` (or a tiny `MidiSender`
  interface); `sendToHardware`/`enableMCUMeter` call `m_pUnit->sendMsg(…)`
  directly. The `m_isProX` ctor param was the last ProX reason to touch
  `m_pMCU`, so this completes the decoupling. (`CSurf_MCU::GetDisplayHandler()`
  remains as a shim returning `m_units[0]->displayHandler()`.)
- **Shim:** `CSurf_MCU::GetDisplayHandler()` → `m_units[0]->displayHandler()`.
  `m_pSplashDisplay`/`m_pActionDisplay` still construct via
  `GetDisplayHandler()` (returns the unit's). The
  `getDisplayHandler()->getDisplay()->resendAllRows()` callsite in `Run()`
  (1168) stays (N=1: unit 0's active display).
- **Verify:** build; REAPER: LCD renders, ProX 2nd panel still works when the
  ProX flag is set, Mackie 1-panel still works without it.
- **N=1 checkpoint:** display byte-identical; the ProX flag and unit
  `isProX()` agree for the single unit.

### Step 4 — `HardwareUnit` owns per-strip LED state + fader cache; `SetLED` becomes a router
**Goal:** the LED state cache, `EmulateBlinkingLEDs`, the fader-position cache,
and the CCSManager dedup caches move into `HardwareUnit`; `CSurf_MCU::SetLED`
routes strip vs. global notes.
- **Move into `HardwareUnit`:** `m_led_state[128]` + `SetLED` + the ProX quirk
  (`LED_BLINK_BYPASSED→LED_ON` when `!isProX`, `csurf_mcu.cpp:1232-1239`) +
  `EmulateBlinkingLEDs` (1241-1269); `m_faderPos[9]` (the per-strip fader
  dedup currently in `CCSManager`, `CCSManager.h` private members).
- **`CSurf_MCU::SetLED(button_nr, state)` → router:** strip notes
  `0x00-0x1F` → unit owning that channel; global notes `0x28+` → broadcast to
  all main units. (Helper `unitForStripNote(nr)` / `isGlobalNote(nr)`.)
- **`CCSManager` changes:** remove `m_stateRec[9]`, `m_stateSolo[9]`,
  `m_stateMute[9]`, `m_stateSelect[9]`, `m_faderPos[9]` (CCSManager.h private).
  `setRecLED/setSoloLED/setMuteLED/setSelectLED` (CCSManager.cpp:450-501) drop
  their local dedup and just call `m_pMCU->SetLED(note, state)` — the unit's
  `m_led_state` dedup replaces them (identical effect).
  `setFader` (CCSManager.cpp:433-447) → `m_pMCU->sendStripFader(channel,
  value)` (new CSurf_MCU method; channel 0 → master broadcast, Step 6;
  1..N*8 → unit). `getFaderPos` → route to the unit's cache.
- **`Run()` blink call:** `EmulateBlinkingLEDs(now)` (Run ~1010) → iterate
  units; each blinks its own LEDs.
- **Verify:** build; REAPER: strip LEDs (select/mute/solo/rec), flip/GV/global
  LEDs, blinking LEDs (ProX + emulate-blinking flag) all behave as before.
- **N=1 checkpoint:** every LED transaction identical (router = identity).

### Step 5 — Input parsing moves into `HardwareUnit` (emits global events)
**Goal:** the per-strip MIDI-in parsers move into `HardwareUnit` and emit
GLOBAL channel events; global parsers stay in `CSurf_MCU`, emitted by main
units.
- **Files:** `src/core/csurf_mcu.{h,cpp}`, `src/hardware/HardwareUnit.{h,cpp}`,
  `src/hardware/ButtonManager.{h,cpp}` (ButtonManager moves to be owned by /
  or reference the unit — see risk R5).
- **Move to `HardwareUnit` (per-strip, emit global via listener):**
  `OnFaderMove` (316), `OnRotaryEncoder` (331), `OnRotaryEncoderPush` (481,
  **CR-F4** — the VPOT-push range `0x20..0x27` is per-strip, NOT global),
  `OnTouch` (693),
  `OnVPOTAssign` (348), `OnChannelSelect`/`DC`/`Long` (515-526),
  `OnRecArm`/`DC` (491-499), `OnMute` (501), `OnSolo`/`DC` (506-513). The unit
  adds `m_unitIndex*8`; for N=1 global==local.
- **Stay in `CSurf_MCU` (global, main-unit-only):** `OnJogWheel`, `OnTransport`
  /`DC`, `OnKeyModifier`, `OnFunctionKey`, `OnMarker`, `OnNudge`, `OnCycle`,
  `OnClick`, `OnSave`, `OnUndo`, `OnCancel`, `OnZoom`, `OnScrub`, `OnFlip`,
  `OnGlobal`, `OnScroll`, `OnGlobalViewKeys`, `OnGlobalSoloButton`,
  `OnDropButton`, `OnPedalMove`, `OnSMPTEBeats`, `OnAutoMode`, `OnBankChannel`,
  `OnNameValue`/`DC`, `ResetAllFaderTouch`,
  `OpenFXFavorite`, `OnMCUReset`.
- **`Run()` MIDI read (1174-1199):** iterate units; each unit does
  `SwapBufs`/`GetReadBuf`/`EnumItems`, runs its own strip parsers, and forwards
  unrecognized/global messages to `CSurf_MCU` (e.g.
  `surface->onGlobalMidiEvent(evt)`). The `OnMIDIEvent` dispatch table
  (800-807) splits accordingly.
- **Verify:** build; REAPER: move faders/VPOTs, press select/mute/solo/rec,
  touch faders — all map to the right channels; jog/transport/F-keys/modifiers
  still work.
- **N=1 checkpoint:** every input event identical.

### Step 6 — Global↔local translation + master broadcast
**Goal:** add the translation helpers and wire the master-slot broadcast.
- **Files:** `src/core/csurf_mcu.{h,cpp}`.
- **Add:** `int numUnits()`, `HardwareUnit* unitForChannel(int g)` (=
  `m_units[(g-1)/8]`), `int localOf(int g)` (=`(g-1)%8+1`),
  `void broadcastMasterFader(int value)` (→ all units `setMasterFader`),
  `int availableChannels()` (= `numUnits()*8`; the runtime/active variant is
  WP-C, here it is just `numUnits()*8`).
- **Master:** `setFader(0)` (CCSManager → CSurf_MCU::sendStripFader(0))
  → `broadcastMasterFader`. A unit's `masterFaderMoved(unit, value)` → reported
  to the active mode as `fader(channel=0)` (linked-mode broadcast, §9.2).
- **`Run()`:** `Tracks::adjust(g_mcu_list.GetSize()*8)` (Run:1014) →
  `Tracks::adjust(availableChannels())`. (Still 8 for N=1.)
- **Verify:** build; REAPER: master fader moves all + reflects Reaper master.
- **N=1 checkpoint:** master fader behavior identical.

### Step 7 — `MultiDisplay` composite + factory; rewire mode construction
**Goal:** the mode-facing `Display` becomes a composite spanning the N units'
displays; modes switch via the broadcast path.
- **Files:** `src/hardware/display/MultiDisplay.{h,cpp}`,
  `src/core/CCSManager.{h,cpp}`, and the mode ctors that build a `Display`
  (see below).
- **`MultiDisplay` (IS-A `Display`):** holds `Display* m_children[N]` (one per
  unit's `DisplayHandler`). Override `changeField(row, globalField, text)` →
  `child[(field-1)/8]->changeField(row, (field-1)%8+1, text)`; rows 2-3 silently
  dropped on non-ProX children. Broadcast `changeText`/`changeTextFullLine`/
  `clear`/`resendAllRows`/`activate` to all children.
- **Display-switching service (CR-F6):** introduce ONE legal entry point —
  `CSurf_MCU::switchActiveDisplay(Display*)` (or extend
  `CCSManager::switchToDisplay`) — that every display switch goes through.
  **Rule: `DisplayHandler::switchTo(<MultiDisplay>)` is INVALID.**
  `sendDifferences` early-returns unless `pDisplay == m_pActualDisplay`
  (`DisplayHandler.cpp:51`), and a handler's `m_pActualDisplay` must always be
  a real per-unit child, never the composite. The service branches: if the
  target is a `MultiDisplay`, call its `switchToAll()` (which, per child `i`,
  does `child[i]->handler()->switchTo(child[i])` so each unit's handler has
  its own child active); else fall back to the legacy single
  `GetDisplayHandler()->switchTo(pDisplay)`.
- **Factory:** `Display* createMultiDisplay(CCSManager* mgr, int numRows)`
  builds the composite from `mgr->getMCU()->units()`; one child per unit's
  `DisplayHandler`.
- **Rewire ALL display constructors** (`new Display(getDisplayHandler(), n)`
  → `createMultiDisplay(this, n)`): `MultiTrackMode.cpp:23`, `PanMode.cpp:~44`,
  `SendReceiveModeBase.cpp:17`, `CommandMode.cpp:~149`, `PlugMode.cpp:36`,
  `PerformanceMode.cpp:13`, `MultiTrackSelector.cpp:25`,
  `PlugModeSelectors.cpp:15`.
- **Rewire ALL `switchTo` callsites through the service (CR-F6, complete list
  — the earlier draft missed several):** mode-owned: `CommandMode.cpp:149` +
  `CommandMode.h:105`, `MultiTrackMode.cpp:61`, `SendReceiveModeBase.cpp:40`,
  `PanMode.cpp:44`, `PerformanceMode.cpp:20`, `MultiTrackSelector.cpp:25`,
  `PlugModeSelectors.cpp:15`, `Options.cpp:45`; CSurf_MCU-owned overlays:
  `csurf_mcu.cpp:287` (splash), `csurf_mcu.cpp:680` (action-display modifier
  overlay), `ActionsDisplay.cpp:27` + `:33`. (Splash / action-display become
  `MultiDisplay`s too, or are accepted as main-unit-only overlays — decide in
  Step 7; both must go through the service, never a raw `switchTo`.) Field
  range stays 1..8 for N=1.
- **`Run()` resend (1168):** `getDisplayHandler()->getDisplay()->resendAllRows()`
  → iterate units, each `displayHandler()->getDisplay()->resendAllRows()`
  (N=1: single).
- **Verify:** build; REAPER: switch modes (Pan/Send/Receive/Command/Plug),
  open editors, verify each mode's LCD fields render in the right place.
- **N=1 checkpoint:** display rendering byte-identical (composite has 1 child).

### Step 8 — Dead-scaffolding removal
**Goal:** delete the dormant extender scaffolding and route all remaining
`!m_is_mcuex` gating through "main units".
- **Delete now (truly dead / internally-only):** the `FIXID` macro
  (`csurf_mcu.h:~97`, confirmed dead — never expanded), the `EXT_ID` macro
  (`csurf_mcu.h:19`; keep `MAIN_ID`), `csurf_mcuex_modified_reg`
  (`csurf_mcu.cpp:1755`), the commented `extern`/`Register` mcuex lines in
  `csurf_main.cpp`, and the "only a single instance can be used" `MessageBox`
  block (`csurf_mcu.cpp:1571-1577`). ctor signature `ismcuex`/`offset`/`size`
  params collapse (`createFunc` builds the unit from `UnitConfig` instead).
- **Keep as shims until WP-C/WP-F (CR-F2):** `g_mcu_list`,
  `m_is_mcuex`/`m_offset`/`m_size`, and `IsExtender()`/`GetOffset()`/
  `GetSize()`/`GetNumMCUs()` are still referenced **outside** `CSurf_MCU`:
  `MultiTrackMode.cpp:217,230,241-245` (`GetSize()` + iterates `g_mcu_list` +
  `GetOffset()`), and `Tracks.cpp:625` (`m_pMCU->GetOffset()`). Deleting them
  now **breaks compilation**. Instead: ctor stores `m_is_mcuex=false`,
  `m_offset=0`, `m_size=8`; `GetOffset()` returns `0`; `GetSize()` returns
  `availableChannels()` (=8); `g_mcu_list` stays a 1-element list (or its
  `MultiTrackMode` uses are replaced inline). These symbols go to zero only
  when WP-C (`Tracks`) and WP-F (`MultiTrackMode` channel logic) no longer
  depend on them.
- **`!m_is_mcuex` gating:** unchanged for WP-A — `m_is_mcuex` stays `false`
  (the CR-F2 shim), so `MCUReset` (263-280), `SetPlayState` (1309),
  `SetRepeatState` (1316), the `Run()` SMPTE block (~1057), and
  `CCSManager::setAssignmentDisplay` (`!IsExtender()`, CCSManager.cpp:519)
  keep routing through the (single, main) unit exactly as today. Converting
  them to `for-each main-unit` is WP-E.
- **`createFunc`/`parseParms`:** keep parsing the legacy 5-int string (WP-B
  will replace the format), construct 1 main unit. The multi-instance guard
  is moot (the surface registers once via `csurf_mcu_modified_reg`); the
  `MessageBox` block, `csurf_mcuex_modified_reg`, and the `csurf_main.cpp`
  dead comments are the ones listed under "Delete now" above.
- **`csurf_main.cpp`:** the commented `extern ... csurf_mcuex_modified_reg`
  (line 12) and the commented `Register("csurf", &csurf_mcuex_modified_reg)`
  (line ~register) — delete the dead comments.
- **Verify:** build; full N=1 regression; `git grep FIXID` returns nothing
  (the only macro actually deleted in WP-A). `git grep m_is_mcuex` /
  `git grep g_mcu_list` / `git grep GetOffset` still hit the **retained shim**
  sites (documented above); they go to zero only in WP-C/WP-F.
- **N=1 checkpoint:** identical behavior; the surface still registers and runs
  as "Mackie Control Protocol (Klinke)".

---

## New-type reference (code skeletons)

> Sketches matching codebase style (`m_` prefix, 2-space indent,
> `safe_call`/`safe_delete`). Final signatures may shift during implementation.

### `src/hardware/HardwareUnit.h`
```cpp
#ifndef MCU_HARDWARE_UNIT
#define MCU_HARDWARE_UNIT
#include "csurf.h"
class DisplayHandler;
class VPOT_LED;
class CSurf_MCU;
struct MIDI_event_t;

enum DeviceModel { Mackie, QConProX };   // QConProX => 2 LCD panels + VPOT/meter/blink/assignment quirks

struct UnitConfig {
    int         midiInDev;
    int         midiOutDev;
    bool        isMain;      // main => devId 0x14 + transport; extender => 0x15
    DeviceModel model;       // isProX() == (model == QConProX); lcdPanels = isProX()?2:1
};

// Input-side listener (CSurf_MCU implements). HardwareUnit emits GLOBAL
// channel indices (1..N*8) by adding its own m_unitIndex*8 offset.
class HardwareEventListener {
public:
  virtual void stripFaderMoved (int globalChannel, int value) = 0;
  virtual void masterFaderMoved(int unitIndex, int value) = 0; // per-unit slot; surface broadcasts as fader(0)
  virtual void vpotMoved       (int globalChannel, int delta) = 0;
  virtual void vpotPressed     (int globalChannel, bool pressed) = 0;
  virtual void stripButton     (int button /*rec/solo/mute/select id*/, int globalChannel,
                                bool pressed, bool doubleClick, bool longPress) = 0;
  // global (transport/F-keys/modifiers/...) forwarded as raw MIDI for CSurf_MCU to parse:
  virtual void globalMidiEvent (MIDI_event_t *evt) = 0;
};

class HardwareUnit {
public:
  HardwareUnit(int unitIndex, const UnitConfig &cfg, CSurf_MCU *pMCU, int *errStats);
  ~HardwareUnit();

  bool isMain() const { return m_cfg.isMain; }
  bool isProX() const { return m_cfg.model == QConProX; }
  int  unitIndex() const { return m_unitIndex; }
  int  stripBase() const { return m_unitIndex * 8; }      // first global channel (1-based) => stripBase+1
  const UnitConfig &cfg() const { return m_cfg; }

  midi_Output   *midiOutput() { return m_midiout; }
  midi_Input    *midiInput()  { return m_midiin; }
  DisplayHandler *displayHandler() { return m_display; }

  void setListener(HardwareEventListener *l) { m_pListener = l; }

  // --- outgoing, LOCAL indices (CSurf_MCU translates global->local first) ---
  void sendStripFader(int local, int value);   // 0xE0 + local  (local 0..7)
  void setMasterFader(int value);              // 0xE8
  void setStripLED   (int note, int state);    // note 0x00-0x1F (rec/solo/mute/select)
  void setGlobalLED  (int buttonId, int state);// 0x28+ ; no-op-equivalent routing on extender
  void sendMidi      (unsigned char status, unsigned char d1, unsigned char d2, int frame_offset);
  void sendMsg       (MIDI_event_t *msg, int frame_offset);
  void reset();                                // F0 00 00 66 <devId> 09 ... per-unit
  void emulateBlinkingLEDs(DWORD now);         // per-unit m_led_state[128]
  void SetLED        (int button_nr, int state); // per-unit dedup + ProX quirk

  // --- incoming: read m_midiin, parse strip msgs (emit global), forward global ---
  void pollMidiInput(DWORD now);               // SwapBufs/GetReadBuf/EnumItems -> dispatch
  bool onMCUReset   (MIDI_event_t *evt);       // host handshake (per devId)

private:
  int             m_unitIndex;
  UnitConfig      m_cfg;
  unsigned char   m_deviceId;       // isMain ? 0x14 : 0x15
  midi_Output    *m_midiout;
  midi_Input     *m_midiin;
  DisplayHandler *m_display;        // THIS unit's center LCD(s)

  int  m_led_state[128];            // per-unit LED dedup
  int  m_faderPos[9];               // local 0..7 + [8]=master

  CSurf_MCU              *m_pMCU;        // for legacy global code paths (shim)
  HardwareEventListener  *m_pListener;
};
#endif
```

### `src/hardware/display/MultiDisplay.h`
```cpp
#ifndef MCU_MULTI_DISPLAY
#define MCU_MULTI_DISPLAY
#include "Display.h"
#include <vector>

// Composite Display spanning N units. IS-A Display so modes hold it unchanged.
// changeField(row, globalField 1..N*8) routes to the owning unit's child.
class MultiDisplay : public Display {
public:
  MultiDisplay(/* per-unit child Display*[] + their handlers */);
  ~MultiDisplay();

  void addChild(Display *child);     // child is owned by its unit's DisplayHandler

  void changeField(int row, int field, const char *text, bool centered = false) override;
  void changeText(int row, int pos, const char *text, int pad, bool centered = false) override;
  void changeTextFullLine(int row, const char *text, bool centered = false) override;
  void clearLine(int row) override;
  void clear() override;
  void activate() override;          // = resendAllRows broadcast
  void resendAllRows() override;
  void resendRow(int iRow) override;

  // Per-unit switchTo: each child switched on ITS OWN handler so its
  // DisplayHandler::m_pActualDisplay == child (sendDifferences check passes).
  void switchToAll();

private:
  std::vector<Display*> m_children;  // one per unit
};
#endif
```

### `CSurf_MCU` new members (added; old MIDI members removed in Step 2)
```cpp
std::vector<HardwareUnit*> m_units;   // size 1 in WP-A (hardcoded main unit)
HardwareUnit *unitForChannel(int g);  // m_units[(g-1)/8]
static int localOf(int g);            // (g-1)%8 + 1
int numUnits() const;                 // m_units.size()
int availableChannels() const;        // numUnits()*8  (runtime-active variant = WP-C)
void broadcastMasterFader(int value); // all units setMasterFader
void sendStripFader(int channel, int value); // channel 0 -> broadcastMasterFader
// SetLED/SendMidi/SendMsg/GetDisplayHandler/GetMidiOutput stay as shims (rerouted bodies)
```

---

## Extraction map (what moves where)

| Today (CSurf_MCU) | Line | Moves to `HardwareUnit` | Transition shim (N=1) |
|---|---|---|---|
| `m_midiout`, `m_midiin`, open/close | ctor 830-832, dtor 949-950, `CloseNoReset` 972-977 | owned raw ports | `GetMidiOutput()`→unit; `SendMidi/SendMsg` forward |
| JACK `usleep(200000)` workaround | ctor 858-867 | unit ctor (after open) | identical timing |
| `MCUReset()` SysEx/handshake | 244-290, `OnMCUReset` 301-314 | `reset()`/`onMCUReset()` per devId | `CSurf_MCU::MCUReset` orchestrates |
| `SendMidi`/`SendMsg` | 1225, 1437 | unit raw send | shim forwards to unit 0 |
| `m_led_state[128]`+`SetLED`+ProX quirk | 1231-1239, member | per-unit LED dedup | `SetLED` router → unit |
| `EmulateBlinkingLEDs` | 1241-1269 | per-unit | `Run` iterates units |
| `m_pDisplayHandler` | h:296 | `m_display` per unit | `GetDisplayHandler()`→unit 0 |
| per-strip input parsers | `OnFaderMove` 316 … `OnSolo` 506-513 | unit emits global events | N=1 global==local |
| `m_is_mcuex`/`m_offset`/`m_size` | members | `m_unitIndex`+`UnitConfig` | removed Step 8 |
| transport/SMPTE/assignment/global-LED | `SetPlayState` 1303, SMPTE Run ~1057, etc. | **stays in CSurf_MCU** | routed to main units |

---

## Dead-scaffolding table (Step 8)

| Symbol | Location | Fate |
|---|---|---|
| `g_mcu_list` | h:46, ctor:849, dtor:967, Run:1014 | **shim retained** (CR-F2): kept 1-elem; deleted in WP-F after `MultiTrackMode` stops iterating it |
| `m_is_mcuex`, `m_offset`, `m_size` | h members | **shim retained** (CR-F2): `m_is_mcuex=false`, `m_offset=0`, `m_size=8`; deleted in WP-C |
| `IsExtender()`, `GetOffset()`, `GetSize()`, `GetNumMCUs()` | h methods | **shim retained** (CR-F2): `GetOffset()→0`, `GetSize()→availableChannels()`; deleted in WP-C/WP-F |
| `FIXID` macro | h:~97 | deleted (confirmed dead, never expanded) |
| `EXT_ID` macro | h:19 | deleted (keep `MAIN_ID`) |
| `csurf_mcuex_modified_reg` | cpp:1755 | deleted |
| "only a single instance" `MessageBox` | cpp:1571-1577 | deleted |
| commented `extern`/`Register` mcuex | csurf_main.cpp:12 + register line | dead comments deleted |

---

## WP boundary (in-scope vs deferred)

**In-scope for WP-A:**
- `HardwareUnit` + `UnitConfig` + `DeviceModel` + `MultiDisplay` + the listener.
- MIDI I/O, SysEx reset, raw send, per-unit `DisplayHandler` (with **per-unit
  send** + `isProX`, CR-F3), per-strip LED + fader caches, MIDI-in parsing
  (incl. `OnRotaryEncoderPush`, CR-F4) — all extracted per-unit.
- Global↔local *translation plumbing*, master broadcast, `SetLED` strip/global
  routing — written N-generic.
- Deletion of the truly-dead scaffolding (`FIXID`, `EXT_ID`,
  `csurf_mcuex_modified_reg`); legacy `GetOffset/GetSize/g_mcu_list` retained
  as shims (CR-F2).
- **Hardcoded `N=1`** built from the legacy 5-int config string, enforced by a
  `numUnits()==1` invariant.

**Explicitly NOT in-scope for WP-A — these are what block N>1 (CR-F1/F5/F7/F8):**
- **CCSManager channel widening (WP-F).** `m_stateRec[9]`…`m_vpotTouched[9]`
  and the `channel <= 8` asserts (`CCSManager.cpp` ~10 sites +
  `CHECKMODEANDCHANNEL`) stay 1..8 for WP-A. A 2nd unit emitting global
  channel 9+ would assert here. (Step 4 moves the *strip-state* arrays into
  `HardwareUnit`, but the channel *bounds* and touch arrays are not widened
  until WP-F.)
- **`VPOT_LED[9]` sizing + per-unit VPOT/meter routing (WP-F).** `VPOT_LED`
  (`SendMidi(0xB0, 0x2F+m_track, …)`) and `MeterBridge`
  (`SendMidi(0xD0, (pos<<4)|meter, …)`) keep using the `CSurf_MCU::SendMidi`
  shim (identity for N=1). `sendVPOT(globalChannel,…)` / `sendMeter(…)`
  routing is added in WP-F.
- **`ButtonManager` per-unit button state (WP-F).** Button pressed /
  double-click / hold state (`m_button_pressed[128]` etc.) is indexed by raw
  note; with N>1 the same note on two units collides. WP-A keeps
  `ButtonManager` CSurf_MCU-owned exactly as today (N=1 → no collision).
  Per-unit state is WP-F.
- **ProX row logic inside modes (per-mode design, CR-F7).** 16
  `CONFIG_FLAG_PROX` sites decide row usage (`MultiTrackMode.cpp:493/500/507`,
  `SendReceiveModeBase.cpp:157/170`, `CommandMode.cpp:237`, `PanMode.cpp:51`,
  `PlugMode.cpp:517/576`, `VPOT_LED.cpp:20`, `MeterBridge.cpp:60`,
  `CCSManager.cpp:509`, plus `csurf_mcu.cpp` sites). WP-A keeps the **global**
  flag (derived from the single main unit's model) as the modes' truth source;
  per-unit ProX-aware mode rendering (incl. the ProX-main + Mackie-extender
  row asymmetry) is deferred to each mode's N-unit design pass. Until then
  **mixed main/extender models are out of scope** (WP-B must prevent mixed
  configs, or accept that ProX rows render only on ProX units).

**Also deferred (as before):**
- **WP-B** — config dialog + new configString format. WP-A keeps
  `parseParms`/`createFunc` constructing 1 main unit from the legacy string.
- **WP-C** — `Tracks` singleton refactor to `N*8`. WP-A leaves
  `Tracks::adjust(numUnits()*8)` (=8) and the singleton as-is.
- **WP-D** — bank/scroll over the N×8 window.
- **WP-E** — `!m_is_mcuex` gating → `for-each main-unit` (SMPTE/assignment /
  transport broadcast to every transport-capable unit).

**Contract:** after WP-A, `numUnits()==1`, the surface registers and runs as
"Mackie Control Protocol (Klinke)" exactly as today; WP-B alone does **not**
enable N>1 — that requires WP-C + WP-F + per-mode work. The milestone
sequence to first multi-unit operation is: WP-B (per-unit config) → WP-C
(Tracks N*8) → WP-F (channel arrays + VPOT/meter routing + per-unit button
state) → per-mode N-unit design → hardware test (CR-R3 JACK N-port caveat).

---

## Risks & open integration points

- **R1 — `MultiDisplay` vs the single-active-display model (highest risk).**
  `DisplayHandler::sendDifferences` early-returns unless
  `pDisplay == m_pActualDisplay` (`DisplayHandler.cpp:51`), and `switchTo`
  sets exactly one active display. A naive `switchTo(multiDisplay)` breaks
  sending. **Mitigation (Step 7):** `MultiDisplay::switchToAll()` switches each
  child on its OWN handler so each unit's `m_pActualDisplay` is that unit's
  child; modes go through `CCSManager::switchToDisplay` (broadcast). Must also
  rewire the direct `getDisplayHandler()->switchTo(...)` callsites in modes.
  Also `ActionsDisplay` (`m_pDisplayHandler->switchTo(this)`,
  `ActionsDisplay.cpp:27`) and `m_pSplashDisplay` need the broadcast/child
  path — verify both. (The complete callsite list is now in Step 7, CR-F6 —
  also `Options.cpp:45`, `csurf_mcu.cpp:680`, `ActionsDisplay.cpp:33`.)
- **R2 — `MultiDisplay` base-class buffer.** `Display::changeField` writes into
  `m_ppText`; the composite must override ALL buffer-touching virtuals so it
  never relies on a single unified buffer (it delegates to children). Allocate
  a minimal dummy buffer in the base ctor only to satisfy the base; never use
  it for field content. Verify `getText()`/`getRowLength()` aren't relied on
  by the composite's consumers.
- **R3 — JACK sequencing on N ports (Q6 caveat).** WP-A uses the "open all →
  one 200 ms → reset all → start all" sequence (§9.6). This is **unverified for
  N>1** (the PipeWire-JACK crash, MEMD 2026-06-27, was only seen with 1 port).
  WP-A ships with N=1 so it cannot regress, but the N>1 path must be tested on
  real multi-unit hardware before WP-B enables it. Fallback: per-port 200 ms.
- **R4 — `connect2FrameSignal` / `Actions::instance()->init(this)` /
  `Tracks::instance()->setMCU(this)`.** These take `this` = `CSurf_MCU` (the
  logical surface) and are **unchanged** by WP-A — `HardwareUnit` does not
  replace `CSurf_MCU` as the `IReaperControlSurface`. `Tracks::m_pMCU` stays
  single until WP-C; for N=1 that is correct.
- **R5 — `ButtonManager` ownership.** `ButtonManager` (constructed in ctor
  819, owns the `dispatchMidiEvent` table) currently references `CSurf_MCU`.
  In Step 5 the per-strip parsing moves into the unit, but `ButtonManager`
  dispatches many global buttons too. Decide per-step whether `ButtonManager`
  stays CSurf_MCU-owned (and the unit calls into it for global buttons) or
  moves per-unit. Recommend: keep `ButtonManager` CSurf_MCU-owned for N=1
  (global buttons are surface-level); revisit at WP-F. (CR-F5: the specific
  N>1 blocker is per-note button state — `m_button_pressed[128]`,
  `m_button_doublepressed[128]`, `m_button_hold_used[128]`,
  `m_button_pressed_time[128]`, `m_button_last_time` — colliding across units;
  per-unit state or unit-tagged dispatch is the WP-F fix.)
- **R6 — `EXT_B`/extender compile switch.** EXT_B blocks were already removed
  (commit 5f03531). WP-A does not reintroduce `EXT_B`; the extender is a runtime
  `UnitConfig`, not a compile-time variant.
- **R7 — `GetTypeString`/`GetDescString`/`GetConfigString`.** After removing
  `EXT_ID`/`m_is_mcuex`, `GetTypeString` returns `MAIN_ID` always;
  `GetConfigString` still emits the legacy 5-int string (WP-B changes the
  format). Keep both stable for N=1 so REAPER's stored config round-trips.
- **R8 — macOS/Windows build.** New files are platform-agnostic but the
  `usleep`/`Sleep` split (`#ifdef _WIN32`) must be preserved in `HardwareUnit`.
  WP-A verification is Linux-only; a cross-platform compile check is advisable
  before merge.
