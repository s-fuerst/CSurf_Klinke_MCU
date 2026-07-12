# WP-SendRecv — Send/Receive Multi-Unit Widening

> Status: PLANNED (2026-07-12). Scope confirmed with maintainer.
> Depends on: WP-A (HardwareUnit), WP-B (SurfaceConfig), WP-C (Tracks
> N\*8), WP-D (banking), WP-EF (routing + MeterBridge vector split).
> Predecessor audit: `extender-wp-f-widening-audit.md` §6 rows 1–2.
> Master-plan home: `extender-support.md` §7 item 2 (2nd per-mode WP).

## What this work package delivers (honest scope)

Widen the **Send** and **Receive** modes from a fixed 8-strip window to the
runtime channel count (`availableChannels()` = `numUnits() * 8`), so that on a
multi-unit surface all `N * 8` channel strips show and control the selected
track's sends/receives at once, with bank/channel scrolling sized to the
surface.

This is a **widening**, not a redesign. The mode's data model, the
single-selected-track semantics, the Send↔Receive toggle on `Select`, the
Flip (vol↔pan) behaviour, the meter bridge, and the ProX 2-panel display path
are all preserved. Only the literals `8` / `9` that bound **surface channel
strips**, the fixed `m_recButtonPressed[8]` array, and the banking step size
change. Per-send/per-block structure (there is none here beyond the strip
window itself) is untouched.

What this WP does **not** do (see "Deferred"):

- No redesign to "one track per strip" (the rejected design option).
- No PlugMode / CommandMode widening (separate per-mode WPs; see §"Split
  behaviour").
- No dynamic unit activation (§5.4 of the master plan) — `availableChannels()`
  is session-static within this WP; revisit when that feature lands.

## Scope contract & design decision

### The pilot question (master plan §7), resolved

> With N units, what do strips 9..N\*8 show? Today each strip shows ONE
> send (or receive) of the ONE selected track.

**Decision: "wider send window" (linked-mode default, option a).**

Strips `1..N*8` = sends `1..N*8` of the *same* selected track. The send
window is `N*8` wide; bank/channel scroll shifts that window across the
track's full send list. This generalises exactly like MultiTrackMode (next 8
channels → next 8 sends) and is the §7 linked-mode default ("extend the same
view").

```
SELECTED TRACK: Guitar   (single track — unchanged)

Strip:   1   2   3   4   5   6   7   8  || 9  10  11  12  13  14  15  16
Send#:   1   2   3   4   5   6   7   8  || 9  10  11  12  13  14  15  16
         <--- unit 1: Guitar sends ----> || <--- unit 2: Guitar sends --->

Fader/VPOT/Mute/Solo/RecArm/Select all index by GLOBAL strip 1..N*8.
Select on strip k → jump to send k's dest (Send) / src (Receive) track
                + flip Send <-> Receive mode.   (already N-safe, see Step 6)
```

**Why not the alternatives:**

- **"One track per strip"** (each strip = a different track's send into the
  selected track): breaks the single-selected-track model the whole mode is
  built on (`selectedTrack()` drives every `GetSetTrackSendInfo` call), and
  re-opens "which vol/pan/meter per strip?" It is a new mode, not a widening.
  Rejected for this WP.
- **"Mirror"** (all units show the same 8 sends): useless for sends and
  contradicts the linked-mode default. Rejected.

### Invariants this WP must not violate

1. **N = 1 ⇒ identical behaviour.** Every literal change is `8 → nStrips`
   with `nStrips == 8` at N=1, so by construction nothing changes for a
   single-unit surface. This is the primary regression guard.
2. **Per-unit correctness.** Strip output goes through the already-N-aware
   CCSManager setters (`setFader`, `setRecLED`, `getVPOT`, …) and
   `MultiDisplay::changeField` (global field → owning unit's local field).
   No new per-unit logic is added in this WP.
3. **No out-of-bounds.** `m_recButtonPressed` becomes dynamically sized and is
   never indexed by a channel ≥ its size. All loops are bounded by
   `min(nStrips, sendsRemaining)` via the existing
   `m_startWithSend + iInfo < m_sendInfos.size()` guards.

## Current state (verified against the tree, 2026-07-12)

The Send/Receive mode is **already feature-complete for a single unit** and
is reachable via `B_VPOT_SEND` (CCSManager.cpp:145–152, 198–201, 729–732).
It already supports: faders (vol, or pan under Flip), VPOT rings (pan, or vol
under Flip), Mute / Solo(=mono) / RecArm(=automation-mode cycle), Select
(track-jump + Send↔Receive toggle), bank/channel scroll within the send list,
metering, and the ProX 2-panel 4-row display.

**Already N-aware upstream (no work here):**

| Layer | Evidence |
|---|---|
| CCSManager strip arrays | `new VPOT_LED[nCh]`, `new bool[nCh]` driven by `availableChannels()+1` (CCSManager.cpp:46–47); setters ASSERT against `availableChannels()` (audit §1). |
| Input routing | `m_currentInputOffset = ui*8` per unit; strip handlers add it (csurf_mcu.cpp:1226–1244). Secondary units are strip-only; unit 0 owns global input. |
| `CHECKMODEANDCHANNEL` | already `channel <= availableChannels()` (CCSManager.h:25). |
| `MultiDisplay` | `changeField` maps global field → owning unit's local field (MultiDisplay.cpp:33–44). `createDisplay(4)` already returns a MultiDisplay for N>1 (CCSManager.cpp:630–633). |
| MeterBridge base | already split to `std::vector<double> m_stripMeterPos` + `ensureStripMeterState()` (MeterBridge.h:31–33, WP-EF Step 6). |
| DisplayHandler meters | already widened to `std::vector<bool> m_metersEnabled` (DisplayHandler.h:25, WP-F). |
| `selectedTrack()` / `getNumberOfChannelStrips()` | CCSMode.h:101 / Tracks.cpp:1282 (`m_numMCUChannels`). |

**Still hardcoded to 8 (this WP's scope).** Every site is a **surface-channel
strip** bound — there is no per-block/per-send-8 structure in this mode to
preserve:

| # | File:line | Site | Kind |
|---|---|---|---|
| 1 | `SendReceiveModeBase.h:105` | `bool m_recButtonPressed[8];` | fixed array indexed by `channel-1` → OOB at N>1 (**hard limiter**) |
| 2 | `SendReceiveModeBase.cpp:37–38` | `for (i=0;i<8;i++) m_recButtonPressed[i]=false;` (activate) | init of #1 |
| 3 | `SendReceiveModeBase.cpp:52` | `for (iInfo=0;iInfo<8;…)` updateRecLEDs | strip loop |
| 4 | `SendReceiveModeBase.cpp:71` | `for (iInfo=0;iInfo<8;…)` updateSoloLEDs | strip loop |
| 5 | `SendReceiveModeBase.cpp:83` | `for (iInfo=0;iInfo<8;…)` updateMuteLEDs | strip loop |
| 6 | `SendReceiveModeBase.cpp:101` | `for (iInfo=1;iInfo<9;…)` updateFaders | strip loop |
| 7 | `SendReceiveModeBase.cpp:132` | `for (iInfo=0;iInfo<8;…)` updateVPOTs | strip loop |
| 8 | `SendReceiveModeBase.cpp:182–183` | `… && iInfo < 8` updateDisplay data loop | strip loop |
| 9 | `SendReceiveModeBase.cpp:188` | `for (iInfo=0;iInfo<8;…)` inner automode display loop | strip loop |
| 10 | `SendReceiveModeBase.cpp:235` | `while (iInfo < 8)` clear remaining fields | strip loop |
| 11 | `SendReceiveModeBase.cpp:249` | `for (iInfo=0;iInfo<8;…)` updateDisplayProX | strip loop |
| 12 | `SendReceiveModeBase.cpp:291–292` | `… && iInfo < 8` updateDisplayProX track loop | strip loop |
| 13 | `SendReceiveModeBase.cpp:308` | `while (iInfo < 8)` clear remaining | strip loop |
| 14 | `SendReceiveModeBase.cpp:342` | `ASSERT(channel > 0 && channel < 9)` buttonRec | channel bound |
| 15 | `SendReceiveModeBase.cpp:368–385` | banking math (`+= 8`, `-= 8`, `+9` guard) | window width |
| 16 | `SendReceiveModeBase.cpp:534–535` | `for (i=1;i<9;…) … m_recButtonPressed[i-1]` setAutoMode | strip loop + array |
| 17 | `SendReceiveMeterBridge.cpp:41` | `for (iInfo=0;iInfo<8;…)` | strip loop |

> Note: `SendReceiveModeBase.cpp:368` reads `m_startWithSend + 9`; this is a
> *sends-remaining* guard expressed in the old 8-window terms and is covered
> by Step 8, not a separate channel literal.

**Already N-safe (no change needed — verify only):** every input handler
(`buttonRec`, `buttonMute`, `buttonSolo`, `fader`, `faderTouched`,
`vpotMoved`, `buttonSelect` in SendMode/ReceiveMode) computes
`sendNr = m_startWithSend + channel - 1` from the incoming global `channel`
and is therefore correct for any channel 1..N\*8. `getNumSends()`,
`getChannelOffset()`, `somethingTouched()`, `setAutoMode()`'s *mechanism*
(it just iterates pressed buttons), and `updateMasterLEDs` (selected-track
peak → all ProX units) are likewise N-agnostic.

## The widening primitive

Mirror MultiTrackMode exactly. At the top of each update method, add:

```cpp
// WP-F: widened from 8 to getNumberOfChannelStrips()
const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
```

`getNumberOfChannelStrips()` (`m_numMCUChannels`) is the proven-correct source
of truth already used by MultiTrackMode and PanMode. It equals
`availableChannels()` today (both are session-static = `numUnits()*8`); when
§5.4 dynamic activation lands, both must be re-checked to use the
runtime-active count (see Risks).

For the meter bridge, use `pMCU->availableChannels()` directly (matches
MultiTrackMeterBridge.cpp:31–32).

Each widened loop keeps its existing
`m_startWithSend + iInfo < m_sendInfos.size()` / `<= size()` guard, so strips
beyond the available sends fall through to the existing `else` (LED OFF,
fader 0, blank display field). That guard is what implements
"min(nStrips, sendsRemaining)".

## Steps

All edits are in `src/modes/sends/` unless noted. Order is chosen so the build
stays compilable after each step and the hardest reasoning (banking) is last.

### Step 1 — `m_recButtonPressed[8]` → dynamically sized (the hard limiter)

`SendReceiveModeBase.h:8` already includes `<vector>`.

- `SendReceiveModeBase.h:105`:
  ```cpp
  bool m_recButtonPressed[8];
  ```
  →
  ```cpp
  std::vector<bool> m_recButtonPressed; // WP-F: indexed by channel-1, sized to nStrips
  ```
  (A `std::vector<char>` is an equally valid choice if the `vector<bool>`
  proxy is undesired; the code only ever writes/reads `bool` by value — no
  references are taken — so `vector<bool>` is safe here.)

- `SendReceiveModeBase.cpp` constructor (around L17): the member-initialiser
  list initialises `m_startWithSend(0)` etc.; `m_recButtonPressed` defaults to
  empty, which is fine — it is (re)sized in `activate()`.

- `SendReceiveModeBase.cpp:37–38` (in `activate()`):
  ```cpp
  for (int i = 0; i < 8; i++) {
      m_recButtonPressed[i] = false;
  }
  ```
  →
  ```cpp
  // WP-F: size rec-button state to the surface channel count.
  m_recButtonPressed.assign(
      Tracks::instance()->getNumberOfChannelStrips(), false);
  ```
  `#include "Tracks.h"` is already present (SendReceiveModeBase.cpp top).

This removes the only fixed array; everything below just widens loops.

### Step 2 — LED update loops (rec / solo / mute)

In `updateRecLEDs`, `updateSoloLEDs`, `updateMuteLEDs`
(`SendReceiveModeBase.cpp:52`, `:71`, `:83`):

```cpp
for (unsigned int iInfo = 0; iInfo < 8; iInfo++) {
```
→
```cpp
// WP-F: widened from 8 to nStrips
const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
for (int iInfo = 0; iInfo < nStrips; iInfo++) {
```

(The `m_startWithSend + iInfo < m_sendInfos.size()` guard and the `else`
LED-OFF branch are unchanged and now correctly blank strips 9+ when there are
fewer sends than strips.)

### Step 3 — `updateFaders` (`:101`)

```cpp
for (unsigned int iInfo = 1; iInfo < 9; iInfo++) {
```
→
```cpp
const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
for (int iInfo = 1; iInfo <= nStrips; iInfo++) {
```

The guard `m_startWithSend + iInfo <= m_sendInfos.size()` stays. The
master-fader block (`channel == 0`, selected-track volume) is outside the
loop and unchanged.

### Step 4 — `updateVPOTs` (`:132`)

```cpp
for (unsigned int iInfo = 0; iInfo < 8; iInfo++) {
```
→
```cpp
const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
for (int iInfo = 0; iInfo < nStrips; iInfo++) {
```

`getVPOT(iInfo+1)` is valid (CCSManager VPOT array is N-sized). The
`m_startWithSend + iInfo < m_sendInfos.size()` guard and `else`
(`VPOT_LED::OFF`, value 0) stay.

### Step 5 — display loops (`updateDisplay` + `updateDisplayProX`)

Five loop bounds + the two "clear remaining" loops. All write via
`m_pDisplay->changeField(row, iInfo+1, …)` / `showDB` / `showPan` /
`clearLine` — global field `1..nStrips`, routed by MultiDisplay to the owning
unit. Compute `nStrips` once at the top of each method.

- `updateDisplay` (`:182–183`): the data loop condition
  `(m_startWithSend + iInfo) < m_sendInfos.size() && iInfo < 8` →
  `… && iInfo < nStrips`.
- `updateDisplay` (`:188`): inner automode loop `iInfo < 8` → `iInfo < nStrips`.
- `updateDisplay` (`:235`): `while (iInfo < 8)` → `while (iInfo < nStrips)`
  (clear remaining strip fields — must cover all strips so unit-2 fields are
  blanked when there are fewer sends).
- `updateDisplayProX` (`:249`): `iInfo < 8` → `iInfo < nStrips`.
- `updateDisplayProX` (`:291–292`): `… && iInfo < 8` → `… && iInfo < nStrips`.
- `updateDisplayProX` (`:308`): `while (iInfo < 8)` → `while (iInfo < nStrips)`.

The assignment-field write at the end of `updateDisplayProX`
(`changeField(2, 9, …)`, selected-track name + master vol) is the per-unit
assignment slot (field 9 = "8 strips + assignment" per unit), **not** a global
strip — leave it at `9`.

### Step 6 — `buttonRec` ASSERT + array guard (`:342–343`)

```cpp
ASSERT(channel > 0 && channel < 9);
m_recButtonPressed[channel - 1] = pressed;
```
→
```cpp
// WP-F: bound to availableChannels(); state array is sized in activate().
ASSERT(channel > 0 && channel <= m_pCCSManager->getMCU()->availableChannels());
if ((int)m_recButtonPressed.size() < channel)
    m_recButtonPressed.resize(channel, false); // defensive (e.g. §5.4 shrink)
m_recButtonPressed[channel - 1] = pressed;
```

The `resize` guard makes the array robust if the surface width ever changes
mid-session without a fresh `activate()` (future §5.4). The downstream
`sendNr = m_startWithSend + channel - 1` is already N-safe.

### Step 7 — `setAutoMode` broadcast (`:534–535`)

Rewrite to be array-driven instead of a fixed 1..8 loop:

```cpp
for (unsigned int i = 1; i < 9; i++) {
    if (m_recButtonPressed[i-1]) {
        int sendNr = m_startWithSend + i - 1;
        setSendInfo(AUTOMODE, sendNr, (void *)&mode);
        ThemeLayout_RefreshAll();
        ret = true;
    }
}
```
→
```cpp
// WP-F: iterate actual pressed-state (sized to nStrips), not a fixed 1..8.
for (size_t i = 0; i < m_recButtonPressed.size(); i++) {
    if (m_recButtonPressed[i]) {
        int sendNr = m_startWithSend + (int)i;
        setSendInfo(AUTOMODE, sendNr, (void *)&mode);
        ThemeLayout_RefreshAll();
        ret = true;
    }
}
```

### Step 8 — `buttonFaderBanks` banking math (`:366–385`) — needs care

This is the only step that changes *numeric* behaviour, not just a loop
bound. The window width becomes `nStrips` instead of `8`. Read `nStrips` once
at the top of the method.

Current:
```cpp
switch (button) {
case B_BANK_UP:
    if ((m_startWithSend + 9) > (int)m_sendInfos.size())
        return true;
    m_startWithSend += 8;
    break;
case B_BANK_DOWN:
    m_startWithSend -= 8;
    break;
case B_CHANNEL_UP:
    m_startWithSend++;
    break;
case B_CHANNEL_DOWN:
    m_startWithSend--;
    break;
}
if (m_startWithSend < 0)
    m_startWithSend = 0;
else if (m_startWithSend + 1 > (int)m_sendInfos.size())
    m_startWithSend = (int)m_sendInfos.size() - 1;
```

Widened (window = `nStrips`):
```cpp
const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
switch (button) {
case B_BANK_UP:
    // No more sends beyond the current window → ignore.
    if (m_startWithSend + nStrips >= (int)m_sendInfos.size())
        return true;
    m_startWithSend += nStrips;
    break;
case B_BANK_DOWN:
    m_startWithSend -= nStrips;
    break;
case B_CHANNEL_UP:
    m_startWithSend++;            // scroll by one send (unchanged)
    break;
case B_CHANNEL_DOWN:
    m_startWithSend--;            // scroll by one send (unchanged)
    break;
}
// Preserve the existing clamp semantics; only the bank step width changed.
if (m_startWithSend < 0)
    m_startWithSend = 0;
else if (m_startWithSend + 1 > (int)m_sendInfos.size())
    m_startWithSend = (int)m_sendInfos.size() - 1;
// (clamp expression kept identical to the original so N=1 is provably
//  unchanged; `updateEverything();` below the switch is unchanged.)
```

Notes:
- `B_BANK_UP` guard: "is the next window empty?" `m_startWithSend + nStrips >=
  size` means the next window start (`m_startWithSend + nStrips`) would be ≥
  the send count → no bank up. At N=1 this is `+8 >= size`, exactly equivalent
  to the old `+9 > size` (both mean "send index `start+8` is not < size").
- `B_CHANNEL_UP/DOWN` stay per-send (±1). Channel-scroll moves the window by
  one send across the whole N\*8 surface, consistent with MultiTrack's
  bank-wide channel scroll.
- The clamp expression is deliberately left byte-identical to the original
  (it already operates on `m_sendInfos.size()`, which is width-independent),
  so only the two bank *step* literals and the bank-up guard change. Optional
  hardening: a track with zero sends would underflow `size()-1` to `-1`, but
  the mode is unreachable then (CCSManager gates `B_VPOT_SEND` on
  `getNumSends() > 0`); leave as-is to keep N=1 equivalence airtight.
- **This step is the one to eyeball hardest in review and on hardware.**
  See Exit Criteria and Risks.

### Step 9 — `SendReceiveMeterBridge::updateMeterBridge` (`:41`)

```cpp
for (int iInfo = 0; iInfo < 8; iInfo++) {
    if ((offset + iInfo) < sendInfos.size()) {
```
→
```cpp
// WP-F: widened from 8 to availableChannels() (matches MultiTrackMeterBridge)
const int nStrips = pMCU->availableChannels();
for (int iInfo = 0; iInfo < nStrips; iInfo++) {
    if ((offset + iInfo) < sendInfos.size()) {
```

`ensureStripMeterState(pMCU->availableChannels())` at `:23` is already
present (WP-EF); only the loop bound at `:41` changes. `updateMeter(iInfo+1,
…)` is valid for 1..N\*8.
`updateMasterLEDs` (selected-track peak → every ProX unit) is unchanged and
already N-aware.

### Step 10 — consistency sweep (verify, mostly no edits)

- `getChannelOffset()` (`SendReceiveModeBase.h:76`) returns `m_startWithSend`
  (a *send* index, not a channel). Used by the meter bridge as `offset`.
  Correct as-is — no change.
- `SendMode` / `ReceiveMode`: their `getSendInfo`/`setSendInfo`/
  `calcSendIdx*`/`getTrackUIVol` take an absolute send index and are
  channel-agnostic. `buttonSelect` computes the send from the global
  `channel` arg and is already N-safe. No change.
- `getNumSends()` returns `m_sendInfos.size()` — unaffected.

After the edits, run:
```bash
grep -nE "iInfo < 8|iInfo < 9|i < 8|i < 9|\[8\]|< 9;" src/modes/sends/
```
The only remaining hit should be the per-unit assignment field literal `9` in
`updateDisplayProX` (intentional, see Step 5).

## N = 1 compatibility

By construction: at N=1, `getNumberOfChannelStrips() == availableChannels()
== 8`, so every `8 → nStrips` and `9 → nStrips+1` substitution evaluates to
the original literal, `m_recButtonPressed.assign(8, false)` is identical to
the old `[8]` zeroing, and the banking window is 8. **No behavioural change
for a single-unit surface** is the primary regression criterion.

## Exit Criteria

A change is ready to merge when **all** hold:

1. **Builds clean** on Linux, Windows, and macOS (Release) with
   `MCU_DEBUG_LOG` both ON and OFF — no new warnings in `src/modes/sends/`.
2. **No widening literals remain** in `src/modes/sends/` except the
   intentional per-unit assignment-field `9` (grep above).
3. **N = 1 regression:** Send and Receive modes behave identically to the
   pre-change build on a single MCU unit (faders, VPOTs, mute/solo/recarm,
   select toggle, flip, bank/channel scroll, meters, ProX display).
4. **N = 2 (16 strips), Send mode:** with a track that has ≥16 sends —
   - all 16 faders reflect send volumes; VPOT rings reflect send pans
     (volumes under Flip);
   - mute/solo/recarm LEDs on strips 9–16 track their sends;
   - pressing RecArm on strip 9–16 cycles that send's automation mode and
     updates the display label;
   - holding ≥1 RecArm buttons and changing the global automation mode
     (`setAutoMode`) applies to *all* held sends, including those on unit 2;
   - Bank Up/Down moves the window by 16; Channel Up/Down by 1; clamps at
     the send-list ends;
   - Select on strip 9–16 jumps to that send's destination track and switches
     to Receive mode; Receive mode mirrors all of the above with `P_SRCTRACK`.
5. **N = 2, fewer sends than strips:** a track with, say, 4 sends shows
   populated strips 1–4 and LED-OFF / fader-0 / blank-field strips 5–16; no
   asserts, no OOB writes, no garbage on unit 2's display.
6. **Meters** populate on both units; the master LEDs show the selected
   track's peak on every ProX unit.
7. **No OOB:** `m_recButtonPressed` is never indexed ≥ its size (ASSERT +
   resize guard in `buttonRec`); `setAutoMode` iterates only the array's
   actual size.

## Deferred / out of scope

- **"One track per strip" redesign** — rejected (see Scope contract).
- **PlugMode / CommandMode widening** — separate per-mode WPs (master plan §7
  items 3–4). This WP leaves them single-unit.
- **Dynamic unit activation (§5.4)** — "release the extenders" runtime
  toggle. When it lands, re-verify that `getNumberOfChannelStrips()` /
  `availableChannels()` used here track the *active* count, and that
  `m_recButtonPressed` survives a mid-session width change (the Step 6 resize
  guard is the forward-compat seam).
- **Editor GUI:** Send/Receive has no on-screen editor component (unlike
  Plug/Command/MultiTrack). None is needed for widening; the surface-edit
  dialog (unit count) already exists from WP-B.

## Split behaviour (release-notes obligation)

Per `extender-wp-f-widening-audit.md` §9: after this WP, **MultiTrack, Pan,
and Send/Receive** support N units, while **Plug and Command** are still
single-unit. A user with extenders will see strips 9–16 live in the mixer/pan
/sends views but dead in FX/action views. **Document this explicitly in the
release notes** until the Plug/Command per-mode WPs land. (Alternative:
temporarily gate the config dialog's unit count to 1 until those modes catch
up — not chosen here, but noted as the audit's fallback.)

## Risks

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | `buttonFaderBanks` off-by-one (Step 8) — bank-up guard or clamp mis-tuned, causing a stuck window or a skip | Medium | Medium (UX glitch, self-contained) | Unit-test the four cases by hand; the N=1 equivalence check pins the regression; hardest-reviewed step. |
| R2 | `m_recButtonPressed` indexed OOB if width changes without `activate()` (future §5.4) | Low today | High (crash) | Step 6 resize guard; revisit at §5.4 time. |
| R3 | `vector<bool>` proxy surprises (someone takes `&m_recButtonPressed[i]`) | Low | Low | No code takes references; flagged in Step 1; switch to `vector<char>` if it ever does. |
| R4 | ProX 2-panel × N display density (fields overflow / mis-routed) | Low | Medium (visual) | `MultiDisplay::changeField` already routes global→local correctly; verify on a 2-unit ProX setup; the "clear remaining" loops (Step 5) must reach all strips. |
| R5 | Banking interaction with the Send↔Receive Select toggle (window resets on track change) | Low | Low | `activate()` already resets `m_startWithSend = 0` when the selected track changes (SendReceiveModeBase.cpp:33); unaffected by width. |
| R6 | Stale `nStrips` captured once but `availableChannels()` changing mid-frame | Very Low | Low | Both are session-static today; capture once per method is correct. Re-check at §5.4. |

## Verification

### Build (all three platforms)

```bash
# Linux (local dev build)
./scripts/fetch_deps.sh                       # one-time
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)"
# repeat with -DMCU_DEBUG_LOG=OFF

# Windows from WSL
scripts/build-windows.sh --reconfigure        # after source-list edits (none here)

# macOS
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(sysctl -n hw.ncpu)"
```

No new source files are added, so **no `CMakeLists.txt` change is required.**

### Static checks

```bash
# 1. No widening literals left (only the intentional assignment-field 9 should remain)
grep -nE "iInfo < 8|iInfo < 9|i < 8|i < 9|\[8\]|< 9;" src/modes/sends/

# 2. Every widened loop carries the WP-F marker for the next auditor
grep -n "WP-F" src/modes/sends/
```

### Manual test matrix (single selected track)

| Config | Track sends | Action | Expected |
|---|---|---|---|
| N=1 | 4 | enter Send | strips 1–4 live, 5–8 off |
| N=1 | 12 | Bank Up | window → sends 9–12 (+ off) |
| N=1 | 12 | Flip | faders↔VPOT swap on all 8 |
| N=1 | 8 | RecArm ch1, ch8 + global auto mode | both sends change mode |
| N=2 | ≥16 | enter Send | 16 strips live across both units |
| N=2 | ≥16 | Bank Up / Down | window moves by 16 |
| N=2 | ≥16 | Channel Up / Down | window moves by 1 |
| N=2 | ≥16 | Select strip 12 | jump to send-12 dest + switch to Receive |
| N=2 | 6 | enter Send | strips 1–6 live, 7–16 off; unit 2 display blank fields |
| N=2 | ≥16 | meters | both units meter; master LEDs = selected-track peak on every ProX unit |
| N=2 | ≥16 | Receive mode | all of the above via `P_SRCTRACK` |

### Regression baseline

Build the current `main` first, exercise the N=1 row of the matrix, record
the surface state (LEDs / faders / display text / VPOT rings). After the WP,
the N=1 row must match byte-for-byte (Exit Criterion 3).
