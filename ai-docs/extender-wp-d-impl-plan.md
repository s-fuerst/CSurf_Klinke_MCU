# WP-D - MultiTrack Banking Semantics - Implementation Plan

> Implementation plan for work package WP-D of the extender-support effort.
> Master plan: `ai-docs/extender-support.md` (sections 5-7). WP-C reference:
> `ai-docs/extender-wp-c-impl-plan.md`.
>
> **This is a plan, not code.** No source changes have been made.
>
> **Prepared 2026-07-09.** This plan assumes the current WP-C direction:
> `Tracks` already owns a runtime logical channel count, maps channels
> `1..m_numMCUChannels`, treats channel `0` as the Reaper master, and keeps
> out-of-range anchors persisted but inactive.

## Goal

Define and implement the banking behavior for the combined MultiTrack surface.
With `N` configured active units, the visible bank is one logical window of
`availableChannels()` strips. Channel and bank buttons move that whole window,
not individual units.

After WP-D:

- `Tracks::globalOffset` means "offset of the combined logical bank".
- bank size is the runtime logical channel count, not a hard-coded `8`.
- Channel Up/Down move by one visible non-anchor slot.
- Bank Up/Down move by one whole active bank.
- Shift + Bank Up/Down keep the legacy one-MCU page step of `8` slots.
- active anchors reduce the effective scroll step only when they are inside
  the active logical range.
- offsets are clamped so the final bank is useful and does not intentionally
  leave an avoidable empty tail.
- persisted anchors outside the active range are ignored for banking math and
  are never cleared or clamped.
- the stale `g_mcu_list` / `CSurf_MCU::GetOffset()` bank guard is removed from
  MultiTrack banking.
- channel loops that call into `CCSManager`, VPOT, fader-touch, LED, display,
  or unit-routing code remain capped at `1..8` until WP-F removes the runtime
  channel barriers.

The N=1 behavior must remain identical except where it fixes already-known
dead compatibility code (`g_mcu_list` is empty after WP-A).

---

## Core Decision

WP-D uses **linked whole-bank scrolling**:

```text
1 unit:   [ 1  2  3  4  5  6  7  8 ]
2 units: [ 1  2  3  4  5  6  7  8 ][ 9 10 11 12 13 14 15 16 ]
3 units: [ 1  2  3  4  5  6  7  8 ][ 9 ... 16 ][17 ... 24]
```

Pressing Bank Up on a 16-channel setup advances by the active 16-strip window
minus active anchors. It does **not** scroll only the main unit, and it does
not keep extenders on fixed absolute track ranges.

`globalOffset` therefore remains one integer shared by all logical slots.
Physical units are just views into consecutive 8-slot blocks.

---

## In Scope

- MultiTrack banking and offset clamping.
- Replacing the remaining hard-coded `8` in MultiTrack bank/offset logic.
- Replacing the stale `g_mcu_list` max-fader-position guard.
- Adding small helper functions in `Tracks` if they make banking math
  explicit and reusable.
- Manual N=1 and N>1 validation through REAPER plus focused debug logging.
- Diagnostic N>1 mapping validation through `Tracks::dumpMappingState()`.

## Out of Scope

- Per-unit transport/global-display routing. That is WP-E.
- Widening every `CCSManager`, `VPOT_LED`, fader-touch, meter, and LED array.
  That is WP-F or per-mode work.
- Sending or receiving live strip traffic on channels above 8. Current
  `CCSManager` assertions, fixed `[9]` arrays, and unit-0-only `CSurf_MCU`
  fader/LED routing are runtime hard barriers owned by WP-F.
- Reworking Send/Receive, CommandMode, or PlugMode banking.
- Dynamic "release extenders" activation, except that helper names should be
  compatible with a future runtime `availableChannels()` that can shrink.
- Changing persisted anchor or quick-jump format.

---

## Current Code Hotspots

| Area | Current issue | File |
|---|---|---|
| End clamp | clamps to `msize - 8` | `src/modes/multitrack/MultiTrackMode.cpp:frameUpdate()` |
| Bank buttons | computes step from channel count but mixes total anchors and stale legacy guard | `src/modes/multitrack/MultiTrackMode.cpp:buttonFaderBanks()` |
| Legacy extender shim | scans `g_mcu_list` and `GetOffset()` although `g_mcu_list` is no longer authoritative | `src/modes/multitrack/MultiTrackMode.cpp:buttonFaderBanks()` |
| Offset setter | accepts any integer and rebuilds mapping immediately | `src/core/Tracks.cpp:setGlobalOffset()` |
| Capacity math | callers must remember active-anchor rules | `src/core/Tracks.cpp`, `src/core/Tracks.h` |
| Runtime width source | `CSurf_MCU::Run()` passes `availableChannels()` into `Tracks::adjust()` | `src/core/csurf_mcu.cpp:Run()` |
| Channel routing barrier | `CCSManager` asserts on channels above 8; touch/VPOT arrays are `[9]`; `CSurf_MCU::SetLED()` and `sendStripFader()` still route to unit 0 | `src/core/CCSManager.{h,cpp}`, `src/core/csurf_mcu.cpp` |

---

## Banking Semantics

### Definitions

- **Window size:** `Tracks::getNumberOfChannelStrips()`.
- **Active anchors:** anchors with `1 <= anchor <= window size`.
- **Effective bank step:** `max(1, window size - active anchors)`.
- **Legacy page step:** `max(1, 8 - active anchors in slots 1..8)` for
  Shift + Bank Up/Down.
- **Track count:** `Tracks::getNumMediaTracksOnMCU()`, including the reflected
  parent row when `MTO_REFLECT_FOLDER` requires it.

### Button Behavior

| Button | Move |
|---|---:|
| Channel Down | `-1` |
| Channel Up | `+1` |
| Bank Down | `-effectiveBankStep` |
| Bank Up | `+effectiveBankStep` |
| Shift + Bank Down | `-legacyPageStep` |
| Shift + Bank Up | `+legacyPageStep` |

This keeps the familiar one-page shift behavior while making unmodified bank
buttons match the real visible width.

At N=1, Shift + Bank and normal Bank both collapse to the legacy
`8 - activeAnchors` step. At N>1, Shift + Bank deliberately remains an
8-slot page move while normal Bank moves the whole active logical window.

### Clamp Behavior

The clamp must return an offset in `0..maxUsefulOffset`.

Recommended helper:

```cpp
int Tracks::getMaxUsefulGlobalOffset();
int Tracks::clampGlobalOffset(int offset);
bool Tracks::clampCurrentGlobalOffset();
```

The helper should make these cases explicit:

- no tracks or all visible: max useful offset is `0`;
- more tracks than visible capacity: max useful offset is the last offset that
  still fills as much of the active bank as possible;
- active anchors reduce free slots in the visible bank;
- out-of-range anchors do not affect the calculation.

For the first implementation, it is acceptable to use a conservative formula
based on `getNumMediaTracksOnMCU()` and the active effective bank size:

```cpp
int activeAnchors = getNumberOfActiveAnchors();
int freeSlots = std::max(1, m_numMCUChannels - activeAnchors);
int maxOffset = std::max(0, getNumMediaTracksOnMCU() - freeSlots);
```

If reflected-folder mode or anchor placement exposes an off-by-one in manual
testing, keep the helper and adjust only the formula. Do not scatter clamp
math back into mode code.

---

## Steps

### Step 1 - Add explicit banking helpers to `Tracks`

**Goal:** centralize the offset math so MultiTrack code does not duplicate
anchor and runtime-width rules.

- **Files:** `src/core/Tracks.h`, `src/core/Tracks.cpp`.
- Add:
  - `int getEffectiveBankStep();`
  - `int getLegacyPageStep();`
  - `int getMaxUsefulGlobalOffset();`
  - `int clampGlobalOffset(int offset);`
  - `bool clampCurrentGlobalOffset();`
- `getEffectiveBankStep()` uses
  `m_numMCUChannels - getNumberOfActiveAnchors()`.
- `getLegacyPageStep()` uses `8 - getNumberOfActiveAnchors(8)`.
- Both step helpers must return at least `1`.
- Do not use `getEffectiveBankStep() <= 1` as a proxy for "all slots are
  anchored"; the helper intentionally returns at least 1. Keep the existing
  active-anchor equality check, or introduce a separate `isFullBankAnchored()`
  helper if that condition needs a name.
- Keep helpers non-destructive: calculating a clamp must not rebuild the
  channel vector by itself.

### Step 2 - Clamp in `setGlobalOffset()`

**Goal:** make every caller benefit from consistent offset bounds.

- **Files:** `src/core/Tracks.cpp`.
- Change `setGlobalOffset(int globalOffset)` to clamp before storing and
  return `true` only when the stored offset changed. Existing callers may
  ignore the return value.
- Rebuild the channel vector only when the stored offset actually changes.
- Continue to call `updateTrackStates(getNumberOfChannelStrips())` after a
  real offset change.
- Avoid recursion: clamp helpers must not call `setGlobalOffset()`.
- N=1 checkpoint: Channel Up/Down and Bank Up/Down stay within the same
  visible ranges as before.
- In `Tracks::adjust()`, after a runtime channel-count change and
  `createChannelTrackVector()`, call the centralized clamp path so future
  shrink flows cannot leave `m_globalOffset` outside the useful range.

### Step 3 - Replace the `frameUpdate()` hard-coded end clamp

**Goal:** prevent the known `msize - 8` bug when the active bank is wider than
one MCU.

- **File:** `src/modes/multitrack/MultiTrackMode.cpp`.
- Remove the old `msize` guard/clamp:

  ```cpp
  if (Tracks::instance()->getGlobalOffset() >= msize &&
      Tracks::instance()->getGlobalOffset() > 0) {
    Tracks::instance()->setGlobalOffset(std::max(msize - 8, 0));
    TrackList_UpdateAllExternalSurfaces();
    updateAssignmentDisplay();
  }
  ```

  and replace it with the dedicated clamp helper:

  ```cpp
  if (Tracks::instance()->clampCurrentGlobalOffset()) {
    TrackList_UpdateAllExternalSurfaces();
    updateAssignmentDisplay();
  }
  ```

- Keep the existing `TrackList_UpdateAllExternalSurfaces()` and assignment
  display update after an actual clamp.
- Do not update every surface on every frame when the offset was already valid.

### Step 4 - Rewrite `buttonFaderBanks()` around helper steps

**Goal:** make bank buttons move the combined logical window.

- **File:** `src/modes/multitrack/MultiTrackMode.cpp`.
- Compute moves as:
  - `-Tracks::instance()->getEffectiveBankStep()` for Bank Down;
  - `+Tracks::instance()->getEffectiveBankStep()` for Bank Up;
  - `-Tracks::instance()->getLegacyPageStep()` for Shift + Bank Down;
  - `+Tracks::instance()->getLegacyPageStep()` for Shift + Bank Up;
  - `-1` / `+1` for Channel Down/Up.
- Remove the `g_mcu_list` scan and `maxfaderpos` guard entirely.
- Delete the old `movesize += Tracks::instance()->getNumberOfAnchors()` and
  `movesize -= Tracks::instance()->getNumberOfAnchors()` lines. The helper
  already accounts for active anchors and must use active anchors, not total
  persisted anchors.
- Delete the post-setter bounds guards that clamp to `msize - 1` or `0`; the
  centralized clamp owns the upper and lower bounds.
- Set the offset through the clamped setter and capture its boolean return.
- If the offset did not change, still return `true` for handled button presses
  but avoid unnecessary `TrackList_UpdateAllExternalSurfaces()`.

### Step 5 - Confirm anchor behavior at window boundaries

**Goal:** active and inactive anchors must affect banking exactly as designed.

- **Files:** usually `src/core/Tracks.cpp`; add debug-only diagnostics only if
  needed.
- Verify:
  - anchor in slot 1 reduces bank step for N=1 and N>1;
  - anchor in slot 9 affects a 16-channel setup but not an 8-channel setup;
  - anchor in slot 17 affects a 24-channel setup but not an 8- or 16-channel
    setup;
  - disabling anchors makes all helpers behave as if active-anchor count is
    zero;
  - no persisted anchor value is rewritten during bank moves.

### Step 6 - Keep MultiTrack display/control loops capped until WP-F

**Goal:** keep WP-D reviewable and avoid runtime assertions while preparing the
MultiTrack pilot path.

The current `MultiTrackMode` update loops still use `channel < 9`. Banking can
be corrected without widening these loops, but a 16-channel visible window is
not fully user-visible until the downstream routing paths accept channels
above 8.

WP-D must leave these loops at `1..8`. Widening them would call into
`CCSManager` setters that still use `CHECKMODEANDCHANNEL`, into input methods
that assert `channel <= 8`, into fixed `[9]` touch/VPOT arrays, and into
`CSurf_MCU` fader/LED routes that still target unit 0.

Add a short `// WP-F: widen to 1..Tracks::getNumberOfChannelStrips()` comment
near each retained loop so the follow-up work is explicit. Loops to revisit
when routing is ready:

- `updateRecLEDs()`
- `updateSoloLEDs()`
- `updateMuteLEDs()`
- `updateSelectLEDs()`
- `updateFaders()`
- `updateVPOTs()`
- `updateDisplay()`

Also keep helper methods such as `trackVolume()` and `trackPan()` at their
current channel bounds until WP-F routes faders and VPOTs per owning unit.

### Step 7 - Remove dead compatibility comments only when code is gone

**Goal:** keep historical notes useful and avoid misleading TODOs.

- When the `g_mcu_list` guard is removed, delete the local TODO in
  `buttonFaderBanks()`.
- Do not remove `CSurf_MCU::GetOffset()` yet if other legacy code still needs
  it or if a later cleanup WP owns it.
- Do not delete `g_mcu_list` as part of WP-D unless a separate search proves
  it has no remaining runtime purpose.

### Step 8 - Verification

**Build:**

```bash
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)")
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

Fully restart REAPER after copying the plugin.

**Manual N=1 regression:**

- 20-track project, no anchors: Bank Up moves from 1-8 to 9-16; Channel Up
  moves one track; end clamp does not leave avoidable empty slots.
- Anchors enabled in slots 1 and 4: Bank Up step is `6`; Shift + Bank Up step
  is also `6`.
- Anchors disabled: Bank Up step is `8`.
- Follow selected track still brings an off-screen selected track into view.

**Manual N=2 checks:**

- Configure two units so `availableChannels() == 16`.
- 40-track project, no anchors: Bank Up moves by `16`; Channel Up moves by
  `1`; final bank clamps to the last useful 16-track window.
- Anchor in slot 9: Bank Up step is `15`.
- Anchor in slot 17: no effect while only 16 channels are active.
- Releasing or removing the second unit in a future WP-G-style test shrinks
  the next `Tracks::adjust()` width to 8 and clamps the offset into an
  8-channel useful range.
- Treat these as logical mapping checks only. Full visual/hardware validation
  for channels 9+ is deferred until WP-F removes the `CCSManager` and
  unit-routing barriers.

**Diagnostics:**

- `Tracks::dumpMappingState()` should show:
  - the expected `numMCUChannels`;
  - the expected `globalOffset`;
  - no mapping above `numMCUChannels`;
  - active-anchor count matching the currently active width.

Remove temporary logs unless they are behind `MCU_DEBUG_LOG` and remain useful
for later extender WPs.

---

## Exit Criteria

- No hard-coded `msize - 8` remains in MultiTrack banking/clamping.
- `buttonFaderBanks()` no longer scans `g_mcu_list` or calls `GetOffset()`.
- Bank size comes from `Tracks::getNumberOfChannelStrips()` or a helper based
  on it.
- Active anchors, not total anchors, drive bank-step math.
- `buttonFaderBanks()` no longer calls `getNumberOfAnchors()` directly and no
  longer has local post-setter clamp guards.
- `setGlobalOffset()` clamps internally, returns whether the offset changed,
  and does not rebuild mapping when the clamped value is unchanged.
- `frameUpdate()` calls a clamp helper that reports whether work was needed.
- `Tracks::adjust()` clamps the current offset after a channel-count change.
- Out-of-range anchors remain persisted and inactive.
- N=1 banking behavior is unchanged except for removing the dead
  `g_mcu_list` guard.
- N=2 mapping can be observed through `Tracks::dumpMappingState()` and bank
  buttons move a 16-channel logical window. Hardware/display behavior above
  channel 8 remains deferred to WP-F.
- Linux Release build succeeds and the built plugin is copied to REAPER's
  `UserPlugins` directory after the implementation build.
