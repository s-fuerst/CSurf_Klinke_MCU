# CSurf Extended(): Event-Driven FX Chain, Window, and Learn Updates (v2)

Status: implementation plan, v2 (corrected after critical review)
Supersedes: `csurf-ext-event-driven-impl-plan.md` (v1) — v1 is retained for
history but must NOT be implemented from; the dispatcher example and the
Parts A-C designs in v1 contain correctness/performance hazards (see
`csurf-ext-event-driven-impl-plan-review.md`).
Related: `csurf-ext-event-driven-impl-plan-review.md` (the review this v2
answers), `large-project-performance-audit.md` (findings #1 and #5).

This document is self-contained. It restates the relevant background, then
gives corrected designs for the dispatcher, the shared event collector, and
Parts A–C. Part D from the previous drafts has been removed completely:
`PlugMode::followChanges()` and its polling behavior remain unchanged. A
traceability table at the end maps every review finding to the
section that addresses it.

## 0. Background

REAPER delivers control-surface notifications through
`IReaperControlSurface::Extended(int call, void* parm1, void* parm2, void* parm3)`
(`reaper_plugin.h:1495`, return 0 if unsupported). `CSurf_MCU` derives from
`IReaperControlSurface` (`csurf_mcu.h:274`) but does **not** override
`Extended()` today, so every notification below is unused and every
corresponding state change is discovered by polling.

All `CSURF_EXT_*` IDs are defined in the pinned SDK
(`reaper-sdk/sdk/reaper_plugin.h:1498-1523`) and delivered by the installed
REAPER 7.74. The SDK comments are the authoritative payload contract; where
they are ambiguous, REAPER's own OSC surface (`reaper-sdk/reaper-plugins/
reaper_csurf/csurf_osc.cpp`) is the reference implementation.

### 0.1 Payload contract (verified against SDK + OSC reference)

The SDK encodes parameters in three different ways. They are **not**
interchangeable and must be read exactly as documented:

| ID | `parm1` | `parm2` | `parm3` |
|---|---|---|---|
| `CSURF_EXT_SETFXCHANGE` 0x10013 | `(MediaTrack*)track` | `(INT_PTR)flags` — **the encoded flag value itself, cast to pointer width** (`&1` = rec fx). NOT a pointer. | unused |
| `CSURF_EXT_SETFXOPEN` 0x10012 | `(MediaTrack*)track` | `(int*)fxidx` — real pointer, dereference OK | `0`/`!0` open state — **pointer truthiness** (`p3 != NULL`), NOT a dereferenceable int. OSC uses `bool en=!!parm3;` (`csurf_osc.cpp` SETFXOPEN_IMPL block). |
| `CSURF_EXT_SETFXPARAM` 0x10008 | `(MediaTrack*)track` | `(int*)(fxidx<<16 \| paramidx)` — real pointer | `(double*)normalized value` — real pointer |

The v1 dispatcher dereferenced `parm3` of `SETFXOPEN` (`*(int*)p3`), which
crashes when REAPER passes `(void*)1`. This v2 reads `p3` as truthiness.

### 0.2 Principles

1. **Notifications are additive triggers.** Older REAPER versions simply
   never fire newer IDs, so every existing polling path stays as a fallback.
   No version detection required.
2. **Notifications fire only on changes.** Caches that today are seeded by
   continuous polling (`PlugMoveWatcher` instance map, `m_knownWndStates`,
   `m_knownChainStates`) need explicit one-time seeding: on mode activation,
   on project switch, and after track removal. The `ProjectConfig`
   project-change signal is the shared hook — `PlugAccess` already
   subscribes to it (`PlugAccess.cpp:45-47`); the handler must be extended,
   not a second subscription added.
3. **`Extended()` is a collector, not a worker.** It runs on the REAPER
   main thread (same as `Run()`), but the pinned header does not state that
   guarantee explicitly, and immediate handling is reentrant: a `SETFXOPEN`
   handler that calls `TrackFX_Show()` can synchronously produce another
   `SETFXOPEN`. Therefore `Extended()` only validates the payload, copies
   the small values into pending sets, and returns. All real work happens in
   `Run()` at a defined point, after track-list reconciliation.
4. **Echo semantics.** REAPER may re-notify a parameter the surface itself
   wrote (surface fader → `TrackFX_SetParam` → `SETFXPARAM` echo).
   For a 1:1 replacement of today's poll this is equivalent behaviour, not a
   bug — but it must be verified per part.
5. **Duplicate delivery.** Multi-surface setups receive the notification
   once per surface. Per-surface window/Learn state is processed separately,
   while the global `PlugMoveWatcher` owns one shared dirty set so duplicate
   `SETFXCHANGE` notifications that arrive before reconciliation are
   coalesced. A late duplicate may cause an idempotent snapshot comparison,
   but it must not emit duplicate movement signals.

## 1. Dispatcher

One `Extended()` override in `CSurf_MCU`. It validates, normalizes, and
queues; it never calls feature logic directly.

```cpp
int CSurf_MCU::Extended(int call, void *p1, void *p2, void *p3) {
  switch (call) {
  case CSURF_EXT_SETFXCHANGE: {
    MediaTrack *tr = (MediaTrack *)p1;
    INT_PTR flags = (INT_PTR)p2;          // encoded value, NOT a pointer
    if (!tr)
      return 0;
    if (flags & 1)                         // rec-fx chain change
      return 1;                            //   acknowledged, not modelled
    m_evtCollector.onFxCacheChanged(tr);   // per-surface Part B invalidation
    if (PlugMoveWatcher::exists())
      PlugMoveWatcher::instance()->trackFXChainChanged(tr); // global Part A queue
    return 1;
  }
  case CSURF_EXT_SETFXOPEN: {
    MediaTrack *tr = (MediaTrack *)p1;
    if (!tr || !p2)
      return 0;
    int fxidx = *(int *)p2;                // p2 IS a real int*
    bool open = (p3 != NULL);              // p3 is pointer TRUTHINESS, never *(int*)p3
    m_evtCollector.onWindowStateChanged(tr, fxidx, open);
    return 1;
  }
  case CSURF_EXT_SETFXPARAM: {
    MediaTrack *tr = (MediaTrack *)p1;
    if (!tr || !p2 || !p3)
      return 0;
    uint32_t packed = (uint32_t)*(int *)p2;
    unsigned fxidx = (packed >> 16) & 0xFFFFu;
    unsigned paramidx = packed & 0xFFFFu;
    // p3 is normalized, but Parts A-C have no normalized-value consumer.
    // Its value is deliberately not queued; Part C re-reads the raw value.
    m_evtCollector.onParamChanged(tr, fxidx, paramidx);
    return 1;
  }
  }
  return 0; // unsupported
}
```

- `CSURF_EXT_SETFXPARAM_RECFX` (0x10018) is **not** handled (return 0): the
  codebase never addresses record-input FX. The rec-fx flag inside
  `SETFXCHANGE` is filtered explicitly above so a rec-chain edit never
  dirties the normal-chain watcher.
- `CSURF_EXT_RESET` is deliberately not handled by this plan. Returning 1
  would promise the SDK's complete surface-reset semantics, which is broader
  than re-seeding the A-C caches. Full reset support is separate work.

### 1.1 Packed parameter index decoding (Part C)

`parm2` packs `fxidx<<16 | paramidx`. Decode as 16:16, unsigned:

```cpp
uint32_t raw = (uint32_t)packed;
unsigned fxidx    = (raw >> 16) & 0xFFFFu;
unsigned paramidx = raw & 0xFFFFu;
```

Both decoded fields are only 16 bits. TrackFX addressing flags such as
`0x1000000` and `0x2000000` cannot be reliably transported by simply shifting
that flagged TrackFX index left by 16 in a 32-bit packed value. In particular,
they do **not** reappear as `fxidx == 0x100`. A guard such as
`fxidx >= 0x100` would merely reject otherwise valid top-level slots 256 and
above.

`CSURF_EXT_SETFXPARAM_RECFX` already separates record/input FX from normal FX.
For the normal event, defer validation until drain time and accept the decoded
slot only when `fxidx < (unsigned)TrackFX_GetCount(track)`. Container event
encoding remains unsupported and must not be inferred from the legacy 16:16
payload. If REAPER reports a value that does not validate as a normal top-level
slot, acknowledge and ignore it.

## 2. The deferred event collector

A single member of each `CSurf_MCU` (e.g. `EventCollector m_evtCollector`)
owns the per-surface pending structures. `Extended()` only inserts; `Run()`
drains. The movement watcher's dirty set is global because
`PlugMoveWatcher` itself is a singleton.

```text
EventCollector
  std::set<MediaTrack*>                     m_dirtyFxCaches;  // Part B cache invalidation
  std::map<std::pair<MediaTrack*,int>,bool> m_pendingWindows; // Part B, last write wins
  std::set<PendingParam>                    m_pendingParams;  // Part C

PendingParam
  MediaTrack *track
  unsigned fxidx
  unsigned paramidx

PlugMoveWatcher (singleton)
  std::set<MediaTrack*> m_dirtyFxChains;                     // Part A
```

- **Why queue, not call directly?** Pointer-backed payloads (`parm2`/`parm3`
  of `SETFXPARAM`) are only valid during the callback; their lifetime is not
  guaranteed afterwards. The collector dereferences and copies them inside
  `Extended()` (extracting both `fxidx` and `paramidx`) and stores only
  values with known lifetime. `MediaTrack*` is stable for the track's life,
  but stale-pointer protection still applies (see 2.2).
- `onParamChanged` stores `(track, fxidx, paramidx)`. Dropping `fxidx` would
  let a parameter event from another FX on the same track masquerade as an
  event for the watched plugin.
- The normalized value is not stored because Part C uses the event only as a
  trigger and re-reads the current raw value.
- `SETFXCHANGE` inserts into the singleton watcher's dirty set for Part A and
  into the local collector's `m_dirtyFxCaches` for Part B. Both operations
  only queue pointers; neither performs feature work in `Extended()`. The
  first set deduplicates global movement work; the second ensures every
  configured surface invalidates its own window cache.

### 2.1 Drain point

In `CSurf_MCU::Run()` (`csurf_mcu.cpp:1033`), the current ordering is:

1. `ProjectConfig::checkReaProjectChange()` (~:1042)
2. `Tracks::tracksStatesChanged()` → `m_pCCSManager->trackListChange()`
   (track removal/reconciliation, ~:1047)
3. `PlugMoveWatcher::instance()->checkMovement()` (:1051)
4. `signalFrame(now)` (:1055) — `PluginWatcher::frame()` is connected here
5. `m_pCCSManager->frameUpdate(now)` (:1218) — PlugMode per-frame

The singleton movement dirty set is already populated by `Extended()`, so
`PlugMoveWatcher::checkMovement()` remains step 3. Drain the local collector
**between steps 3 and 4**: after track-list reconciliation and after movement
signals have re-keyed location-based state, but before watcher/frame work.
Within the local drain, process chain-cache invalidations before window events
and parameter events. This preserves the stale-pointer invariant and prevents
Part C from observing a half-reconciled FX move.

### 2.2 Stale-pointer protection for queued `MediaTrack*`

`Tracks::tracksStatesChanged()` / `trackListChange()` already run before the
drain. The collector therefore validates every queued `MediaTrack*` against
the live track set before dispatching. Validation must explicitly accept
`GetMasterTrack(NULL)` in addition to normal tracks enumerated with
`CSurf_TrackFromID`; otherwise valid master-FX events would be discarded.
The track-removal path erases removed normal tracks from all local pending
structures and from `PlugMoveWatcher::m_dirtyFxChains`.

### 2.3 Idempotency / fan-out

Draining is idempotent: each local structure is cleared after dispatch, and
feature handlers are state-based. The singleton movement dirty set is drained
once per batch and emits `signalPlugMoveFinished()` exactly once after all net
movement signals for that batch. At the start of a drain, swap each pending
container into a local batch before invoking handlers. Events produced
reentrantly by `TrackFX_Show()` then land in the now-empty member container
and remain queued for the next frame instead of being erased with the current
batch.

## 3. Cross-cutting changes

- **Project switch re-seed (extend existing subscription).** `PlugAccess`
  already subscribes to `ProjectConfig::connect2ProjectChangeSignal` in its
  constructor (`PlugAccess.cpp:45-47`). Its `projectChanged()` handler
  currently only persists/restores slot state (`PlugAccess.cpp:739-765`).
  Extend `FREE`/`READ` to clear the window/chain caches and set a
  `needsReseed` flag. Do **not** enumerate FX from inside the project callback:
  `FREE` occurs at the beginning of project loading. The next `Run()`, after
  track-list reconciliation, performs the actual seed. Do **not** add a
  second subscription.
- **Movement snapshot on project switch.** `PlugMoveWatcher` adds its own
  `ProjectConfig` subscription. `FREE`/`READ` only mark its global snapshot
  for a forced rebuild; rebuilding happens from `checkMovement()` after track
  reconciliation, never from the project callback.
- **`GetProjectStateChangeCount(NULL)` import.** This API is **not**
  currently imported (`grep` finds no occurrence in `csurf_main.cpp`). The
  Part A fallback depends on it. Adding it requires a
  declaration and an `IMPAPI` entry in `src/csurf_main.cpp` — which is
  therefore added to the file-by-file table (v1 omitted it). Note: a
  continuously changing project counter can still cause a fallback scan
  during heavy editing; this is acceptable as a transitional fallback but
  must be measured (section "Measurements").
- **Fallback safety guarantee.** v1 claimed window open/close is an
  undoable project state change and used that as a safety guarantee. This is
  unproven. Each part now has an explicit fallback suited to its semantics:
  Part A uses the latched project counter plus forced rebuilds, Part B uses
  an independent 5–10 Hz timer, and Part C retains a shadow parameter poll
  during validation.

## Part A: FX chain changes — `CSURF_EXT_SETFXCHANGE`

### A.1 Design: dirty set + GUID-indexed snapshot diff

`PlugMoveWatcher` keeps its public signal API (`signalPlugMove`,
`signalPlugMoveFinished`, `connectPlugMoveSignal`,
`connectPlugMoveFinishedSignal`) but replaces the old per-slot movement
algorithm with a batch GUID snapshot diff.

- New: `trackFXChainChanged(MediaTrack*)` inserts the track into the
  singleton watcher's `m_dirtyFxChains` set (O(1)). Because the set is owned
  by the singleton, duplicate delivery to multiple `CSurf_MCU` instances is
  coalesced before movement reconciliation.
- `checkMovement()` (`PlugMoveWatcher.cpp:152`) processes only dirty tracks,
  then a gated fallback:
  - **forced full scan** (initial seed or project switch via
    `ProjectConfig` FREE/READ), or
  - **coalesced counter fallback**: `GetProjectStateChangeCount(NULL)`
    changed AND ≥ 250 ms since the last full scan (covers undo/redo and
    anything REAPER does not notify); otherwise nothing.

The counter fallback must latch work. Keep `lastScannedStateCount` unchanged
while the 250 ms rate limit prevents a scan, or set `fullScanPending=true`.
Only a completed scan advances the scanned counter. Otherwise a state change
can be recorded as observed without ever being reconciled.

The v1 design retained `PlugInstanceInfo::movedTo()`, which for each
displaced FX scans the master chain and then every track/FX
(`PlugMoveWatcher.cpp:25-43, 103-130`). That is O(project) **per displaced
FX**, so a single reorder displacing N FX causes N full-project scans.
`checkMovementForTrack()` is therefore **not** O(1); only the queue insert
is O(1). The audit recommended a one-pass GUID→location snapshot diff, and
this v2 adopts it:

- Maintain both directions explicitly:
  `GUID -> (MediaTrack*, slot)` and `(MediaTrack*, slot) -> GUID`.
- Snapshot **all dirty tracks first**, including the master when dirty, before
  emitting any signal. Diff the complete dirty batch against the old index so
  a move between two reported dirty tracks is resolved without a project-wide
  search.
- Emit deterministic net changes for every GUID whose location changed and
  deletions for GUIDs that disappeared. Additions with a new GUID only seed
  the snapshot; they are not moves.
- Only when a GUID vanished and its destination track was not reported dirty,
  use one project-wide lookup for that missing GUID. This is the compatibility
  path for incomplete cross-track notification delivery.
- Apply the new indexes atomically after the diff, then emit
  `signalPlugMoveFinished()` once for the entire batch. This preserves the
  transaction boundary used by `PlugMapManager::m_oldMaps`.

### A.2 Edge cases

- **Rec-fx flag** (`flags & 1`): filtered in the dispatcher (§1) so the
  watcher only ever sees normal-chain changes. (v1 forwarded rec-fx events
  to the normal-chain watcher.)
- **Undo/redo:** unknown whether REAPER re-fires `SETFXCHANGE`; the
  counter fallback covers it. Runtime test decides if it can be relaxed.
- **Cut/paste with FXID preservation (REAPER 7.x):** removal + re-addition
  with the same GUID; the GUID index keeps the identity, so re-keying by
  GUID yields the correct outcome. Validate.
- **`exists()` guard during teardown:** the singleton is deleted in
  `~CSurf_MCU()` (`csurf_mcu.cpp:1016`); `Extended()` must not lazily
  recreate it. The dispatcher checks `PlugMoveWatcher::exists()` before
  inserting into its shared dirty set.
- Containers, take FX, master track, bypass toggles: no behavioural change
  vs. today (the watcher models the normal top-level chain).

## Part B: FX window open/close — `CSURF_EXT_SETFXOPEN`

### B.1 What is polled today (per frame, in `PlugMode::frameUpdate()`)

- `PlugAccess::checkFloatWindows()` (`PlugAccess.cpp:928`): per selected
  track (FOLLOW=SAME_TRACK) or every track + master (FOLLOW=ALWAYS),
  `TrackFX_GetFloatingWindow` per FX, diffed against `m_knownWndStates`.
  Runs EVERY frame, no debounce.
- `PlugWindowManager::allowOnlySelectedFloat()` (`PlugWindowManager.cpp:83`):
  full-project scan closing every floating window except one; called from
  `checkFloatWindows()` whenever `PMO_LIMIT_FLOATING == ONLY_ONE_GLOBAL`.
- `PlugWindowManager::moveWnd()` (`PlugWindowManager.cpp:106`): full-project
  scan repositioning windows not in the known set (`PMO2_MOVE`; KLINKE build
  moves to -1280). Runs every frame while the option is on.
- `PlugAccess::syncKnownStates()` (`PlugAccess.cpp:980`): full seed of
  `m_knownWndStates` and `m_knownChainStates`.

Note: the feature only ever observes **floating** windows
(`TrackFX_GetFloatingWindow`); the chain window's embedded FX UI is the
domain of `checkChain()` (kept as a poll, see v1 Decisions log).

### B.2 Design

New `PlugAccess::windowStateChanged(MediaTrack* tr, int fxidx, bool open)` is
called from the collector drain after validating
`0 <= fxidx < TrackFX_GetCount(tr)`. Cache maintenance always runs;
user-visible policy is applied only while Plug Mode is active (see B.6):

1. **Float-vs-chain disambiguation.** The payload does not say whether the
   window is floating or embedded in the FX chain window. On an **open**
   event, one follow-up call decides:
   - `TrackFX_GetFloatingWindow(tr, fxidx) != NULL` → floating → proceed
     with follow/move/limit logic (B.4).
   - otherwise → retain a bounded one- or two-frame retry because REAPER may
     have notified before the HWND became available. If it is still NULL
     after the retry, treat it as a chain-window event, update the cache only,
     and take no floating-window action. Chain follow remains a poll.
   On a **close** event: set the cache entry to NULL (correct for both a
   float close and a chain selection change).
2. (B.3) Cache maintenance: `m_knownWndStates[(tr, fxidx)] =
   TrackFX_GetFloatingWindow(tr, fxidx)` on open, NULL on close.
3. (B.4) If floating and open: reproduce the existing follow decision, then
   apply move/limit — see B.4/B.5.
4. Remove the per-frame `checkFloatWindows()` call after validation. Keep a
   real polling method for the independent fallback timer; it must not become
   a no-op while it is still the safety path.

### B.3 Slot-keyed cache invalidation on chain edits (corrects review #5)

`m_knownWndStates` is `std::map<boost::tuple<MediaTrack*,int>, HWND>`
keyed by `(track, slot)` (`PlugAccess.h:383`). Reorder/insert/delete FX
**change slot meaning** even when no window opens or closes. v1 routed
`SETFXCHANGE` only to `PlugMoveWatcher`, so Part B could keep HWND state for
the wrong FX after a chain edit. v2 fans the chain-dirty signal out:

- On drain, for each dirty FX-chain track, **clear all
  `m_knownWndStates` / `m_knownChainStates` entries for that track**, then
  re-seed them (`TrackFX_GetFloatingWindow` per remaining slot; chain state
  via `TrackFX_GetChainVisible`). One central FX-chain dirty notification
  fans out to every slot-derived cache.
- `PlugAccess`, `PluginWatcher`, and the active plug map are also slot-based.
  To preserve current selection semantics, the selected location remains
  `(track, slot)` rather than following an FX GUID. If the selected track is
  dirty, explicitly rebind that slot after movement reconciliation: reload
  the map and reset the `PluginWatcher` baseline for whatever FX now occupies
  the slot, or deselect if the slot no longer exists. This prevents Part C
  from remaining attached to stale slot contents when adjacent FX have the
  same displayed name.

### B.4 Follow / LIMIT_FLOATING semantics (corrects review #9)

Current code, on a newly-appearing float under `PMOA_ONLY_ONE_GLOBAL`,
preserves the **currently selected** FX:
`allowOnlySelectedFloat(m_pPlugTrack, m_iSlot)` (`PlugAccess.cpp:975-976`).
v1 proposed preserving the **newly opened event FX**, which is only
equivalent if the follow logic selected that FX first. With follow
disabled, v1 changed behaviour. v2 reproduces the existing decision order:

1. Reproduce the follow feature exactly as `checkFloatWindows()` does today
   (call `accessPlugin(tr, fxidx, true)` only when the follow options apply
   and the window is not already the selected plug).
2. **Then** call `allowOnlySelectedFloat(m_pPlugTrack, m_iSlot)` — i.e.
   preserve whatever FX is selected after step 1, matching today.
3. `LIMIT_FLOATING == ONLY_CHAIN`: close the float, open the chain (as
   today, for this one window).

### B.5 `PMO2_MOVE` per-window: the `moveWnd(HWND)` overload must be implemented (corrects review #11)

`PlugWindowManager.h:47` declares `void moveWnd(HWND hwnd);` but
`PlugWindowManager.cpp` defines only the no-argument full-scan
`moveWnd()` (`:106`). The per-window overload is **new code**, not an
existing helper. Part B therefore adds the `moveWnd(HWND)` definition (move
the single known HWND) and the per-frame `moveWnd()` scan is deleted. This
is listed explicitly in the file-by-file table as new code.

### B.6 Inactive-mode awareness (corrects review #9)

`PlugMode` is constructed once and lives for the whole session
(`CCSManager.cpp:34`), but today's window scans run only from
`PlugMode::frameUpdate()`, i.e. only while Plug Mode is active. Applying
follow/move/limit actions merely because `getPlugMode()` is non-null would
introduce new behaviour in other modes. v2 therefore splits handling
explicitly:

- Always: validate the event and maintain/invalidate the cache.
- Only while Plug Mode is active: apply follow, move, and limit side effects.

No user-visible window action is taken while another mode is active.
`syncKnownStates()` still runs on activation as an authoritative baseline.

### B.7 Seeding corrections (corrects review #10)

- `syncKnownStates()` is called from `PlugMode::activate()`
  (`PlugMode.cpp:136`), **not** "once at PlugMode construction" as v1
  claimed. It stays as the activation seed.
- `syncKnownStates()` seeds master **chain** state but not master
  **floating-window** state (the `TrackIterator` loop covers only normal
  tracks; `PlugAccess.cpp:980-1004`). Extend it to seed master floating
  windows too, so the continuous scan can disappear safely.
- `PlugMoveWatcher` needs an explicit forced initial seed, otherwise the GUID
  index is empty until the first project-switch notification. Its constructor
  sets a `forceFullScan` flag; the scan itself runs from the first
  `checkMovement()`, not inside construction.

### B.8 Open questions / runtime validation

- Does `SETFXOPEN` fire for floating windows of all types (VST/CLAP/AU/JS),
  for master-track FX, and for both open AND close?
- Does it ALSO fire for chain-window selection changes? B.2.1 disambiguates.
- **Timing:** at notification time, is the floating HWND already valid? The
  bounded retry in B.2 handles a late-created HWND. Do not infer floating
  state from `TrackFX_GetChainVisible(tr) != fxidx`; another chain slot can
  be visible while a float is opening.
- **Recursion:** a `SETFXOPEN` handler calling `TrackFX_Show()` can
  synchronously produce another `SETFXOPEN`. The deferred collector (§2)
  collapses the re-entry into the pending map (last write wins), preventing
  unbounded recursion.
- Until validated: run the existing scan from an **independent 5–10 Hz
  timer** (not the project counter). The timer belongs to `PlugAccess` and
  invokes a renamed `pollFloatWindowsFallback()` from `frameUpdate()` only
  when its deadline expires. Event and poll observations are instrumented
  separately; only one path applies each side effect.

## Part C: Selected-plugin parameter watch (Learn) — `CSURF_EXT_SETFXPARAM`

### C.1 What is polled today

`PluginWatcher::frame()` (`PluginWatcher.cpp:26`, connected to `signalFrame`)
reads the selected plugin's **raw** parameter values via
`TrackFX_GetParam(m_pMediaTrack, m_iSlot, iParam, &minVal, &maxVal)`
(`:49`) and formats them via `TrackFX_FormatParamValue`
(`getParamString`, `:67`). It emits `signalParamChanged` on change.

Consumer (Learn): `PlugModeComponent`'s param-change connection feeds
`PlugModeVPOTComponent::changeParamId(paramId, value, paramName)`
(`PlugModeVPOTComponent.cpp:144`), which stores the **value as a step-map
key**: `(*pSteps)[value] = ...` (`:146`).

### C.2 The normalized-vs-raw hazard (corrects review #2)

`CSURF_EXT_SETFXPARAM` carries a **normalized** value (`parm3=(double*)`,
`reaper_plugin.h:1506`). The existing pipeline is raw end-to-end:

- read: `TrackFX_GetParam` → raw.
- format: `TrackFX_FormatParamValue` expects raw.
- Learn key: the value becomes a persisted map key.

Feeding the event's normalized value directly into `signalParamChanged`
(v1 Part C.4) therefore (a) corrupts persisted mappings for any parameter
whose raw range is not `[0,1]`, and (b) pairs the wrong API
(`TrackFX_FormatParamValue` on a normalized value). v2 uses the event
**only as a trigger**:

- On drain, require the complete event key `(track, fxidx, paramidx)` to
  match the watched `(track, fxidx)`. Then read the current **raw** value
  once with `TrackFX_GetParam` and emit `signalParamChanged` with that raw
  value and the existing `getParamString()`. Still O(1) per event; Learn
  semantics preserved.
- v1's note about migrating the whole consumer contract to normalized
  values is rejected as out of scope (a separate data-format migration that
  must not be hidden in this optimization).

### C.3 Parameter exclusion range

The current poll excludes the final two synthetic parameters by iterating
to `numParams - 2` (`PluginWatcher.cpp:47-50`). Preserve that exact range
without signed/unsigned underflow:

```cpp
int numParams = TrackFX_GetNumParams(track, fxidx);
if (numParams <= 2 || paramidx >= (unsigned)(numParams - 2))
  return;
```

This is intentionally a compatibility rule, not a claim that every plugin
will always expose exactly two synthetic parameters at the end.

### C.4 Watch activation and queued-event baseline

The current watcher seeds its value cache without emitting on the first poll
after Learn is enabled. A parameter event that occurred before Learn was
enabled must therefore not be delivered merely because it was still queued
when the connection was created. Since Follow Changes is out of scope and
Learn is the only consumer of `m_pendingParams`,
`PluginWatcher::connect2ParamChanged()` clears pending parameter events for
its watched `(track, fxidx)` before activating the connection. Events arriving
after activation are captured.

### C.5 `signalNameChange` = plugin name, not parameter name

The name signal carries the **plugin's** name (fed by `TrackFX_GetFXName`).
There is no REAPER notification for FX renames, so the name poll stays: one
`TrackFX_GetFXName` per frame, only while name connections exist. The param
loop is deleted; `frame()` shrinks to the name check (fallback variant kept
behind a compile/runtime switch until validated).

### C.6 Validation fallback and open questions

During validation, retain the old parameter poll as a shadow observer: it
records mismatches between polled and event-observed parameter IDs but does
not emit a second Learn signal. Once event coverage is established for the
supported plugin types, remove the parameter loop. If a correctness fallback
must remain, run it at a measured lower rate and let only one path emit.

- Does `SETFXPARAM` fire for plugin-internal changes (LFOs, in-plugin VUs)?
  If not, Learn misses those — acceptable, documented; the poll fallback can
  stay for that case.
- Echo of the surface's own writes: identical to today's poll result; verify.

## 4. File-by-file changes (corrected)

| File | Change |
|---|---|
| `src/core/csurf_mcu.h/.cpp` | `Extended()` override + dispatcher (`<cstdint>` for `uint32_t`); new per-surface `EventCollector` member (§2); local drain after `checkMovement()` and before `signalFrame()` in `Run()`. |
| `src/csurf_main.cpp` | **New (v1 omitted):** import `GetProjectStateChangeCount` (declaration + `IMPAPI`). |
| `src/modes/plugin/PlugMoveWatcher.h/.cpp` | Singleton-owned dirty set; batch GUID-indexed snapshot diff replacing per-displaced-FX `movedTo()` scans; latched coalesced fallback; project-switch forced rebuild flag/subscription; `exists()` guard; forced initial seed; dirty cleanup in `trackRemoved()`. (Part A) |
| `src/modes/plugin/PlugAccess.h/.cpp` | `windowStateChanged()`, bounded HWND retry, cache/policy split, `paramValueChanged()`; slot-keyed cache invalidation and selected-slot rebind on chain edits; extend the existing `ProjectConfig` subscription with deferred re-seeding; master floating-window seed; raw-value read and complete event-key validation. (Parts B, C) |
| `src/modes/plugin/PlugWindowManager.h/.cpp` | **Implement `moveWnd(HWND)`** (currently declared, not defined — B.5); `allowOnlySelectedFloat()` called event-triggered only. (Part B) |
| `src/modes/plugin/PluginWatcher.h/.cpp` | Param loop replaced by collector feed (raw re-read); clear pre-Learn queued events when parameter watching starts; name poll stays; safe `numParams-2` exclusion; shadow-poll instrumentation during validation. (Part C) |

Style: no `override` keyword in the existing class, 2-space indent, `m_` /
`p`-arg conventions (matching surrounding files).

## 5. Validation matrix

Per part, after build + deploy + full REAPER restart (AGENTS.md flow):

- **A:** reorder / cross-track move / delete / add / copy / cut-paste (with
  FXID preservation); undo/redo; project switch A→B→A; master chain; move
  while Plug Mode inactive; **multiple FX displaced by one reorder** (GUID
  snapshot must not scan per-FX).
- **B:** open/close float of VST/CLAP/AU/JS on normal + master tracks; all
  FOLLOW and `LIMIT_FLOATING` combinations; `PMO2_MOVE` on/off (incl.
  KLINKE -1280); chain-window selection change must NOT trigger float logic;
  notification timing vs. HWND validity; project switch with windows open;
  **FX insertion/reorder while floats are already open** (B.3
  invalidation); **window ops while Plug Mode inactive** (B.6); **follow
  disabled + new float under `ONLY_ONE_GLOBAL`** (B.4 preserves selected).
- **C:** Learn: tweak param in plugin UI → capture; rename FX → name change
  still detected; plugin-internal parameter movement; parameter changes from
  another FX on the same track ignored; pre-Learn queued changes ignored;
  rec-fx and invalid/non-top-level payloads never acted on.
- **Regression:** Plug Mode idle on a 400-track project — no A/B/C full-FX
  scan on every frame. `followChanges()` is explicitly out of scope and retains
  its current every-tenth-frame poll when that option is active.

## 6. Required tests before polling removal

In addition to the matrix above (adds the review's "Required tests"):

- `SETFXOPEN` payload truthiness **without** dereferencing `parm3`
  (unit/compile-time contract test: `Extended(SETFXOPEN, tr, &one, (void*)1)`
  must read `open==true` and never dereference).
- Event recursion from `TrackFX_Show()` inside window-policy handling (§2.3
  pending-map last-write-wins).
- Raw-vs-normalized Learn values on a parameter with a non-`[0,1]` range
  (C.2).
- Parameter events from two different FX on the same track; only the exact
  watched `(track, fxidx)` may reach Learn.
- A synthetic packed value whose decoded top-level FX slot is 256 or above;
  it must be validated against `TrackFX_GetCount`, not rejected by a made-up
  addressing-flag guard.
- A parameter event queued before Learn is enabled; it must not be emitted
  after the connection is activated.
- FX insertion/reorder while floating windows are already open (B.3).
- Window operations while Plug Mode is inactive (B.6).
- Same-value parameter notifications and surface-write echoes.
- Missing source OR destination notification during a cross-track move (A.1
  project-wide missing-GUID compatibility lookup).
- Initial plugin state before the first project-switch notification (A.2
  ctor seed).
- `GetProjectStateChangeCount` import compiles and links (csurf_main.cpp).

## 7. Measurements

Temporary instrumentation (`MCU_DEBUG_LOG` counters):
- `TrackFX_GetFloatingWindow` / `TrackFX_GetParam` / `TrackFX_GetFXGUID`
  calls per second before vs. after. Attribute calls to A, B, C, and the
  unchanged `followChanges()` path separately; do not count its retained
  polling as a regression in A-C.
- Time in `frameUpdate()`, `checkMovement()`, window handling, and the
  `PluginWatcher` parameter path before vs. after.
- Event counts per editing action (validates notification reliability).
- Fallback-timer fire frequency (validates that the 5–10 Hz fallback is
  rarely the source of truth once events are proven).

## 8. Implementation order

Each step is an independent build + deploy + test cycle:

1. **Dispatcher + collector + `GetProjectStateChangeCount` import + payload
   contract tests**, with NO polls removed yet (§1, §2, §3).
2. **Part A** with the GUID snapshot diff; keep the coalesced fallback and
   measure its real frequency.
3. **Part B** behind the existing poll; compare event vs. poll observations;
   use the independent 5–10 Hz fallback until coverage is proven; implement
   `moveWnd(HWND)`.
4. **Part C** using event-triggered raw-value reads; preserve the
   `numParams-2` exclusion and complete `(track, fxidx, paramidx)` identity.
5. Remove each old A-C polling path **separately** after its event path passes
   the full validation matrix + the required tests.
6. Leave `PlugMode::followChanges()` and all of its current cache/polling code
   untouched.

## 9. Traceability — review findings → v2 sections

| Review finding | Disposition | Where addressed in v2 |
|---|---|---|
| #1 `SETFXOPEN`/`SETFXCHANGE` payload deref | Fixed (blocking) | §0.1, §1 |
| #2 Part C normalized vs raw (Learn key + format) | Fixed (blocking) | §0.1, Part C.2, C.3 |
| #3 Follow Changes semantics | Removed from scope | Current `followChanges()` remains unchanged |
| #4 Direct calls from `Extended()` reentrant | Adopted (deferred collector) | §0.2(3), §2 |
| #5 `SETFXCHANGE` not invalidating slot-keyed window cache | Fixed | Part B.3 |
| #6 Part A retains per-displaced-FX nested search | Fixed (GUID snapshot diff) | Part A.1 |
| #7 `GetProjectStateChangeCount` not imported; `csurf_main.cpp` missing | Fixed | §3, §4, step 1 |
| #8 Packed-index dead checks (`>= 0x1000000`) | Fixed | §1.1 |
| #9 `ONLY_ONE_GLOBAL` preserves selected, not newly-opened; inactive-mode | Fixed | Part B.4, B.6 |
| #10 Cache-seeding claims vs code (subscription exists; `syncKnownStates` on activate; master float not seeded) | Fixed | §0.2(2), §3, Part B.7 |
| #11 `moveWnd(HWND)` declared but not defined | Fixed (new code) | Part B.5, §4 |
| #12 Reverse-index ownership + invalidation coverage | Removed from scope | No reverse index is introduced; current `followChanges()` remains unchanged |

## 10. Out of scope

- Audit items #2/#3 (Tracks pointer index, dirty-state gating), #6 (solo
  cache), #7 (display buffer), #8 (editor refresh), #10 (surface value
  caches) — separate work.
- Chain-window selection follow — stays a poll (v1 Decisions log:
  `SETFOCUSEDFX` also fires on float focus; "focused FX" ≠ chain-window
  slot; 2 s debounce makes the poll cheap).
- Preset changes, param-info changes — rejected (v1 Decisions log).
- Send/Receive event updates — deferred; audit #4 carries the option.
- Follow Changes optimization (audit #9) — removed from this plan completely.
  `PlugMode::followChanges()`, `refillParamCache()`, its every-tenth-frame
  cadence, and all existing invalidation behavior remain unchanged.
- Full `CSURF_EXT_RESET` support. This requires complete surface reset and
  output-cache republishing semantics, not only A-C cache invalidation.
- Rec-FX handling (`SETFXPARAM_RECFX`, rec-fx flag in `SETFXCHANGE`).
- Container (FX-in-FX) addressing.
