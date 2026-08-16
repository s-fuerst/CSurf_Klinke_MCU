# 8-Channel Widening Audit (WP-F / per-mode scope)

> Audit date: 2026-07-12.
> Scope: systematic scan of `src/` for every hardcoded 8-channel limit that
> must be widened (or explicitly decided out of scope) before channel strips
> 9..N*8 work end-to-end.
> Method: read-only audit. **No code was changed.** Findings are grouped by
> subsystem with file:line references and a verdict per item.

## TL;DR

The widening is **already much further along than the planning docs imply.**

- `extender-wp-ef-impl-plan.md` states "modes still iterate 1-8" and "channels
  9+ are NOT enabled in WP-EF." That was true when the plan was written, but
  the tree has since moved on:
  - `CHECKMODEANDCHANNEL` is **already widened** to
    `channel <= m_pMCU->availableChannels()` (CCSManager.h:25). The F1 "hard
    barrier" from the WP-D critical review is gone.
  - `MultiTrackMode`, `PanMode`, `MultiTrackMeterBridge`, and the CCSManager
    strip/touch/VPOT arrays already iterate over `availableChannels()` /
    `getNumberOfChannelStrips()`, each tagged with a `// WP-F:` comment.
  - The input path already routes extender-unit strips to global channels via
    `m_currentInputOffset` (set per-unit in `CSurf_MCU::Run()`), and
    `MultiDisplay::changeField` already maps global field → owning unit's
    local field.
- **Still hardcoded to 8** (the genuine gaps this audit is about):
  1. `SendReceiveModeBase` — channel loops, `m_recButtonPressed[8]`, ASSERT.
  2. `PlugMode` / `PlugAccess` — many `channel < 8` loops and the
     `m_channel < 9` / `m_bank < 9` validity bounds.
  3. `DisplayHandler` — `m_metersEnabled[9]` fixed array + ASSERT `channel <= 9`
     and the `for (i = 1; i < 9; i++)` loops.
  4. `Display::changeField` — `ASSERT(field < 9)` / `field < 10` (per-unit
     field count is actually correct; see note).
- Inconsistency risk: the routing plumbing is N-aware, but `PlugMode` /
  `CommandMode` / `SendReceiveMode` still emit only channels 1-8. A user with
  extenders will see MultiTrack/Pan strips 9-16 working but Send/Plug strips
  9-16 dead. This split behavior is the real thing to resolve.

The distinction that matters throughout: some `8`s are the **per-block VPOT /
bank / page count** (CommandMode `Page`, PlugMode bank/page/channel) — those
are *intentional* per-block-8 layout and the master plan (§5.3) says they stay
8 because they are relative-within-an-8-block, NOT surface-channel limits.
Only the `8`s that bound **surface channel strips** need widening.

---

## 1. Core layer — ALREADY widened

| Location | Status | Evidence |
|---|---|---|
| `src/core/CCSManager.h:25` `CHECKMODEANDCHANNEL` | ✅ widened | `ASSERT(channel >= 0 && channel <= m_pMCU->availableChannels())` |
| `src/core/CCSManager.cpp:46` | ✅ dynamic | `int nCh = pMCU->availableChannels() + 1;` drives `new VPOT_LED[nCh]`, `new bool[nCh]` for fader/vpot touched arrays |
| `src/core/CCSManager.cpp:103,240-398` | ✅ widened | LED/touch setters ASSERT against `availableChannels()` |
| `src/core/CCSManager.cpp:651-687` | ✅ widened | `getNumTrueArrayEntries(..., availableChannels()+1)` |
| `src/core/csurf_mcu.h:436,440` | ✅ widened | `unitForChannel()` guards `1..availableChannels()`; `availableChannels() = numUnits()*8` |
| `src/core/Tracks.cpp:399,567-570` | ✅ dynamic | `m_numMCUChannels` runtime, `m_channelTracks.resize(m_numMCUChannels+1)` |

No action needed in the core layer for widening.

---

## 2. Input routing — ALREADY per-unit

The WP-EF plan (Step 2d / WP-EF-0b) says the strip-input gate is retained and
units >0 drop strip input. **The code already does more:**

- `src/core/csurf_mcu.cpp:1226-1244` — `Run()` iterates every unit's MIDI
  input, sets `m_currentInputOffset = ui * 8` per unit, and dispatches all
  events through `OnMIDIEvent`.
- Every strip handler adds `m_currentInputOffset` to translate local → global
  channel: `OnFaderMove` (309), VPOT move (324), `OnRotaryEncoderPush` (477),
  `buttonRec/Mute/Solo/Select(+DC)` (487-516), `OnTouch` (683).
- `m_currentInputOffset != 0` gates drop *some* global-only events on
  secondary units (345, 766), i.e. secondary units are strip-only, unit 0
  owns global input. Global input ownership is also pinned via
  `m_globalInputUnitIndex` (875).
- `ButtonManager` keeps per-unit pressed state
  (`m_perUnitState`, ButtonManager.cpp:21-33, 139, 212-224) and translates
  local → global channel for long-press handlers (224).

**Verdict:** the strip-input widening is effectively done at the routing
level. What is NOT done is making the *modes* emit to channels 9+ (see §3-5).

---

## 3. Modes — mixed

### 3.1 MultiTrackMode — ✅ ALREADY widened
All channel loops in `src/modes/multitrack/MultiTrackMode.cpp` use
`int nStrips = Tracks::instance()->getNumberOfChannelStrips();` with
`// WP-F: widened from 8 to getNumberOfChannelStrips()` comments
(lines 66-67, 97-98, 110-111, 123-124, 145-146, 165-166, 440-441, 461-462,
475-477). `MultiTrackMeterBridge.cpp:32` iterates `availableChannels()`.

### 3.2 PanMode — ✅ ALREADY widened
`src/modes/multitrack/PanMode.cpp:49-51` uses `getNumberOfChannelStrips()`.

### 3.3 SendReceiveModeBase — ❌ NOT widened (GAP)
**The biggest remaining mode gap.** All channel-related loops hardcode 8:
- `src/modes/sends/SendReceiveModeBase.cpp:101` `for (iInfo = 1; iInfo < 9; …)` (vol/pan read)
- `:182, 291` `iInfo < 8` (visible-sends loops — these bound *sends per
  channel*, arguably per-block-8 by design, but the rec-button array tied to
  them is surface-channel-sized — see below)
- `:188, 249` `iInfo < 8` (automode / track-name display loops)
- `:342` `ASSERT(channel > 0 && channel < 9)` (setRecButtonPressed)
- `:534` `for (i = 1; i < 9; i++)` (setAutoMode broadcast)
- `src/modes/sends/SendReceiveModeBase.h:105` `bool m_recButtonPressed[8];`
  — **fixed-size array indexed by channel-1** (`:343`, `:535`). This is the
  hard limiter: with N>1, channel 9 would write out of bounds.
- `src/modes/sends/SendReceiveMeterBridge.cpp:41` `iInfo < 8`.

**Action:** widen to `availableChannels()`; make `m_recButtonPressed` a
`std::vector<bool>` sized to `availableChannels()`. The per-channel send
loops (`iInfo < 8` over `m_sendInfos`) need a design call: do extenders show
more sends per channel, or more channels of the same send-window? Per master
plan §7 the Send/Receive per-mode design is still TBD, so this is the pilot
question for that WP.

### 3.4 CommandMode — ⚠️ MOSTLY per-block-8 (design question)
`CommandMode` inherits `MultiTrackMode` (channel-strip handling already
widened upstream). The remaining `8`s in `src/modes/commands/` are the
**VPOT/bank/page structure**, which the master plan (§5.3) classifies as
relative-within-an-8-block and therefore unit-independent:

- `CommandMode.h:35,40` `ASSERT(shift < 2 && channel < 8)` — 8 = the 8 VPOTs
  on one MCU unit. This is **per-unit-local**, correct by design.
- `CommandMode.h:77,101,109` `index < 8` / `for (i=0;i<8;i++)` — page/bank
  indices, also per-block.
- `CommandMode.cpp:21,49,90,105,139,269,276` and the editor files — all
  iterate the 8 VPOTs / 8 pages / 8 banks per block.

**Action:** none for widening per se. Open design question for the Command
per-mode WP: does an extender (a) mirror the same 8 VPOTs, (b) show a
different page/bank, or (c) add 8 more VPOTs to the action surface? That
decision drives whether any of these `8`s become `availableChannels()`-scaled.
Today they are correctly scoped as per-unit.

### 3.5 PlugMode / PlugAccess — ❌ NOT widened (GAP, largest)
`PlugMode` is the biggest subsystem and still hardcoded throughout. Two
distinct kinds of `8`:

**(a) Per-block structure** (bank/page/channel within an 8-block) — by design
stays 8 (§5.3): `PlugAccess.h:50-51` `m_bank < 9 && m_page < 9`,
`PlugAccess.cpp:187,194,203` bank/page/channel loops, `PlugMode.cpp:243,1167-
1169`, `PlugPresetManager.cpp:29-33`, `PlugMap.cpp:142,206,272`, editor
`for (i=0;i<8;i++)` in `PlugModeBankComponent/PageComponent/ChannelComponent`.

**(b) Surface-channel strip loops** — these need widening:
- `src/modes/plugin/PlugMode.cpp:357,370,855,885,898` `for (channel=0; channel<8;…)`
  — fader/VPOT update loops over surface channels (tied to the 8 strips).
- `:440,452,520,549,651,685` `iFader/iVPot/iChannel < 8` — strip update paths.
- `src/modes/plugin/PlugAccess.h:51` `m_channel >= 0 && m_channel < 9` —
  channel validity bound. Note the latent bug: the condition reads
  `m_page >= 0 && m_bank < 9` (should be `m_page < 9`); orthogonal to
  widening but worth fixing in the same pass.
- `src/modes/plugin/PlugModeSelectors.cpp:132,141,152,160` `i < 8` — selector
  rings per strip.
- `src/modes/plugin/PlugModeMeterBridge` — needs the same treatment as
  MultiTrackMeterBridge (already widened).

**Action:** split the two kinds. The (a) bank/page/channel 8s stay. The (b)
strip loops should iterate `availableChannels()` like MultiTrackMode does, with
`// WP-F:` comments. The `PlugMap` param-offset model (FX-param-relative)
keeps working unchanged because it is per-block.

---

## 4. Hardware / display layer

### 4.1 VPOT_LED — ✅ dynamic
`CCSManager` allocates `new VPOT_LED[nCh]` (CCSManager.cpp:47) and inits each
with its owning unit's ProX flag (CCSManager.cpp:56). `VPOT_LED` itself is a
single-channel object (no internal `[9]`). Routing via `sendStripCC` is
N-aware. **No widening gap.**

### 4.2 DisplayHandler — ❌ fixed `[9]` (GAP)
- `src/hardware/display/DisplayHandler.h:22` `bool m_metersEnabled[9];` —
  fixed array, channel-indexed.
- `DisplayHandler.cpp:31` `for (i=0;i<9;i++)` init.
- `:121` `ASSERT(channel > 0 && channel <= 9)` in `enableMCUMeter`.
- `:155` `for (i=1;i<9;i++)` (enableMCUMetersAll?).

**Action:** make `m_metersEnabled` a `std::vector<bool>` sized to
`availableChannels()+1`; relax the ASSERT to `<= availableChannels()`. This
is a straightforward widening.

### 4.3 Display::changeField — ⚠️ per-unit field count (CORRECT as-is)
- `src/hardware/display/Display.cpp:93` `ASSERT(field > 0 && field < 9)` for
  rows <2 (the 8 strip fields on one unit's LCD).
- `:96` `ASSERT(field > 0 && field < 10)` for rows ≥2 (8 strips + assignment).

**Verdict:** these `9`/`10` are the **per-unit** field count (one MCU LCD
always shows 8 strip fields + assignment). They are correct *per unit* and
must NOT be widened — the global field is split into local fields by
`MultiDisplay::changeField` (MultiDisplay.cpp:36-44), which already computes
`unitIndex = (field-1)/8` and `localField = (field-1)%8 + 1`. **No gap here.**

### 4.4 MultiDisplay — ✅ N-aware
`MultiDisplay::changeField` (MultiDisplay.cpp:33-44) accepts a global field
1..N*8 and routes to the owning child display. `numStrips = m_children.size()*8`.
All broadcast methods iterate `m_children`. **No widening gap.**

### 4.5 MeterBridge base — ✅ planned N-aware (per WP-EF)
`MeterBridge` strip state is being split into `m_stripMeterPos` vector +
`m_masterMeterPos[2]` per WP-EF Step 6. Check the current tree before acting:
the audit found `MultiTrackMeterBridge` already uses `availableChannels()`,
so the base class may already be partially dynamic. Verify before widening
the sends/plugin meter bridges.

### 4.6 HardwareUnit — ✅ per-unit correct
`HardwareUnit.h:110` `int m_faderPos[9]` is the **per-unit** local cache
(0..7 strips + [8]=master). Per-unit, correct, not a widening target.

---

## 5. Config / persistence — check needed

Per master plan §5.3, the only unit-count-sensitive persisted values are
`TrackState.anchor` and `TrackState.q_channel` (absolute surface slots
1..N*8, 0=none). Out-of-range handling (inactive/hidden, not lost) is
required on load and at runtime.

- **Action (verification, not widening):** confirm `anchor`/`q_channel` are
  stored and loaded as absolute surface slots and that an anchor beyond the
  current active range is preserved, not clamped-and-destroyed. This is a
  load-path review, separate from the channel-loop widening.

---

## 6. Summary table — what still needs widening

| # | Subsystem | File:line | Kind | Action |
|---|---|---|---|---|
| 1 | SendReceiveModeBase | `.cpp:101,182,188,249,291,342,534`, `.h:105` | surface channel | widen loops + `m_recButtonPressed[8]` → vector |
| 2 | SendReceiveMeterBridge | `.cpp:41` | surface channel | widen loop |
| 3 | PlugMode strip loops | `.cpp:357,370,440,452,520,549,651,685,855,885,898` | surface channel | widen to `availableChannels()` |
| 4 | PlugAccess validity | `.h:51` | surface channel | `m_channel < availableChannels()+1`; fix latent `m_page`/`m_bank` typo |
| 5 | PlugModeSelectors | `.cpp:132,141,152,160` | surface channel | widen loop |
| 6 | PlugModeMeterBridge | (verify base) | surface channel | align with MultiTrackMeterBridge widening |
| 7 | DisplayHandler meters | `.h:22`, `.cpp:31,121,155` | surface channel | ✅ DONE (2026-07-12): `m_metersEnabled[9]` → `std::vector<bool>`, ASSERT relaxed, loop bounds use `.size()` |

## 7. Explicitly NOT widening (correct as-is)

| Item | Why |
|---|---|
| `CommandMode::Page` 8s (VPOTs/pages/banks) | per-block-8 structure, unit-independent (§5.3) |
| `PlugMode` bank/page/channel 8s (PlugMap, presets, PlugAccess bank/page) | FX-param-relative, per-block (§5.3) |
| `Display::changeField` field<9/field<10 | per-unit LCD field count; global split done by MultiDisplay |
| `HardwareUnit::m_faderPos[9]` | per-unit local cache (0..7 + master) |
| `csurf_mcu.cpp` `m_mackie_lasttime[10]`, `bla[10]` | timecode digit buffers, not channel-related |

## 8. Recommended order of work (per-mode WPs)

Following master plan §7 (easiest → hardest), and given that MultiTrack/Pan
are already done:

1. **DisplayHandler meters** (§6 row 7) — trivial, unblocks meter display
   for channels 9+ in every mode.
2. **SendReceiveMode** (§6 rows 1-2) — design pilot: decide
   more-channels vs. more-sends-per-channel, then widen.
3. **PlugMode strip loops** (§6 rows 3-6) — biggest, but the per-block
   PlugMap model means only the strip loops change, not the bank/page math.
4. **CommandMode** — likely no widening needed (per-block); confirm the
   per-mode design decision (mirror vs. extend) first.

## 9. Inconsistency warning

Because MultiTrack/Pan strips 9-16 already work but Send/Plug strips 9-16
are dead, a user with extenders today sees a **partly working** surface. If
this is shipped before §6 is closed, document the split behavior explicitly
in the release notes ("MultiTrack/Pan support N units; Send/Plugin/Command
still single-unit"). Alternatively, gate the extender channel count in the
config dialog to 8 until the remaining modes catch up.
