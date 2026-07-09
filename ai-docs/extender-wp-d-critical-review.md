# Extender Support WP-D — Critical Review

> Review date: 2026-07-09.
> Scope: `ai-docs/extender-wp-d-impl-plan.md`, checked against
> `src/modes/multitrack/MultiTrackMode.{h,cpp}`, `src/core/Tracks.{h,cpp}`,
> `src/core/CCSManager.{h,cpp}`, `src/core/CCSMode.cpp`.

## Summary

The WP-D plan targets the right banking semantics: whole-window scrolling with
active-anchor-adjusted step sizes, centralized clamp math, and removal of the
dead `g_mcu_list` guard. The eight steps are logically ordered and the
in-scope/deferred boundary is well drawn.

There is one **hard dependency barrier** (F1) that WP-D cannot cross without
WP-F, two **behavioral design questions** (F4, F11) that affect N=1 semantics,
and several **implementation detail gaps** (F2, F3, F5–F7, F10, F12). None are
hard blockers, but F4 and F11 in particular deserve a maintainer call before
implementation.

## Findings

### F1 — CHECKMODEANDCHANNEL ASSERT prevents loop widening (hard barrier)

`CCSManager.h:24-26`:

```cpp
#define CHECKMODEANDCHANNEL \
  ASSERT(channel >= 0 && channel < 9); \
  CHECKMODE
```

Every `setRecLED`, `setSoloLED`, `setMuteLED`, `setSelectLED`, `setFader`
call from MultiTrackMode goes through this macro. If Step 6 widens any of the
seven `channel < 9` loops, the first channel >= 9 hits the ASSERT and crashes.

The plan correctly notes "If WP-F is not present, leave those loops at `1..8`
and document that WP-D only corrects logical banking." But it should be
**stronger**: the ASSERT is a **runtime hard gate** that makes any accidental
widening fatal when the widened path is exercised. The plan should state that
**Step 6 retains `channel < 9` unconditionally until WP-F widens
CHECKMODEANDCHANNEL and the related input guards / fixed arrays**. No "if WP-F
is already present" conditional — the answer for WP-D is always NO.

**Recommended:** Make the seven loops a named checklist in the plan, not a
"maybe maybe not" conditional. Each loop's comment should reference
`// WP-F: widen to 1..Tracks::getNumberOfChannelStrips()`.

### F2 — frameUpdate() end-clamp guard may not trigger at N>1

Current code (`MultiTrackMode.cpp:48-53`):

```cpp
if (Tracks::instance()->getGlobalOffset() >= msize &&
    Tracks::instance()->getGlobalOffset() > 0) {
  Tracks::instance()->setGlobalOffset(std::max(msize - 8, 0));
  TrackList_UpdateAllExternalSurfaces();
  updateAssignmentDisplay();
}
```

The guard condition `globalOffset >= msize` fires when the bank has scrolled
past all tracks. The inner clamp `msize - 8` reduces to the last 8-track
window. At N>1 with 16 channels, this should be `msize - 16`. The plan Step 3
correctly replaces the hardcoded `8`, but there's a subtlety:

With the new clamped `setGlobalOffset()`, do we still need the outer guard
condition at all? If `setGlobalOffset()` clamps every call, the guard
becomes a dead check — offset can never exceed max useful offset. The plan
should clarify whether the guard is removed entirely (leaving only the
`TrackList_UpdateAllExternalSurfaces`+`updateAssignmentDisplay` call when
the offset changes) or retained as a secondary safety net.

### F3 — buttonFaderBanks() still uses total anchors, not active anchors

Current code (`MultiTrackMode.cpp:218, 231`):

```cpp
movesize += Tracks::instance()->getNumberOfAnchors();  // Bank Down
movesize -= Tracks::instance()->getNumberOfAnchors();  // Bank Up
```

This adjusts the move by TOTAL anchors, not active anchors. At N=1 (max
anchor = 8), total == active (no anchor can be >8). At N>1 with anchors in
slots 9+, total > active, so the bank step would over-subtract — shrinking
the effective window size incorrectly.

The plan Step 4 says "Remove direct `getNumberOfAnchors()` adjustments; the
helper already accounts for active anchors." This is correct: the new helpers
(`getEffectiveBankStep()`, `getLegacyPageStep()`) internally use
`getNumberOfActiveAnchors()`, so the mode code doesn't need to add/subtract
anchors at all. But the plan doesn't explicitly call out that removing these
lines is a **required part of the rewrite**, not optional cleanup. An
inattentive implementer might keep them and keep calling total anchors.

**Recommended:** Step 4 should explicitly state that the old
`movesize +/-= getNumberOfAnchors()` lines are **deleted** in favor of
`movesize = -/+ getEffectiveBankStep()` which already accounts for active
anchors.

### F4 — Shift + Bank step semantics change (N=1 behavioral delta)

Current code:
```cpp
if (isModifierPressed(VK_SHIFT))
  movesize = -8;     // hardcoded 8, NO anchor adjustment
else
  movesize = -Tracks::instance()->getNumberOfChannelStrips();
movesize += Tracks::instance()->getNumberOfAnchors();  // applied to BOTH paths
```

Wait — the `movesize += getNumberOfAnchors()` applies to **both** the Shift
and non-Shift path because it's outside the if/else. So the current N=1
Shift + Bank step IS adjusted for anchors:
- Shift + Bank Down: `-8 + totalAnchors` (e.g., with anchors at 1,4: step = -6)
- Non-Shift Bank Down: `-8 + totalAnchors` (same step! Shift is a no-op at N=1)

This means at N=1, Shift + Bank and normal Bank produce the **same** movesize
regardless of anchors (both `-8 + totalAnchors`). The plan's new design makes
them diverge:

| Action | New step |
|---|---|
| Bank Down | `-max(1, windowSize - activeAnchors)` |
| Shift+Bank Down | `-max(1, 8 - activeAnchors_1..8)` |

At N=1, `windowSize == 8`, so both formulas collapse to the same value (`-8 +
activeAnchors`). **N=1 identity is preserved**. At N>1, Bank moves by the full
window, Shift by the legacy 8-slots page. This is a sensible design but is a
**new behavior** for N>1 — previously there was no N>1, so "behavioral change"
is moot. The plan should note this explicitly: Shift+Bank as a per-8-channel
page step is a deliberate design choice, not inherited from old code.

### F5 — Double-clamping in buttonFaderBanks()

After the new offset is set (line 258), there are two more brute-force clamps:

```cpp
if (Tracks::instance()->getGlobalOffset() >= msize)
  Tracks::instance()->setGlobalOffset(msize - 1);
if (Tracks::instance()->getGlobalOffset() < 0)
  Tracks::instance()->setGlobalOffset(0);
```

These are bypassing the proposed centralized clamp in Step 2. If
`setGlobalOffset()` clamps internally, these lines are dead code. The plan
Step 4 says "Set the offset through the clamped setter" but doesn't explicitly
say to delete these post-clamp guards. They should be removed to avoid
confusion about who owns the clamp.

Additionally, `msize - 1` is the wrong upper bound. The correct bound is
`max(0, msize - freeSlots)` (the plan's `getMaxUsefulGlobalOffset` formula).
Leaving old code that clamps to a different value than the helper is a bug
waiting to happen if somehow offset bypasses the internal clamp.

### F6 — getMaxUsefulGlobalOffset formula edge case

The plan proposes:

```cpp
int freeSlots = std::max(1, m_numMCUChannels - activeAnchors);
int maxOffset = std::max(0, getNumMediaTracksOnMCU() - freeSlots);
```

When `getNumMediaTracksOnMCU() < freeSlots` (fewer visible tracks than the
adjusted window), maxOffset becomes negative from the subtraction, but
`std::max(0, negative)` returns 0. This is correct — bank should be at slot 0.

But for the intermediate case where `msize == freeSlots`, maxOffset = 0, also
correct (only one possible bank). The formula is sound.

The one gap: `getNumMediaTracksOnMCU()` includes the reflected parent track in
`MTOA_REFLECT_PLUS` mode. This parent occupies slot 1 of the bank, so the
effective track count for offset calculation is:
- With reflect-plus: `getNumMediaTracksOnMCU() - 1` children + 1 parent = same total
- The formula is unaffected because both are counted.

### F7 — Bank step can reach 0 if all slots are anchored

If `m_numMCUChannels == 8` and all 8 slots are anchored,
`activeAnchors == 8`, then `m_numMCUChannels - activeAnchors == 0`. The plan
correctly guards with `max(1, ...)`, so `getEffectiveBankStep()` returns 1.
Without this guard, the bank step would be 0, causing:

- Infinite loop in `moveSelectedTrack2MCU()` (already guarded by the WP-C
  `numChannels <= 0` check — wait, that uses `getNumberOfActiveAnchors()`,
  not `getEffectiveBankStep()`, so it has its own safety).

- Bank buttons would appear dead (movesize=0 → offset unchanged, button
  returns true but does nothing visible).

**The plan's `max(1, ...)` guard handles this correctly.** No fix needed.

But note: `moveSelectedTrack2MCU()` currently guards with `if
(getNumberOfActiveAnchors() == m_numMCUChannels) return;` — this uses active
anchors, which is correct. Do **not** replace this with
`getEffectiveBankStep() <= 1`: that helper intentionally returns at least 1
and cannot distinguish a fully anchored bank from a bank with one free slot.
If this condition needs a shared name later, add a dedicated
`isFullBankAnchored()` helper. Not blocking for WP-D, just a code consistency
note.

### F8 — Activate / base-track navigation call setGlobalOffset(0) directly

Several paths in MultiTrackMode set the offset to 0 directly:

- `activate()` → no explicit offset set (good — leaves offset as-is)
- `buttonSelectLong()` → `moveBaseTrack()` → `setGlobalOffset(0)` (line 424)
- `buttonGView()` → `moveBaseTrackToParent()` → `setGlobalOffset(0)` (line 448)

Setting offset to 0 after a base-track change is correct — the new base
track's children start at slot 1 of offset 0. This is always a valid offset
(0 ≤ maxUsefulOffset). With Step 2's clamping, these calls would be clamped
to 0 anyway if somehow negative, but they're already 0. **No issue here,**
just documenting for reviewer confidence.

### F9 — Performance: frameUpdate() runs every ~30Hz

The plan Step 3 notes "Prefer a helper that reports whether the offset
changed, so this path does not update every surface on every frame." The
concern is `TrackList_UpdateAllExternalSurfaces()` being called ~30 times per
second even when nothing changed.

Current code already has this problem — `frameUpdate()` calls
`TrackList_UpdateAllExternalSurfaces()` unconditionally when offset exceeds
msize. With centralized clamping, this conditional becomes "if offset was
just clamped by setGlobalOffset" — but `setGlobalOffset()` doesn't tell
callers whether it changed the offset.

**Recommended:** The plan should specify the exact interface change:
- Option A: `setGlobalOffset()` returns `bool` (true if changed).
- Option B: Add `bool clampCurrentGlobalOffset()` that clamps in-place and
  returns whether a change occurred.
- Option C: Keep `setGlobalOffset()` void, and the caller checks
  `getGlobalOffset()` before and after.

Option B is cleanest for the `frameUpdate()` path. The plan should pick one.

### F10 — getLegacyPageStep helper definition issue

The plan defines:
```
Legacy page step: max(1, 8 - active anchors in slots 1..8)
```

And says `getLegacyPageStep()` uses `8 - getNumberOfActiveAnchors(8)`.
But `getNumberOfActiveAnchors(8)` counts anchors where `anchor >= 1 && anchor
<= 8`. That's correct — it counts anchors in the first 8-channel block.
The step is `8 - this count`, min 1. This matches the current N=1 behavior.

For N>1, this means Shift+Bank moves by 8 minus anchors-in-first-block,
regardless of how many other anchors exist in slots 9+. This is a sensible
design: Shift means "give me the next 8-strip page, skipping anchored tracks."

### F11 — Clamp math: `msize - freeSlots` vs `msize - windowSize`

The plan's `getMaxUsefulGlobalOffset` formula:
```cpp
int maxOffset = std::max(0, msize - freeSlots);
```
where `freeSlots = max(1, windowSize - activeAnchors)`.

This is "last offset that fills as many of the active slots as possible,"
which is correct: if there are N tracks and freeSlots visible slots, the
last useful offset places tracks `N - freeSlots + 1 .. N` into slots
`1 .. freeSlots`. Offset zero-based, that's `N - freeSlots`, clamped to ≥0.

But there's a subtle anchoring scenario: suppose windowSize=8, anchors at
slots 1,2, freeSlots=6, msize=10. maxOffset = 10-6 = 4. At offset 4:
- Slot 1 = anchor
- Slot 2 = anchor
- Slots 3-8 show tracks 5-10
Correct — fills all 6 non-anchor slots.

Now what if msize=8 (exactly fills the bank)? maxOffset = 8-6 = 2. At offset 2:
- Slot 1 = anchor
- Slot 2 = anchor
- Slots 3-8 show tracks 3-8
That's 6 tracks, correct. No wasted slots.

Formula is sound. No issue.

### F12 — Runtime channel count change interaction with banking

When "release extenders" (future WP-G) shrinks `availableChannels()` at
runtime, `Tracks::adjust(newCount)` is called, which:
1. Updates `m_numMCUChannels`
2. Rebuilds `m_channelTracks` vector
3. Calls `updateTrackStates(m_numMCUChannels)`

But it does NOT clamp the current offset. If offset was 16 on a 16-channel
surface and the surface shrinks to 8, the offset stays at 16 — but the new
`m_channelTracks` has only 9 slots, so `getMediaTrackForChannel(16)` returns
NULL and channels 9-16 become unmapped. The next call to `setGlobalOffset()`
(with clamping from Step 2) would fix this. But if nothing triggers
`setGlobalOffset()` between the shrink and the next display update, the
surface shows empty slots 1-8 (because channels 9-16 of the now-too-small
vector map nowhere, AND channels 1-8 at offset 16 also map nowhere).

**Not a WP-D bug** — WP-G owns the "release" flow. But the plan should note
that `Tracks::adjust()` (or the shrink path) should call the centralized clamp
after resizing. A one-line addition in the adjust() path:
```cpp
if (m_numMCUChannels != clampedChannels) {
  m_numMCUChannels = clampedChannels;
  createChannelTrackVector();
  // Clamp offset to the new smaller window
  setGlobalOffset(m_globalOffset);  // or a dedicated clamp
}
```
This is cheap defensive code. Recommend adding it in WP-D (or noting it as
WP-G prerequisite).

---

## Step ordering recommendation

The plan's order (1-8) is correct. No reordering needed.

---

## Dependency on WP-F (revisited)

| WP-D task | Needs WP-F? |
|---|---|
| Tracks helpers (Step 1) | No |
| setGlobalOffset clamp (Step 2) | No |
| frameUpdate end clamp (Step 3) | No |
| buttonFaderBanks rewrite (Step 4) | No |
| Anchor boundary verification (Step 5) | No |
| **Widen MultiTrack loops (Step 6)** | **YES — CHECKMODEANDCHANNEL ASSERT** |
| Dead code removal (Step 7) | No |
| N=2 hardware verification (Step 8) | **YES — channels >8 can't reach hardware** |

Step 6 is gated on WP-F. Step 8's N=2 hardware verification is gated on
Step 6 (and thus WP-F). N=2 diagnostic verification (dumpMappingState, log
outputs) is feasible in WP-D without WP-F.

---

## Additional exit criteria (suggested)

- `getEffectiveBankStep()` returns `max(1, windowSize - getNumberOfActiveAnchors())`.
- `getLegacyPageStep()` returns `max(1, 8 - getNumberOfActiveAnchors(8))`.
- `buttonFaderBanks()` no longer calls `getNumberOfAnchors()` directly.
- `buttonFaderBanks()` no longer has post-clamp bounds guards (lines 261-264).
- A call to `setGlobalOffset(9999)` on an 8-channel surface with 10 tracks
  results in offset being clamped to `getMaxUsefulGlobalOffset()`, not 0.
- Since WP-D **cannot** widen the `channel < 9` loops (F1 above), all
  N>1 validation is diagnostic-only (`dumpMappingState`, log output).
  Full visual N>1 validation is deferred to WP-F + WP-D re-test.

---

## Bottom line

The WP-D plan is **structurally sound** with correct banking semantics.
Findings summary:

1. **F1 (blocker)** — loop widening is gated on WP-F, not optional. Make this
   explicit with a TODO comment per loop.
2. **F3 (gap)** — plan must explicitly state old anchor-add/subtract lines are
   deleted, not just "remove from this function."
3. **F5 (gap)** — post-clamp guards in buttonFaderBanks() must be deleted when
   centralized clamp is added.
4. **F4 (design)** — Shift+Bank step semantics need a one-sentence design
   rationale in the plan (why per-8-block).
5. **F9 (API design)** — pick a specific return-value or clamp-in-place
   interface for the frameUpdate() performance optimization.
6. **F12 (future-proofing)** — consider adding offset clamp to
   `Tracks::adjust()` shrink path, either in WP-D or noted for WP-G.

After F1-F5 are addressed, WP-D is ready to implement (with the caveat that
Step 6 defers to WP-F).
