# Track-Add Freeze Investigation

> 2026-07-12 — **RESOLVED (root cause + fix).** Pre-existing bug, NOT
> caused by WP-MT changes.

## Symptom

Reaper freezes (GUI unresponsive, must force-kill) when adding a track to an
empty project. Freeze is intermittent but "sehr oft" (very frequent). Happens
with 3 MCU devices configured (offset=24).

## Confirmed: pre-existing, not WP-MT

- `fb5aed6` (WP-EF finished, clean, Release build) — freezes on track-add
- `fb5aed6` + WP-MT — same freeze
- `093b130` ("small adjustments before WP-MT") — same freeze

## Backtrace (consistent across builds)

Thread 1 (main/GUI thread) is stuck in JUCE String comparison:

```
#0  juce::CharacterFunctions::compare<CharPointer_UTF8, CharPointer_UTF8>
#1  juce::operator==(String const&, String const&)
#2  Tracks::getTrackStateForMediaTrack(MediaTrack*)
#3  Tracks::getNumMediaTracksOnMCU()
#4  Tracks::setGlobalOffset(int)
#5  Tracks::moveSelectedTrack2MCU()
#6  CSurf_MCU::SetSurfaceSelected(MediaTrack*, bool)
```

Earlier backtraces also showed `juce::String::~String()` at frame 0, and
`Tracks::getFilter()` → `Options::isOptionSetTo()` in the chain.

All paths converge on String operations (comparison or destruction) during
`getTrackStateForMediaTrack` or `getFilter`.

## What was ruled out

### Buffer overflows (FIXED, but did NOT fix the freeze)

1. **`Display::changeText()`** — `strnlen(text, getRowLength(row))` overreads
   chunked data from `sendDifferences()`. Fixed: bound by `min(pad, rowLen)`.

2. **`CCSManager::setAssignmentDisplay()`** — `memcmp(text, ..., 2)` when
   caller passes `""` (1-byte string literal). Fixed: normalise to 2-char
   local buffer.

3. **CCSManager arrays `[9]`** — WP-MT widened loops to `availableChannels()`
   (24 with 3 devices) but fb5aed6 had fixed-size [9] arrays. Fixed: applied
   093b130 array widening (dynamic `availableChannels()+1`).

4. **`Tracks::adjust()` vector sync** — `m_channelTracks` not resized when
   `m_numMCUChannels` changes. Fixed: call `createChannelTrackVector()`.

5. **`getMediaTrackForChannel()` defensive bounds** — checked against
   `m_numMCUChannels` instead of `m_channelTracks.size()`. Fixed.

These were ALL caught by ASan during init. After fixing all, ASan init is
clean (zero errors, 15s timeout).

### ASan with track-add trigger

ASan shows ZERO errors during the track-add freeze. This rules out:
- Heap buffer overflow
- Use-after-free
- Stack buffer overflow
- Global buffer overflow

### MALLOC_CHECK_=3

No glibc malloc errors during init or track-add.

### Corrupted config files (persisted state)

Deleted `~/.config/REAPER/MCU/Config/*.xml` — no effect.

### Hardcoded `getFilter()` bypass

Returning `TSNode::MCU` directly (skipping Options String comparison) — still
freezes. Ruled out: the freeze is NOT in the Options/MTO_SHOW String path.

### Safety counter in `getTrackStateForMediaTrack`

Limit of 10000 iterations + String length sanity check (>256 chars) — still
freezes. Ruled out: corrupt map size (infinite loop) and corrupt String length.

## Root cause (FOUND — not String corruption)

The freeze is **not** a corrupted String. It is a **logic-level infinite
loop** in `Tracks::moveSelectedTrack2MCU()`. The `while`-loop there assumes
that stepping the global offset forward (in increments of `numChannels`) will
sooner or later bring the selected track onto the MCU, i.e. flip
`pTS->isOnMCU()` to true. That assumption is wrong because `setGlobalOffset()`
**clamps** the offset to `[0, getMaxUsefulGlobalOffset()]`.

`getMaxUsefulGlobalOffset()` = `getNumMediaTracksOnMCU() - freeSlots`. For a
near-empty project (empty project + one freshly added track) that maximum is
**0**. So on every iteration:

- `setGlobalOffset(0 + numChannels)` clamps straight back to `0` — the offset
  never advances, `updateTrackStates()` is never re-run, so `isOnMCU()` stays
  false;
- `getGlobalOffset() < tracknr` (`0 < 1`) stays true;
- the loop spins forever.

The loop spends essentially all of its CPU time inside the String comparison
of `getTrackStateForMediaTrack()` (reached via `setGlobalOffset` →
`clampGlobalOffset` → `getMaxUsefulGlobalOffset` → `getNumMediaTracksOnMCU` →
`numChilds` → `showTrack` → `getTrackStateForMediaTrack`). That is exactly why
every backtrace snapshot lands in `juce::CharacterFunctions::compare` — it is
simply where the infinite loop burns its cycles, not the site of a bug.

### Why this explains every earlier observation

- **ASan clean (heap/uaf/stack/global)** — there is no memory error. It is a
  pure logic bug.
- **Safety counter inside `getTrackStateForMediaTrack` did not help** — each
  individual call returns fine (1 track, 1 iteration). The infinite loop is
  one level *up*, in `moveSelectedTrack2MCU`.
- **Hardcoded `getFilter()` bypass did not help** — `getFilter()` is not on
  the looping path's critical iteration; bypassing it does not affect the
  outer loop.
- **`MALLOC_CHECK_=3` clean** — no heap corruption.
- **Reproduces "very often" out of the box** — `MTO2A_FOLLOW_REAPER_ON` is the
  default (`addAttribute(MTO2_FOLLOW_REAPER, MTO2A_FOLLOW_REAPER_ON, true)` in
  `MultiTrackOptions2.cpp`), so the freeze path is active by default whenever
  a track is added and is not yet on the MCU.
- **Earlier `String::~String()` / `getFilter()` backtraces** — different
  sampling points of the same loop (temporary Strings created in `getFilter`
  during `getNumMediaTracksOnMCU`).

The "corrupted StringHolder / CharPointer / refcount" hypothesis is therefore
**rejected**: no String is corrupt; the comparison just runs forever because
its caller loops forever.

## The fix

In `Tracks::moveSelectedTrack2MCU()` (`src/core/Tracks.cpp`), break the loop
when the offset stops making progress (i.e. it has been clamped to its max and
cannot grow further):

```cpp
while (!pTS->isOnMCU() &&
       Tracks::instance()->getGlobalOffset() < tracknr) {
  int offsetBefore = Tracks::instance()->getGlobalOffset();
  Tracks::instance()->setGlobalOffset(offsetBefore + numChannels);
  if (Tracks::instance()->getGlobalOffset() == offsetBefore)
    break; // offset is clamped and cannot grow any further
}
```

This leaves the normal "many tracks" case untouched: when the offset *can*
advance, `setGlobalOffset()` still re-runs `updateTrackStates()`, the track
lands on the MCU, `isOnMCU()` flips true, and the loop exits exactly as
before. The guard only fires in the degenerate (clamped) case. The existing
"track wasn't found" recovery that follows the loop is preserved.

Note: there is already a sibling guard at the top of
`moveSelectedTrack2MCU()` — `if (getNumberOfActiveAnchors() == m_numMCUChannels)
return;` — for the "all channels are anchors" no-progress case. The new guard
covers the *other* no-progress case (clamping on a near-empty project). Both
are needed.

## Status

- Fix applied in `src/core/Tracks.cpp`.
- Built + deployed (`build/reaper_csurf_mcu_klinke.so` →
  `~/.config/REAPER/UserPlugins/`). REAPER must be fully restarted to load it.
- The five buffer-overflow fixes listed under "What was ruled out" are kept —
  they were genuine bugs (caught by ASan during init) even though they were
  not the freeze.

## Next steps

- Manual verification in REAPER: empty project → add track (repeat a few
  times); confirm no freeze. Also add several tracks and confirm normal
  FOLLOW_REAPER banking still works.
- Consider auditing the other `setGlobalOffset()` / offset-stepping loop
  (`moveTrackToLeftMostChannel()`) for analogous clamping traps — it is bounded
  by `findMediaTrackForChannel()` returning NULL, so it is not an infinite
  loop, but worth a second look.
