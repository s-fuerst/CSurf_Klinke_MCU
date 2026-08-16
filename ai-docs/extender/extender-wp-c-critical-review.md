# Extender Support WP-C — Critical Review

> Review date: 2026-07-08.
> Scope: `ai-docs/extender-wp-c-impl-plan.md`, checked against
> `src/core/Tracks.h`, `src/core/Tracks.cpp`, and
> `src/modes/multitrack/MultiTrackMode.cpp`.

## Summary

The WP-C plan targets the right abstraction: make `Tracks` aware of a dynamic
logical surface width instead of hard-coding 8. The nine steps decompose the
work cleanly, and the in-scope/deferred boundaries are well drawn.

There is one **structural gap** (F1), two **step ordering issues** (F2/F3),
and several **omissions and edge-case gaps** in the detailed step descriptions
(F4–F9). All are fixable without changing the scope.

## Findings

### F1 — `adjust()` never calls `createChannelTrackVector()` (data-flow gap)

The plan’s Step 1 says *“if the value changes, store it and rebuild
`m_channelTracks`.”* But the current `adjust()` implementation calls ONLY
`updateTrackStates(numMCUChannels)` — it never invokes
`createChannelTrackVector()`.

`updateTrackStates()` reads the channel mapping via
`getMediaTrackForChannel(c)`, which returns `m_channelTracks[c]`. If
`m_channelTracks` was never resized to accommodate the larger channel
count (it stays at 9 slots from the initial `setDisplayHandler()`→
`createChannelTrackVector()` call), then `getMediaTrackForChannel(9)`
returns NULL — the track at logical slot 9 is never found. Channels 1–8
continue to work, but channels above 8 silently produce empty mapping.

This means WP-C would appear to work (N=1 regression passes) but the
wider mapping is dead on arrival.

**Required fix:** `adjust()` must explicitly call `createChannelTrackVector()`
when `m_numMCUChannels` changes — not just hint at “rebuild.” The simplest
correct sequence inside `adjust()` after storing the new value:

```cpp
if (m_numMCUChannels != oldValue) {
  m_numMCUChannels = clampedNewValue;
  createChannelTrackVector();
}
updateTrackStates(m_numMCUChannels);
```

Alternatively, `createChannelTrackVector()` could be rolled into
`updateTrackStates()` (they are always called together), but changing the
existing contract is riskier than adding one call.

### F2 — Step 3 (offset removal) depends on Step 4 (active anchors) but is listed first

Step 3 proposes a new formula for `findMediaTrackForChannel()`:

```cpp
int channelWithOffset =
    channel + m_globalOffset - numActiveAnchorsWithLowerChannel;
```

The concept `numActiveAnchorsWithLowerChannel` — counting only anchors with
`1 <= anchor <= m_numMCUChannels` — requires the active-anchor infrastructure
from Step 4, which is the next step. Either reorder (Step 4 before Step 3) or
define the helper inline and split it out in Step 4.

Additionally, the `anchor == channel` early-return branch in the same function
must also filter by active range: if an anchor is at channel 17 on an 8-channel
surface, `findMediaTrackForChannel(17)` must **not** return the anchored track.
The plan’s “ignore for mapping but do not clear or clamp” principle is correct
but must be applied to this return path too, not just to the counting path.

### F3 — `moveTrackToLeftMostChannel()` has the same anchor-filtering gap

Inside the do-while loop of `moveTrackToLeftMostChannel()`, after the target
track is found, a nested `for` loop counts anchors for the offset calculation:

```cpp
for (int j = 1; j < childWithTrack; j++) {
  if (...->getAnchorChannel() > 0 ...)
    numAnchors++;
}
Tracks::instance()->setGlobalOffset(childWithTrack - numAnchors - 1);
```

This counts ALL anchors, including those mapped to channels above
`m_numMCUChannels`. For a 16-channel surface, an anchor at channel 17 would
inflate `numAnchors` and produce a wrong `m_globalOffset`. The plan’s Step 7
mentions *“use `getNumberOfActiveAnchors()` where the code currently derives an
offset from anchor count”* but does not call out this specific loop — it is
inside a different function and counts anchors iteratively (not via
`getNumberOfAnchors()`). It needs the same active-range guard.

### F4 — `moveSelectedTrack2MCU()` edge cases with `numChannels == 0` or `numChannels < 0`

Step 5 correctly says *“if `numChannels <= 0`, return early to avoid an
infinite loop.”* But the early-return position matters:

- The function currently calls `setGlobalOffset(0)` (with side effects:
  rebuild channel vector + update TCP/MCP) before entering the while-loop.
- If `numChannels <= 0`, the early return must fire **before**
  `setGlobalOffset(0)`, otherwise the bank is reset to slot 0 for no reason.

The plan doesn’t specify the early-return position. Recommended:
after computing `numChannels` and **before** `setGlobalOffset(0)`.

Additionally, `numChannels` can be **negative** when
`getNumberOfActiveAnchors() > m_numMCUChannels`. While this cannot happen at
N=1 (max anchors = 8) and at N>1 would require 17+ anchors on a 16-channel
surface, the guard should handle `<= 0` as stated, not just `== 0`.

### F5 — `getNumberOfActiveAnchors()` may change behavior at N=1

The plan says `getNumberOfAnchors()` keeps returning TOTAL anchors (all
configured, regardless of range). External callers (`MultiTrackMode`,
`MultiTrackSelector`, `MultiTrackOptions`) continue to receive total counts.

The new `getNumberOfActiveAnchors()` returns only anchors within
`1..m_numMCUChannels`. At N=1 (`m_numMCUChannels == 8`), this is a subset of
total anchors — any anchor at channel 9+ would be excluded. But no anchor can
be at channel 9+ in an N=1 project because the UI only offers 1–8. So the two
functions return identical results under N=1. This is correct — the split is
future-proof and has zero N=1 impact.

The plan should explicitly note this N=1 identity guarantee for reviewer
confidence.

### F6 — Hard-coded `8` in `MultiTrackMode::activate()` (deferred to WP-D, but noted)

```cpp
// MultiTrackMode.cpp:50
Tracks::instance()->setGlobalOffset(std::max(msize - 8, 0));
```

When `m_numMCUChannels` grows to 16, a bank that has scrolled past the last
track would be clamped to `msize - 8` instead of `msize - 16`, leaving 8
gratuitous empty slots at the end. The plan correctly defers this to WP-D
(“full bank-scroll semantics are WP-D”). This finding is recorded for
traceability, not as a WP-C correction.

### F7 — `MultiTrackMode::buttonFaderBanks()` `maxfaderpos` loop is stale (deferred but visible)

The `g_mcu_list`-scanning loop (lines 241–245) computes `maxfaderpos` by
summing per-unit offsets. Since WP-A kept `g_mcu_list` as an empty shim,
`maxfaderpos` is always 0. The bank-scroll guard:

```cpp
if (movesize > 1 && (Tracks::instance()->getGlobalOffset() + maxfaderpos >= msize))
  return true;
```

collapses to `getGlobalOffset() >= msize`. This is harmless for N=1. For N>1
it loses the original intent (preventing a partial bank at the end). The plan
defers this to WP-D. Acceptable for WP-C.

### F8 — `moveTrackToLeftMostChannel()` has a missing upper bound

Step 7 says *“bound any search that depends on visible slots to
`m_numMCUChannels`.”* But the do-while loop in `moveTrackToLeftMostChannel()`
is **not** bounded by `m_numMCUChannels` — it scans through the TSGraph via
`findMediaTrackForChannel(++childWithTrack)` until NULL. This is correct for
the function’s purpose (finding ANY track’s position to scroll to it), and
the TSGraph child count provides an implicit bound via the NULL return.
The plan’s wording is misleading — the loop is bounded by track count, not
channel count, and that’s correct.

What SHOULD be bounded is the **nested anchor-counting loop** inside the
found-track branch — see F3 above.

### F9 — `findMediaTrackForChannel` skip-anchor logic for `channelWithOffset` when anchor is the current base track

In the reflect-folder-plus path:

```cpp
if (channelWithOffset == 1) {
  return m_pCurrentBaseTrack;
}
channelWithOffset--;
```

The `--channelWithOffset` skips the base track (which counts as slot 1).
The base track’s anchor state is separately checked via
`getAnchorChannel() == 0`. This logic is independent of N and requires no
WP-C change. Noted for reviewer awareness — no action needed.

---

## Step ordering recommendation

The current order (1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9) should become:

| New order | Old | Reason |
|---|---|---|
| 1 | 1 | Runtime channel count + `createChannelTrackVector()` call |
| 2 | 2 | Dynamic resize |
| 3 | 4 | Active-anchor infrastructure (needed by 4/5) |
| 4 | 3 | Offset removal + anchor-filtering (now has active-anchor helpers) |
| 5 | 5 | N-aware follow-selection (needs active anchors + offset removal) |
| 6 | 6 | Track-state marking (independent, but logically after above) |
| 7 | 7 | `moveTrackToLeftMostChannel()` bounds (needs active anchors) |
| 8 | 8 | Legacy offset cleanup |
| 9 | 9 | Diagnostics |

Steps 4 and 5 are genuinely sequential on 3 (the new Step 4). The plan’s
original Steps 3 and 4 were a dependency inversion.

---

## Exit criteria — additions

The plan’s exit criteria are good but should add:

- `Tracks::adjust()` calls `createChannelTrackVector()` when
  `m_numMCUChannels` changes (smoke-test: `m_channelTracks.size()` equals
  `m_numMCUChannels + 1` after `adjust(N)`).
- `findMediaTrackForChannel(channel > m_numMCUChannels)` returns NULL even
  when an anchor is set to that channel (anchor-is-inactive check).
- `moveTrackToLeftMostChannel()` anchor-counting loop only counts anchors in
  `1..m_numMCUChannels` (verified via the `numAnchors` calculation inside
  the found-track branch).

---

## Bottom line

The WP-C plan is **structurally sound** and appropriately scoped. The findings
are all implementation-level gaps that affect correctness at the N>1 boundary
but cost zero N=1 regression risk:

1. **F1 is the only blocking issue** — without the `createChannelTrackVector()`
   call inside `adjust()`, the wider mapping is silently dead.
2. **F2/F3** are step-ordering and edge-case omissions — fixable by reordering
   and adding one more active-range check.
3. **F4–F9** are quality/safety refinements, not blockers.

After addressing F1–F3, WP-C is ready to implement.
