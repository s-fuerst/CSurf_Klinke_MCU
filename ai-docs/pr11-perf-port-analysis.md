# Porting analysis — Bitbucket PR #11 (Performance + Crashfixes)

Source PR: https://bitbucket.org/Klinkenstecker/csurf_klinke_mcu/pull-requests/11
Author: Alex Hayes (`alexhayes`), branch `feature/perf-only-build-graph-when-necessary`
PR target: `master` @ `ceaa4a5` (v0.9.1.3) — the OLD flat layout (sources in repo root).
Local target: `cross-platform` (restructured `src/` tree + extender support + JUCE 8).

The PR was fetched as git remote `alexhayes`; the PR head is `c805f91d59`.
Compare base used below: `ceaa4a5432dde` (= merge-base of PR branch and master).

## Scope decision

- Port **only** topic (A): performance + crashfixes (the actual PR subject).
- **Skip** topic (B): the "Open FX Favorite" feature (CC 0x72-0x79). It is
  **already integrated** in `cross-platform` via commit `841627d`
  ("feat: integrate 8 FX Favorites + syncKnownStates bugfix from origin/master").
  Verified: `OpenFXFavorite`, `getPlugMode()`, `accessFXFavorite()`,
  `syncKnownStates()`, ButtonManager handler `{0x72,0x79,...}` all present.
- **Drop** the `QueryPerformanceCounter` instrumentation (`CSURF_PERF_LOG`,
  `PERFORMANCE_ANALYSIS.md`, `measurements/*.log`). It is disabled in the PR
  (`#define CSURF_PERF_LOG 0`) and the project already has `MCU_LOG`.
- **Drop** the version bump to 0.9.1.4 — versioning is via `VERSION.txt`,
  not a hard-coded string (see AGENTS.md §4).

## Current state of `cross-platform` (already done — do NOT redo)

- `getChannelForMediaTrack()` already uses `==` (not the `=` bug). -> PR commit
  `72edc28` is a no-op here.
- `UpdateGlobalSoloLED()` is already called in `Run()` (csurf_mcu.cpp:1056).
- FX Favorite feature fully present.
- `MediaTrackInfo` helpers already have `assert + if (pMT==NULL)` guards on the
  *track pointer* argument (but NOT on the value returned by
  `GetSetMediaTrackInfo` — see P0 below).

## Per-change mapping

Risk key: LOW = pure local edit, no extender interaction.
         MED = touches code that extender support also touches; verify.
         HIGH = semantic change in a path shared with extender logic; needs care.

### P0 — NULL-guard dereference of GetSetMediaTrackInfo return values  (LOW)

PR: Tracks.cpp `MediaTrackInfo::isShownInTCP/isShownInMCP/getHeight`
    change `return *bShown;` -> `return bShown ? *bShown : false;`
    and `return *pHeight;` -> `return pHeight ? *pHeight : 0;`
X-P: src/core/Tracks.cpp:294-300, 308-314, 327-329 (already guard `pMT`, NOT
     the returned pointer).
Note: cross-platform already guards `pMT` itself; PR additionally guards the
      returned pointer. Pure defensive addition, no behavioural risk.
Risk: LOW.

### P1 — Skip buildGraph() when track list unchanged  (MED)

PR: Tracks.h: `typedef std::set<MediaTrack*> tTrackSet` -> `std::vector`
    Tracks.cpp `tracksStatesChanged()`:
      - fill `m_pAllTracksNow` via `push_back` (ordered)
      - early-exit `if (*m_pAllTracksNow == *m_pAllTracksBefore) return false;`
      - build temporary `std::set`s only for the set_difference add/remove pass
      - replace final "re-fill before-set via iterator" with `*before = *now`
X-P: src/core/Tracks.h:302 (`typedef std::set<MediaTrack*> tTrackSet;`)
     src/core/Tracks.cpp:480-536 `tracksStatesChanged()`
Note: `m_pAllTracksBefore/Now` are heap pointers (`new tTrackSet()`); assignment
      works for vector too. The O(n) vector equality is the win. The set_difference
      still needs sorted inputs, hence the temporary sets (PR keeps these).
Risk: MED — `m_pAllTracksBefore/Now` are also used only inside `tracksStatesChanged`,
      so the type change is local; but verify no other consumer assumes `std::set`
      ordering/semantics (grep: only the two pointers, no external use).

### P2 — Remove tracksStatesChanged() from SetSurfaceVolume()  (MED)

PR: csurf_mcu.cpp `SetSurfaceVolume()` drops the
    `if (Tracks::instance()->tracksStatesChanged()) m_pCCSManager->trackListChange();`
X-P: src/core/csurf_mcu.cpp:1324-1330 `SetSurfaceVolume()`
Note: With P6 (run every frame) the per-callback rebuild is redundant and the PR
      calls it an O(n^2) on every fader move. Safe to remove once Run() drives it.
Risk: MED — confirms that track-add/remove is fully handled by the Run() path
      before any fader-driven code path depends on it.

### P3 — Incremental selection in SetSurfaceSelected()  (MED)

PR: Tracks.h: new `void updateSelection(MediaTrack*, bool);`
    Tracks.cpp: new impl — insert/erase into `m_selectedTracks` directly, then
                conditional `moveSelectedTrack2MCU()` if single-track changed.
    csurf_mcu.cpp `SetSurfaceSelected()`: replace `selectionChanged()` with
                `updateSelection(trackid, selected);`
X-P: src/core/Tracks.h:192 (add decl), src/core/Tracks.cpp:421 (selectionChanged
     stays for the adjust()-internal rebuild at :741), add new `updateSelection`.
     src/core/csurf_mcu.cpp:1339 replace the call.
     src/modes/plugin/PlugAccess.cpp:569 ALSO calls `selectionChanged()` — leave
     it (different path, full rebuild is fine there).
Note: `m_selectedTracks` is `std::set<MediaTrack*>` in cross-platform (direct
      member, not via typedef) — `insert`/`erase` exist on set, so the PR's set
      usage maps 1:1. (P1 changes the *trackList* set type, not this one.)
Risk: MED — make sure the conditional `moveSelectedTrack2MCU()` trigger matches
      the existing `selectionChanged()` tail exactly (it does in the PR).

### P4 — Gate MCP/TCP adjustment loops on a dirty flag  (MED)

PR: Tracks.h: new `bool m_adjustNeeded;` (init true in ctor)
    Tracks.cpp `adjust()`: wrap the MCP+TCP GetSetMediaTrackInfo loops in
      `if (m_adjustNeeded) { m_adjustNeeded = false; ... }`
    `setGlobalOffset()` sets `m_adjustNeeded = true;`
X-P: src/core/Tracks.h: add member near `m_globalOffset`.
     src/core/Tracks.cpp:730 `adjust()` (note the cross-platform prologue that
     clamps numMCUChannels to [8,64] and may call setGlobalOffset — the flag must
     be set there too, which it is via setGlobalOffset).
     src/core/Tracks.cpp:1234 `setGlobalOffset()` — set flag before/after the
     clamp work.
Note: cross-platform `adjust()` already differs (extender channel clamp). The
      flag gates the MCP/TCP visibility loops only — keep the channel-clamp and
      `updateTrackStates()` prologue OUTSIDE the gate (they are cheap / already
      guarded). Must also set the flag wherever a bank/page/visibility change
      happens (see P11).
Risk: MED — mis-setting the flag = stale TCP/MCP visibility. Set generously.

### P5 — Move UpdateAutoModes() out of per-track callbacks  (MED)

PR: Remove `m_pMCU->UpdateAutoModes();` from `CCSManager::trackSelected()`.
    Add `UpdateAutoModes();` to `Run()` (once per frame).
X-P: src/core/CCSManager.cpp:566 (remove call in trackSelected).
     src/core/csurf_mcu.cpp:1031 `Run()` — add `UpdateAutoModes();` next to the
     existing `UpdateGlobalSoloLED();` at :1056.
Note: `UpdateGlobalSoloLED()` is ALREADY in Run() (line 1056) and STILL also in
      `SetSurfaceSolo()` (:1345). For consistency with the PR, the per-track
      `UpdateGlobalSoloLED()` in `SetSurfaceSolo()` should also be removed (it is
      the exact pattern the PR optimises: once-per-frame, not once-per-track-callback).
      Keep `SetAutoMode()` -> `UpdateAutoModes()` (:1401): that is the legitimate
      automation-mode-change trigger, not a per-track broadcast.
Risk: MED — extender: `trackSelected()` builds the shared `m_selected_tracks`
      list used by UpdateAutoModes; calling UpdateAutoModes once per frame in Run()
      still sees the fully-updated list. Verify Run() ordering vs. trackSelected.

### P6 — tracksStatesChanged() runs every frame (crashfix)  (HIGH)

PR: csurf_mcu.cpp `Run()`: drop the `if (runCounter % 3 == 0)` gate around
    `tracksStatesChanged()` so it runs every frame (cheap because of P1's
    early-exit), and so a deleted track is flushed before any code can deref it.
X-P: src/core/csurf_mcu.cpp:1042-1045
        runCounter++;
        if (runCounter % 3 == 0)
          if (Tracks::instance()->tracksStatesChanged())
            m_pCCSManager->trackListChange();
     -> remove the `% 3` gate (keep runCounter if used elsewhere, else drop).
Note: This is the use-after-free fix (project switch/close). Depends on P1's
      early-exit being in place, otherwise this re-introduces the O(n^2) cost.
Risk: HIGH — this is the headline crashfix. Must land together with P1. Extender
      consideration: `trackListChange()` fans out to all units; confirm it is
      idempotent when nothing changed (it already early-exits inside).

### P7 — O(1) pointer map for getTrackStateForMediaTrack()  (MED)

PR: Tracks.h: `#include <unordered_map>` + member
      `std::unordered_map<MediaTrack*, TrackState*> m_tracksByPointer;`
    Tracks.cpp:
      - `tracksStatesChanged()`: on add, `m_tracksByPointer[pMT] = pTS;`
                                on remove, `m_tracksByPointer.erase(pTS->getMediaTrack());`
      - `getTrackStateForMediaTrack()`: try the map first, fall back to the GUID
        scan (needed because REAPER reassigns a new MediaTrack* on project restore).
X-P: src/core/Tracks.h:296 (add member near `m_trackStates`).
     src/core/Tracks.cpp: getTrackStateForMediaTrack (find def line), and the
     add/remove branches in tracksStatesChanged (:505, :518-523).
Note: cross-platform already uses `std::map<String, TrackState*> m_trackStates`.
      The new map is a secondary index, kept in sync. ~45 call sites of
      getTrackStateForMediaTrack benefit; correctness depends on keeping the map
      in sync on every add/remove (including the projectChanged path).
Risk: MED — must wire the erase into the remove branch AND any other place that
      deletes a TrackState (grep `delete (m_trackStates` / `m_trackStates.erase`).

### P8 — Flush m_selectedTracks on track removal (crashfix)  (MED)

PR: Tracks.cpp remove branch in `tracksStatesChanged()`: add
      `m_selectedTracks.erase(pMT);` before deleting the TrackState, because
      REAPER does not always fire SetSurfaceSelected(child,false) when a folder
      track is deleted.
X-P: src/core/Tracks.cpp:513-525 (remove branch).
Note: `m_selectedTracks` is `std::set<MediaTrack*>`; erase of a non-present
      pointer is a no-op. Land together with P3 (which makes selection incremental
      and therefore more sensitive to stale entries).
Risk: MED.

### P9 — NULL-guard GetSetMediaTrackInfo dereferences in adjust()  (LOW)

PR: Tracks.cpp `adjust()`: add `if (!pShowInTCP) continue;` (and the MCP_ALL
    branch already gets a `pShowInMixer &&` guard).
X-P: src/core/Tracks.cpp:787-800 (TCP_BANK/SELECTED branch has NO guard on
     `pShowInTCP`); MCP_ALL branch :762 also dereferences without guard.
Note: cross-platform already guards `pShowInMixer` in MCP_BANK (:754) but not
      in MCP_ALL or in the TCP branches. Mirror the PR guards.
Risk: LOW (defensive, same as P0).

### P10 — moveSelectedTrack2MCU() trackExists guard (LOW)

PR: Tracks.cpp `moveSelectedTrack2MCU()`: early-return if
      `!m_structure.trackExists(trackid)` before calling getTrackState on a
      pointer that was captured before tracksStatesChanged() ran.
X-P: src/core/Tracks.cpp:437-447 (the method currently calls
     getTrackStateForMediaTrack(trackid) unconditionally; it already null-checks
     the returned `pTS`, but that path can still call GetTrackGUID on freed memory
     via the slow GUID scan — the PR short-circuits before that).
Risk: LOW.

### P11 — MCU bank not updating when tracks shown/hidden in TCP  (MED)

PR: commit `c805f91` "fix: MCU bank not updating when tracks shown/hidden in TCP"
    sets `m_adjustNeeded = true` whenever TCP/MCP visibility could have changed
    externally (so P4's gate re-runs the loops).
X-P: identify every place TCP visibility changes outside setGlobalOffset and set
     the flag there. Candidates: `MultiTrackOptions.cpp:41`
     (`Tracks::instance()->tracksStatesChanged();`), option-change handlers,
     anchor changes. The PR essentially treats this as "set the dirty flag
     anywhere the bank window could move".
Note: This is the commit that makes P4 safe — without it, hiding/showing tracks
      in the TCP would not refresh the MCU bank.
Risk: MED — easy to miss a trigger site; when in doubt set the flag.

## Recommended commit order on `cross-platform`

Each step = one commit, build + deploy + smoke-test after each.

1. P1  + P6         — buildGraph skip + run-every-frame + the four project-switch
                      crashfix sub-parts (a..d). P1's early-exit makes P6a cheap;
                      the null guards (P6 b,c,d) belong to the same crashfix
                      commit and ride along. This is the headline change.
2. P7               — O(1) pointer map (independent, big call-site win).
3. P3  + P7-flush   — incremental selection + selectedTracks flush on removal.
4. P2               — remove tracksStatesChanged() from SetSurfaceVolume.
5. P5               — move UpdateAutoModes() to Run(), drop per-track calls
                      (incl. the redundant UpdateGlobalSoloLED in SetSurfaceSolo).
6. P4  + P8         — dirty-flag gating of MCP/TCP loops + all trigger sites
                      (TCP show/hide fix).

Optional / verify-only:
- PR commit `fd82807` "remove per-callback updateFader() from SetSurfaceVolume()":
  cross-platform `SetSurfaceVolume` still calls `m_pCCSManager->updateFader();`.
  Decide deliberately whether to drop it (it is the actual fader-move echo path;
  the PR removed it as redundant once Run() drives updates). -> needs a runtime
  check that faders still track; default: KEEP unless a measurable gain is shown.

## Files that will be touched

- src/core/Tracks.h
- src/core/Tracks.cpp
- src/core/CCSManager.cpp   (only the UpdateAutoModes removal in P5)
- src/core/csurf_mcu.cpp    (Run(), SetSurfaceVolume, SetSurfaceSelected,
                             SetSurfaceSolo)

Estimated net change: ~+130/-90 lines, matching the PR's "~128 lines of actual
functional code" once instrumentation/docs are excluded.

## Validation per step

- Build: `cd build && cmake .. -DCMAKE_BUILD_TYPE=Release &&
          cmake --build . -- -j$(nproc)`
- Deploy: `cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/`
- Full REAPER restart, then smoke test: bank up/down, Ctrl+A on a large project,
  add/remove folder track with selected children, switch/close project, hide/show
  tracks in TCP (for P11).
