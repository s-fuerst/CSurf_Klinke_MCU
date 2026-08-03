# Large-Project Performance Audit

Date: 2026-08-03

## Scope

This is a static audit of code paths that run once per control-surface frame or
scale with the number of tracks, FX instances, sends, receives, or configured
surface channels. The primary target is REAPER projects containing several
hundred tracks.

No performance changes described below have been implemented as part of this
audit. Release debug logging was disabled separately at the build-system level.
The findings should be confirmed with timing instrumentation before and after
each optimization.

## Executive summary

The three highest-priority issues are:

1. `PlugMoveWatcher` scans every track and every FX instance on every surface
   frame, even when the project has not changed.
2. `Tracks::adjust()` performs full-project state work every frame, while
   `getTrackStateForMediaTrack()` uses a linear lookup that turns several
   otherwise bounded operations into project-size-dependent work.
3. Send and receive modes enumerate the selected track's complete routing list
   up to seven times during one frame update.

The recommended implementation order is documented at the end of this file.

## 1. Full-project FX movement scan on every frame

### Evidence

`CSurf_MCU::Run()` calls `PlugMoveWatcher::checkMovement()` on every surface
frame. `checkMovement()` iterates over every normal track and the master track,
and `checkMovementForTrack()` inspects every FX slot on each track.

Relevant locations:

- `src/core/csurf_mcu.cpp`, `CSurf_MCU::Run()`
- `src/modes/plugin/PlugMoveWatcher.cpp`,
  `PlugMoveWatcher::checkMovement()` and
  `PlugMoveWatcher::checkMovementForTrack()`

When an FX instance no longer matches its stored position,
`PlugInstanceInfo::movedTo()` performs another scan of every FX in the project.
Multiple changed instances can therefore cause repeated full-project searches.

For a project with 400 tracks and an average of 10 FX per track, the stable
path performs approximately 4,000 FX GUID checks per surface frame. At a
30 Hz surface refresh rate, this is approximately 120,000 checks per second
before any FX movement is detected.

### Recommendation

Maintain a snapshot indexed by FX GUID:

```text
FX GUID -> { MediaTrack*, slot }
```

Rebuild the snapshot only when the project may have changed. A practical first
gate is `GetProjectStateChangeCount()`, with changes coalesced so that the scan
runs at most once every 100-250 ms. Compare the previous and current snapshots
in one pass and emit move, removal, and addition notifications from that diff.

This removes the per-frame stable scan and the nested `movedTo()` search.
Because some UI-only FX state may not affect the project state counter, retain
a slower fallback scan until runtime testing confirms the exact REAPER
behaviour.

### Validation

Test FX reorder, cross-track moves, copies, deletions, undo/redo, master-track
FX, project switches, and saved plug-map/favourite references.

## 2. Full-project track work in the stable frame path

### Evidence

`CSurf_MCU::Run()` calls both `Tracks::tracksStatesChanged()` and
`Tracks::adjust()` on every frame.

`tracksStatesChanged()` has an ordered-vector early exit and avoids rebuilding
the graph when the track list is unchanged. It still enumerates all tracks once
per frame. This was introduced deliberately to prevent stale `MediaTrack*`
dereferences after track removal, so it should not be removed without an
equivalent safety mechanism.

The larger remaining cost is `Tracks::adjust()`, which unconditionally calls
`updateTrackStates()`. With the default non-managed TCP and MCP options,
`updateTrackStates()` reads TCP visibility, TCP height, and MCP visibility for
every track on every frame. It then resolves the TrackState for every surface
channel.

Relevant locations:

- `src/core/csurf_mcu.cpp`, `CSurf_MCU::Run()`
- `src/core/Tracks.cpp`, `Tracks::tracksStatesChanged()`
- `src/core/Tracks.cpp`, `Tracks::adjust()` and
  `Tracks::updateTrackStates()`

### Recommendation

Introduce explicit dirty state for track mapping and TCP/MCP synchronization.
The dirty state should be set by:

- track additions, removals, and reorder operations;
- bank-offset, base-track, filter, folder-mode, and anchor changes;
- surface channel-count changes;
- TCP/MCP adjustment-option changes;
- selection changes when TCP visibility follows selection.

Only recompute channel mapping and managed visibility when the corresponding
state is dirty. Capture unmanaged TCP/MCP state when entering a managed mode,
rather than reading it continuously on every frame.

`SetTrackListChange()` can become a cheap dirty notification, but the existing
per-frame safety scan should initially remain in place. After profiling and
runtime validation, it may be replaced by the notification plus a slower
integrity scan or targeted pointer validation.

### Validation

Test track addition, deletion, reorder, undo/redo, project switching, banking,
folder navigation, anchors, all track filters, TCP/MCP visibility options, and
selection-follow behaviour.

## 3. Linear TrackState lookup and quadratic graph rebuilds

### Evidence

`Tracks::getTrackStateForMediaTrack()` converts the track GUID to a string and
then linearly scans `m_trackStates`, even though `m_trackStates` is already
keyed by GUID string. If that scan fails, it performs a second linear scan by
pointer.

The method has many hot-path callers, including meter updates, display updates,
channel-state updates, track-graph construction, and the track-state editor.
During `TSGraph::buildGraph()`, it is called repeatedly for each track. Because
the lookup itself is O(track count), graph reconstruction becomes O(n^2).

Relevant locations:

- `src/core/Tracks.cpp`, `Tracks::getTrackStateForMediaTrack()`
- `src/core/Tracks.cpp`, `TSGraph::buildGraph()`
- `src/hardware/MeterBridge.cpp`, `MeterBridge::updateMeter()`
- `src/modes/multitrack/MultiTrackMode.cpp`,
  `MultiTrackMode::updateDisplay()`

An earlier porting analysis in `ai-docs/pr11-perf-port-analysis.md` already
recommended a pointer index named `m_tracksByPointer`. The current source does
not contain that index, although one source comment refers to it.

### Recommendation

Add a secondary index:

```cpp
std::unordered_map<MediaTrack *, TrackState *> m_tracksByPointer;
```

Use the pointer index as the normal O(1) path. If the pointer is absent, use
`m_trackStates.find(guid)` directly as the project-restore fallback and update
the TrackState pointer plus the pointer index. Do not linearly scan the GUID
map.

Keep both indexes synchronized in all creation, project-load, replacement,
track-removal, and teardown paths. This change should be implemented before
more aggressive frame-loop gating because it is local and benefits many call
sites.

### Validation

Test project load and switch, undo/redo, deleted selected folder tracks, track
duplication, and any operation where REAPER may replace a `MediaTrack*` while
preserving its GUID.

## 4. Repeated full routing enumeration in Send and Receive modes

### Evidence

`SendReceiveModeBase::frameUpdate()` updates faders, VPOTs, record LEDs, solo
LEDs, mute LEDs, the display, and the meter bridge separately.

The individual update functions call `getSendInfos()` for different properties.
`updateDisplay()` performs two enumerations, and the meter bridge performs
another one. With a selected track, this can produce seven complete send or
receive enumerations in one surface frame.

`SendMode::getSendInfos()` and `ReceiveMode::getSendInfos()` enumerate from
index zero until `GetSetTrackSendInfo()` returns null. Consequently, the cost
depends on the selected track's total routing count, not just the currently
visible surface window.

Relevant locations:

- `src/modes/sends/SendReceiveModeBase.cpp`,
  `SendReceiveModeBase::frameUpdate()`
- `src/modes/sends/SendMode.cpp`, `SendMode::getSendInfos()`
- `src/modes/sends/ReceiveMode.cpp`, `ReceiveMode::getSendInfos()`
- `src/modes/sends/SendReceiveMeterBridge.cpp`,
  `SendReceiveMeterBridge::updateMeterBridge()`

### Recommendation

Build one typed frame snapshot containing only the visible routing window:

```text
RoutingEntry {
  MediaTrack* peer;
  double volume;
  double pan;
  bool mute;
  bool mono;
  int automationMode;
}
```

Read the total count once with `GetTrackNumSends()`, then query only
`m_startWithSend` through `m_startWithSend + availableChannels()`. Reuse the
snapshot for faders, VPOTs, LEDs, display, and meters. Avoid storing raw
property pointers beyond the frame.

### Validation

Test sends, receives, hardware outputs, routing banks, flipped controls,
automation modes, deleted routes, and tracks containing hundreds of routes.

## 5. Project-wide FX window polling in Plug Mode

### Evidence

`PlugAccess::checkFloatWindows()` can iterate through every track and every FX
on each Plug Mode frame. The broad scans are enabled by the `always`,
`only chain`, and `only selected` option combinations.

`checkChainChanges()` can additionally scan every track, and
`PlugWindowManager::allowOnlySelectedFloat()` performs another full FX scan.
The latter may therefore repeat work already performed earlier in the same
frame.

Relevant locations:

- `src/modes/plugin/PlugAccess.cpp`, `PlugAccess::checkFloatWindows()`
- `src/modes/plugin/PlugAccess.cpp`, `PlugAccess::checkChainChanges()`
- `src/modes/plugin/PlugWindowManager.cpp`,
  `PlugWindowManager::allowOnlySelectedFloat()` and
  `PlugWindowManager::moveWnd()`

### Recommendation

- Poll window state at 5-10 Hz instead of at the full surface refresh rate.
- Spread broad scans over multiple frames using a persistent track cursor.
- Reuse one FX/window snapshot across follow, limit, and move-window features.
- Run the close-other-windows pass only after a newly opened window is detected.
- Prefer focused/touched FX information where it covers the required behaviour,
  while retaining polling for cases REAPER does not report directly.

### Validation

Exercise every Plug Mode follow and floating-window option combination on all
platforms. Window handles and visibility behaviour differ between native
Win32, Cocoa, and SWELL/X11.

## 6. Global solo detection scans all tracks per frame

### Evidence

`CSurf_MCU::UpdateGlobalSoloLED()` is called every frame and calls
`SomethingSoloed()`. `SomethingSoloed()` walks all normal tracks until it finds
a soloed track. The worst case is the common case where no track is soloed.

Relevant locations:

- `src/core/csurf_mcu.cpp`, `CSurf_MCU::SomethingSoloed()`
- `src/core/csurf_mcu.cpp`, `CSurf_MCU::UpdateGlobalSoloLED()`

The REAPER control-surface API documents that `SetSurfaceSolo()` receives the
master track as the aggregate "any solo" notification.

### Recommendation

Cache the aggregate solo state from the master-track `SetSurfaceSolo()`
callback and update the LED only when the cached state changes. As a smaller
intermediate improvement, use REAPER's `AnyTrackSolo()` API rather than a local
track loop.

Preserve the separate master-muted/blinking behaviour.

## 7. Per-update heap allocation in the display buffer

### Evidence

`Display::changeText()` allocates and frees a temporary character buffer for
every field or line update. Mode frame updates call this method repeatedly even
when the rendered text does not change. Hardware output is diffed later, but
the allocation and buffer writes have already happened.

Relevant locations:

- `src/hardware/display/Display.cpp`, `Display::changeText()`
- `src/hardware/display/DisplayHandler.cpp`,
  `DisplayHandler::sendDifferences()`

### Recommendation

Use a fixed stack buffer sized for the maximum 56-character display row, or
write padding directly into the destination buffer. Compare the affected range
before copying and mark only changed rows dirty. `resendAllRows()` can then skip
clean rows without scanning all row contents.

This does not scale with project track count, but it is a frequent hot-path
operation and becomes more visible with multiple surface units.

## 8. Track-state editor performs project-wide work every frame

### Evidence

While the track-state editor is open,
`TrackStatesTableComponent::frame()` walks every track on every frame. Each
iteration currently invokes the linear `getTrackStateForMediaTrack()` lookup,
making the editor refresh O(n^2).

Relevant location:

- `src/modes/multitrack/editor/TrackStatesTableComponent.cpp`,
  `TrackStatesTableComponent::frame()`

### Recommendation

The TrackState pointer index reduces this to O(n). After that, add explicit
change notifications or a lower refresh rate so only changed rows are
repainted. Avoid the current double `flipRowSelection()` repaint workaround if
JUCE offers a targeted row repaint.

## 9. Follow Changes scans the complete mapping grid

### Evidence

When Plug Mode Follow Changes is enabled, `PlugMode::followChanges()` checks all
8 banks, 8 pages, and 8 channels for both fader and VPOT values. This is 1,024
value reads every tenth surface frame, including unused or duplicate parameter
mappings. Cache initialization performs the same work.

Relevant location:

- `src/modes/plugin/PlugMode.cpp`, `PlugMode::followChanges()` and
  `PlugMode::refillParamCache()`

### Recommendation

Build a unique list of actually mapped parameter IDs when a plug map is loaded.
Poll only that list. Where suitable, use REAPER's last-touched or focused-FX
information as a fast path and retain polling as a compatibility fallback.

## 10. Unbounded surface value caches

### Evidence

`CSurf_MCU::SetSurfaceVolume()` and `SetSurfacePan()` add entries keyed by
`MediaTrack*` to `m_surface_volume` and `m_surface_pan`. Track removal does not
erase these entries. Long editing sessions that repeatedly create and delete
tracks can therefore grow both maps and retain stale pointer keys.

Relevant locations:

- `src/core/csurf_mcu.h`, `m_surface_volume` and `m_surface_pan`
- `src/core/csurf_mcu.cpp`, `SetSurfaceVolume()` and `SetSurfacePan()`

### Recommendation

Erase both entries from the existing track-removal notification path and clear
them on project replacement. This is primarily a memory and stale-cache
correctness improvement; the map lookup cost grows only logarithmically.

## Recommended implementation order

1. Add the O(1) TrackState pointer index and direct GUID-map fallback.
2. Add dirty-state gating for `Tracks::adjust()` and channel mapping.
3. Replace `PlugMoveWatcher` with a state-gated, single-pass GUID snapshot.
4. Introduce one visible-window Send/Receive snapshot per frame.
5. Cache aggregate solo state from control-surface callbacks.
6. Rate-limit and consolidate Plug Mode FX-window polling.
7. Remove display hot-path allocations and add row dirty flags.
8. Make editor refresh and Follow Changes operate on explicit changed or used
   elements.
9. Clean surface value caches on track removal and project replacement.

Each step should be built, deployed, and tested independently. Do not combine
the track-lifetime safety changes with unrelated optimizations in one patch.

## Suggested measurements

Collect frame timings and call counts for at least these scenarios:

- 400 tracks, no FX, no solo, MultiTrack Mode idle;
- 400 tracks with approximately 4,000 total FX, idle and during playback;
- a selected routing hub with 200 or more sends and receives;
- 64 configured surface channels;
- Plug Mode with each follow/window-limiting option combination;
- track-state editor open on a 400-track project;
- track add, delete, reorder, project switch, and undo/redo bursts.

Useful counters include total frame time and individual time spent in
`tracksStatesChanged()`, `Tracks::adjust()`, `PlugMoveWatcher::checkMovement()`,
mode `frameUpdate()`, display diffing, and MIDI output. Report average, maximum,
and high-percentile values; average time alone can hide the graph-rebuild and
FX-move spikes that are most likely to cause visible stalls.
