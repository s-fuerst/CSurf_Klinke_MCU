# WP-C - Tracks Singleton Refactor - Implementation Plan

> Implementation plan for work package WP-C of the extender-support effort.
> Master plan: `ai-docs/extender-support.md` (sections 5-6). WP-A reference:
> `ai-docs/extender-wp-a-impl-plan.md`. WP-B reference:
> `ai-docs/extender-wp-b-impl-plan.md`.
>
> **This is a plan, not code.** No source changes have been made.
>
> **Revised 2026-07-09** per
> `ai-docs/extender-wp-c-critical-review.md`: active-anchor infrastructure now
> precedes offset removal, `adjust()` explicitly rebuilds the channel vector
> when the runtime width changes, and the exit criteria cover the N>1
> data-flow gaps found in review.
>
> WP-C is the first real logical-channel-width change. WP-A extracted the
> physical-unit layer. WP-B can persist/configure up to 8 rows. WP-C makes the
> `Tracks` singleton understand the logical surface width (`N * 8`) instead of
> assuming a single 8-channel MCU.

## Goal

Refactor `Tracks` so it maps Reaper tracks to **logical surface slots**
`1..availableChannels()` plus channel `0` for the Reaper master track. The
mapping must no longer depend on a per-`CSurf_MCU` offset and must no longer
hard-code 8 user channels.

After WP-C:

- `Tracks::adjust(numMCUChannels)` stores and uses the runtime channel count.
- `m_channelTracks` is sized to `numMCUChannels + 1`.
- `getMediaTrackForChannel()` and `getChannelForMediaTrack()` work for
  channels above 8.
- `findMediaTrackForChannel()` uses logical channel numbers directly and no
  longer calls `m_pMCU->GetOffset()`.
- anchor and quick-jump channels are treated as absolute surface slots
  `1..numMCUChannels`.
- out-of-range anchors/quick-jumps remain persisted but inactive.

WP-C still does **not** make every mode work with extenders. `CCSManager`
channel bounds, `VPOT_LED`, meter routing, per-unit `ButtonManager` state, and
per-mode N-channel loops are WP-F/per-mode work. WP-C only makes the central
track mapping ready.

---

## Golden thread

The singleton itself is not the problem. With the chosen architecture there is
one logical `CSurf_MCU`, so one global `Tracks` manager is acceptable.

The problem is that `Tracks` currently mixes global track state with a
single-MCU 8-channel mapping:

- `m_channelTracks.resize(9)`.
- loops use `i < 9`.
- `getMediaTrackForChannel(channel >= 9)` returns `NULL`.
- `findMediaTrackForChannel()` adds `m_pMCU->GetOffset()`.
- follow-selection uses `8 - getNumberOfAnchors()`.
- `getNumberOfChannelStrips()` always returns `8`.

WP-C keeps the singleton but turns it into a logical-surface mapper.

---

## Build / verify contract

Every step should build clean on Linux:

```bash
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)")
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

Then fully restart REAPER and run the N=1 regression first. For N>1 testing,
configure only the MIDI devices you intentionally want active. With current
WP-B/WP-F boundaries, hardware events above channel 8 may still be gated or
blocked elsewhere; WP-C's direct validation is primarily the track mapping
state and display/mixer updates driven by `Tracks`.

---

## Current code hotspots

| Area | Current problem | File |
|---|---|---|
| Channel vector | `m_channelTracks.resize(9)` and `for (i=1; i<9)` | `src/core/Tracks.cpp:createChannelTrackVector()` |
| Lookup | channels `>=9` return `NULL` | `src/core/Tracks.cpp:getMediaTrackForChannel()` |
| Reverse lookup | loops `1..8`; also has an assignment typo `=` instead of comparison | `src/core/Tracks.cpp:getChannelForMediaTrack()` |
| Mapping formula | adds `m_pMCU->GetOffset()` | `src/core/Tracks.cpp:findMediaTrackForChannel()` |
| Follow selected track | uses `8 - getNumberOfAnchors()` | `src/core/Tracks.cpp:moveSelectedTrack2MCU()` |
| Channel count | hard returns `8` | `src/core/Tracks.cpp:getNumberOfChannelStrips()` |
| Anchors | counts all anchors, including anchors outside the active range | `src/core/Tracks.cpp:getNumberOfAnchors()` |
| Legacy mode bank code | uses `GetSize()`, `g_mcu_list`, `GetOffset()` | `src/modes/multitrack/MultiTrackMode.cpp:buttonFaderBanks()` |

---

## Steps

### Step 1 - Add explicit runtime channel count to `Tracks`

**Goal:** make `Tracks` remember the logical surface width passed by
`CSurf_MCU::Run()`.

- **Files:** `src/core/Tracks.h`, `src/core/Tracks.cpp`.
- **Add member:** `int m_numMCUChannels;`
- **Constructor default:** initialize to `8`.
- **Add helper:** `int getNumMCUChannels() const;` or reuse
  `getNumberOfChannelStrips()` as the public getter.
- **Change `getNumberOfChannelStrips()`:** return `m_numMCUChannels` instead
  of hard-coded `8`.
- **Change `adjust(int numMCUChannels)`:**
  - clamp to at least `8` and at most `64` for now;
  - if the value changes, store it and explicitly call
    `createChannelTrackVector()`;
  - continue to call `updateTrackStates(m_numMCUChannels)`.
- **Required sequence inside `adjust()`:**

  ```cpp
  int clampedChannels = std::max(8, std::min(numMCUChannels, 64));
  if (m_numMCUChannels != clampedChannels) {
    m_numMCUChannels = clampedChannels;
    createChannelTrackVector();
  }
  updateTrackStates(m_numMCUChannels);
  ```

  `updateTrackStates()` reads the mapping through `getMediaTrackForChannel()`;
  without the rebuild, channels above the old vector size stay unmapped.
- **N=1 checkpoint:** `m_numMCUChannels == 8`; behavior unchanged.

### Step 2 - Resize `m_channelTracks` dynamically

**Goal:** build channel mapping for `0..m_numMCUChannels`.

- **Files:** `src/core/Tracks.cpp`.
- **Change `createChannelTrackVector()`:**
  - `m_channelTracks.resize(m_numMCUChannels + 1);`
  - keep `m_channelTracks[0] = CSurf_TrackFromID(0, false);`
  - loop `for (int i = 1; i <= m_numMCUChannels; ++i)`.
- **Change `getMediaTrackForChannel(int channel)`:**
  - accept `0..m_numMCUChannels`;
  - return `NULL` for negative or out-of-range channels;
  - do not assume vector size 9.
- **Change `getChannelForMediaTrack(MediaTrack *pMT)`:**
  - loop `1..m_numMCUChannels`;
  - fix the existing assignment typo (`=` must become `==`);
  - return `-1` if not mapped.
- **N=1 checkpoint:** vector size remains 9; reverse lookup bug is fixed.

### Step 3 - Split total anchors from active anchors

**Goal:** calculations that depend on visible surface capacity must count only
anchors whose slot exists right now.

- **Files:** `src/core/Tracks.h`, `src/core/Tracks.cpp`.
- **Keep `getNumberOfAnchors()`** if existing UI code expects the total number
  of configured anchors.
- **Add:** `int getNumberOfActiveAnchors(int maxChannel = -1);`
  - if anchors are disabled, return `0`;
  - default `maxChannel` to `m_numMCUChannels`;
  - count anchors with `1 <= anchor <= maxChannel`.
- **N=1 identity guarantee:** because current N=1 UI only creates anchors in
  slots `1..8`, `getNumberOfActiveAnchors()` and `getNumberOfAnchors()` return
  the same value under a normal single-unit project.
- **Do not destroy persisted values:** anchor `17` is inactive on an 8-channel
  setup and becomes active again when the surface has at least 17 channels.

### Step 4 - Remove per-MCU offset from track mapping

**Goal:** logical channel `g` maps directly to slot `g` of the combined
surface. No `CSurf_MCU::GetOffset()` is involved.

- **Files:** `src/core/Tracks.cpp`.
- **Change `findMediaTrackForChannel(int channel)`:**
  - return `NULL` immediately when `channel < 1` or
    `channel > m_numMCUChannels`;
  - remove `m_pMCU->GetOffset()` from `channelWithOffset`;
  - new formula:

    ```cpp
    int channelWithOffset =
        channel + m_globalOffset - numActiveAnchorsWithLowerChannel;
    ```

  - count lower anchors only when they are active in the current range
    (`1 <= anchor <= m_numMCUChannels`).
- **Filter anchor direct hits too:** the early `anchor == channel` return must
  only fire for active anchors. `findMediaTrackForChannel(channel >
  m_numMCUChannels)` must return `NULL` even if a persisted anchor uses that
  channel.
- **Out-of-range anchors:** if a track has `anchor > m_numMCUChannels`, ignore
  it for the current mapping but do not clear or clamp it.
- **Keep `m_pMCU` for now:** it is still used by other `Tracks` methods
  (`updateVUactive()` calls `m_pMCU->SomethingSoloed()`, editor code uses
  `Tracks::getMCU()`). Removing that pointer is not required for WP-C.
- **N=1 checkpoint:** with `m_globalOffset` unchanged, mapping should match
  today exactly for channels 1..8.

### Step 5 - Make selected-track-follow N-aware

**Goal:** the "follow selected track" path should move the logical bank by the
current surface width minus active anchors.

- **Files:** `src/core/Tracks.cpp`.
- **Change `moveSelectedTrack2MCU()`:**
  - replace `if (getNumberOfAnchors() == 8)` with a capacity check against
    `m_numMCUChannels`;
  - replace `int numChannels = 8 - getNumberOfAnchors();` with
    `int numChannels = m_numMCUChannels - getNumberOfActiveAnchors();`;
  - if `numChannels <= 0`, return early to avoid an infinite loop;
  - the `numChannels <= 0` guard must run before `setGlobalOffset(0)`, so a
    fully anchored or otherwise invalid effective capacity does not reset the
    current bank as a side effect.
- **Verify:** selecting a track outside the visible bank moves the bank by 8
  for N=1, by 16 for two configured units, etc.

### Step 6 - Update track-state marking over the dynamic range

**Goal:** `TrackState::isOnMCU()` and `getOnMCUChannel()` reflect the combined
logical surface.

- **Files:** `src/core/Tracks.cpp`.
- **Current code already accepts `numMCUChannels`:**
  `updateTrackStates(int numMCUChannels)` loops `1..numMCUChannels`.
- **Verify after Steps 1-4:**
  - `getMediaTrackForChannel(c)` now works for channels above 8;
  - `setIsOnMCUChannel(c)` receives absolute slots `1..N*8`;
  - TCP/MCP adjustment based on `isOnMCU()` can include tracks shown on
    extender slots.
- **Add guards:** if `numMCUChannels <= 0`, no track should be marked on MCU.

### Step 7 - Make `moveTrackToLeftMostChannel()` anchor-aware

**Goal:** preserve the existing track-search semantics while making the offset
calculation respect only active anchors.

- **Files:** `src/core/Tracks.cpp`.
- **Review loop:**
  - it increments `childWithTrack` and calls `findMediaTrackForChannel()`;
  - keep the semantic goal: move the selected track to logical slot 1 of the
    current bank;
  - keep the outer search bounded by track graph exhaustion (`NULL` from
    `findMediaTrackForChannel()`), not by `m_numMCUChannels`, because this
    function may need to find a track beyond the currently visible surface;
  - in the nested anchor-counting loop inside the found-track branch, count
    only anchors whose channel is active in `1..m_numMCUChannels`;
  - do not count persisted anchors above `m_numMCUChannels`, because they
    would inflate the computed `m_globalOffset`.
- **N=1 checkpoint:** quick-jump / selector behavior unchanged.

### Step 8 - Minimize legacy `GetOffset()` dependency

**Goal:** after WP-C, `Tracks` no longer depends on the old extender-instance
offset shim.

- **Files:** `src/core/Tracks.cpp`, maybe `src/core/Tracks.h`.
- **Required:** `git grep "GetOffset" src/core/Tracks.*` should return no
  hits.
- **Do not delete `CSurf_MCU::GetOffset()` yet:** `MultiTrackMode` still uses
  `GetOffset()` and `g_mcu_list` in its bank-button code. Removing those
  symbols belongs with WP-D/per-mode cleanup unless this WP deliberately pulls
  that small cleanup forward.
- **Recommended small cleanup if pulled forward:**
  - in `MultiTrackMode::buttonFaderBanks()`, replace the `g_mcu_list` scan
    for `maxfaderpos` with `Tracks::instance()->getNumberOfChannelStrips()`;
  - replace `m_pCCSManager->getMCU()->GetSize()` with
    `Tracks::instance()->getNumberOfChannelStrips()`.
- **Boundary:** full bank-scroll semantics are WP-D. WP-C may only do the
  minimal compile/runtime cleanup needed to avoid stale offset assumptions.

### Step 9 - Add focused diagnostics/tests

**Goal:** make the refactor reviewable even without all hardware paths ready.

- **Preferred:** add a small debug-only helper or temporary log during local
  verification that prints:
  - `m_numMCUChannels`;
  - `m_globalOffset`;
  - channels `0..m_numMCUChannels` and their track numbers/names;
  - active anchor count.
- **Manual REAPER checks:**
  - N=1 config: channel mapping, bank scroll, follow selected track, anchors,
    quick-jumps unchanged.
  - N=2 config with only unit 1 MIDI active if desired: `Tracks` can still be
    asked to map 16 slots without crashing.
  - Project with an anchor on slot 9 or 17: anchor is inactive on 8 channels,
    active when `m_numMCUChannels` covers it.
- **Do not leave noisy logs enabled** unless they are behind `MCU_DEBUG_LOG`
  and useful for later WPs.

---

## In-scope vs deferred

**In-scope for WP-C:**

- Dynamic channel count in `Tracks`.
- `m_channelTracks` vector sized to `N*8 + 1`.
- logical channel lookup/reverse lookup above 8.
- removal of `m_pMCU->GetOffset()` from `Tracks` mapping.
- active-anchor handling for `1..N*8`.
- selected-track-follow capacity based on the runtime channel count.

**Explicitly not in-scope for WP-C:**

- Removing `Tracks` as a singleton.
- Removing every `Tracks::getMCU()` user.
- Full bank-scroll behavior design (WP-D), beyond minimal stale-shim cleanup.
- `CCSManager` channel widening and touch arrays (WP-F).
- `VPOT_LED` / meter routing (WP-F).
- per-unit `ButtonManager` state (WP-F).
- per-mode N-channel loop expansion and display layout.
- dynamic "release extenders" activation.

---

## Risks and mitigations

### R1 - Changing `availableChannels()` before WP-F

WP-C makes `Tracks` handle `N*8`, but `CCSManager` may still reject hardware
events above channel 8. Mitigation: keep the WP-B input gate for unit 2+ until
WP-F widens `CCSManager`; validate WP-C through mapping/state paths first.

### R2 - Anchors outside the active range

If implementation clamps anchors, project data is lost. Mitigation: never
write back a changed anchor just because it is out of range. Treat it as
inactive in calculations only.

### R3 - Bank/follow loops can become infinite

Any loop that advances by "available channels minus anchors" must handle the
case where active anchors fill the whole surface. Mitigation: if effective
capacity is `<= 0`, return early before changing the global offset.

### R4 - Existing behavior depends on total anchor count

Some UI/editor code may display or edit total anchors, not active anchors.
Mitigation: keep `getNumberOfAnchors()` as total and introduce a separate
`getNumberOfActiveAnchors()`.

### R5 - `getChannelForMediaTrack()` behavior changes because of typo fix

The current code uses assignment in the condition. Fixing it is necessary for
correct reverse lookup and is in-scope because widening the loop would make the
bug more damaging. Verify all current callers after the fix.

---

## Exit criteria

WP-C is done when:

- `Tracks::getNumberOfChannelStrips()` returns the runtime channel count.
- `Tracks::adjust()` calls `createChannelTrackVector()` when
  `m_numMCUChannels` changes.
- `m_channelTracks` maps `0..numMCUChannels`.
- A mapping smoke test or debug check confirms `m_channelTracks.size()` equals
  `m_numMCUChannels + 1` after `adjust(N)`.
- `getMediaTrackForChannel(9)` can return a valid track when
  `numMCUChannels >= 9`.
- `findMediaTrackForChannel(channel > m_numMCUChannels)` returns `NULL` even
  when a persisted anchor is set to that channel.
- `getChannelForMediaTrack()` searches the dynamic range and uses comparison,
  not assignment.
- `Tracks` no longer calls `m_pMCU->GetOffset()`.
- active-anchor calculations ignore anchors outside `1..numMCUChannels`
  without deleting them.
- `moveTrackToLeftMostChannel()` counts only active anchors in its
  `numAnchors` calculation inside the found-track branch.
- N=1 behavior is unchanged in REAPER.
- A local N>1 mapping smoke test shows logical slots above 8 can be populated
  without `Tracks` returning `NULL` solely because the channel is above 8.
