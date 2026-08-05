# Add-Track Performance Investigation — Status (in progress)

Date opened: 2026-08-05
Related: `ai-docs/large-project-performance-audit.md`, commit `04af225`

## TL;DR

Adding a track to a large project (≈300 tracks) is visibly slow **and the user
confirmed it is control-surface related** (with the surface removed from
REAPER's preferences the add is fast; with it added it lags). Investigation has
**ruled out the surface's `Run()` loop and its callback bodies** as the cause.
The per-add stall is a ~280–340 ms main-thread gap that coincides with REAPER
re-broadcasting the full set of `Set*` callbacks to the registered surface.
The decisive next step is a "neuter-callbacks" build to separate "REAPER's cost
of dispatching the broadcast" from "REAPER-internal track-add work that only
runs because a surface exists".

Nothing in this file is a committed plan; it is a working status so we can pick
up where we left off.

## Symptom

- "Adding tracks gets slower the larger the project."
- Confirmed A/B by the user: **without** the control surface enabled, adding
  tracks is fast; **with** it, each add to a ≈300-track project stalls.

## What was tried

1. **Pointer index `m_tracksByPointer` (Audit item #3 / impl-order #1).**
   Implemented and committed in `04af225` (`src/core/Tracks.{h,cpp}`). Makes
   `getTrackStateForMediaTrack()` O(1) and turns the `TSGraph::buildGraph()`
   rebuild from O(n²) to O(n). **This is correct and should stay** — it kills a
   real quadratic in the graph rebuild. It did **not** fix the perceived
   add-track lag, which is why the investigation continued.
2. **FOLLOW_REAPER analysis.** With `MTO2A_FOLLOW_REAPER_ON` (the default) a
   track add can trigger a second `buildGraph()` (inside `moveBaseTrack()`) and
   an offset-stepping while-loop. Ruled out as the main cause: the stall
   reproduces with FOLLOW_REAPER **off**.

## Measurement setup (currently in the working tree, UNCOMMITTED)

A temporary `MCU_TIMING` build switch plus `MCU_DEBUGLog`-forced-on drives
lightweight instrumentation. The probes live in:

- `CMakeLists.txt` — temporary block forcing `MCU_DEBUG_LOG` + `MCU_TIMING` in
  every config (so a Release build emits the log). **Revert after profiling.**
- `src/core/McuDebugLog.h` — `MCU_TIMING_SCOPE` (RAII chrono scope) and
  `MCU_TIMING_LOG` macros; inert when `MCU_TIMING` is undefined.
- `src/core/csurf_mcu.cpp` — file-static callback counters `g_cb_{vol,pan,sel,
  solo,tlc}`, incremented in each `Set*` callback; a `@run` timestamp logged on
  every `Run()` entry; per-gated-frame `FRAME` header logging track count,
  timestamp, and the callback-counter deltas (reset each frame); RAII phase
  scopes around `tracksStatesChanged`, `PlugMoveWatcher::checkMovement`,
  `adjust`, `UpdateGlobalSoloLED`, `frameUpdate`, `displayResend`.
- `src/hardware/display/DisplayHandler.cpp` — the per-write `ROW0/ROW1` display
  diff log is suppressed under `#ifndef MCU_TIMING` (its `fopen`/`fclose` per
  call otherwise dominates and skews timings).

Build / deploy / test loop used: `scripts/build-windows-from-wsl.sh`
(reconfigure once after the CMake change, then incremental). Log file:
`%APPDATA%\REAPER\mcu_klinke_debug.log` (truncate before each test).

> **Lesson learned (important):** an earlier attempt logged a line **per**
> `Set*` callback via `MCU_LOG`, which calls `fopen`/`fclose` every line. During
> an add REAPER fires ≈1200 callbacks, so the logging alone added ≈120 ms and
> made the timing unusable. Always use cheap counters + one log line per frame
> for high-frequency events.

## Key findings (clean measurement, ≈300-track project, 5 single-track adds)

### Run()'s own work is tiny and NOT the cause

Phase timing on frames with n ≥ 300 (avg / max, ms):

| phase                |   avg |    max |
|----------------------|------:|-------:|
| adjust               | 0.184 |  0.311 |
| frameUpdate          | 0.071 |  0.120 |
| PlugMoveWatcher      | 0.023 |  0.063 |
| tracksStatesChanged  | 0.013 |  0.352 | (0.352 = the buildGraph spike on the add frame)
| UpdateGlobalSoloLED  | 0.009 |  0.019 |
| displayResend        | 0.003 |  0.052 |

Total surface `Run()` work per frame ≈ **0.3 ms** (frame budget at 30 Hz is
33 ms). So whatever blocks REAPER during an add is **not** the surface's
`Run()`.

### The per-add stall is real and lives BETWEEN frames

Clean inter-frame gaps (no per-callback fopen):

- project load (0 → 301): 2771 ms (normal)
- 301 → 301: 689 ms (load tail)
- **each single-track add: 263, 293, 311, 319, 341 ms** (consistent ≈280–340 ms)

During those gaps `Run()` is essentially not called — REAPER owns the main
thread.

### Callback broadcast volume

Over the short test REAPER fired into the surface:

- `SetSurfaceVolume` 2738, `SetSurfacePan` 2738, `SetSurfaceSolo` 2736,
  `SetSurfaceSelected` 2139, `SetTrackListChange` 7.

That is roughly **one full re-broadcast per track-change event** (4 properties
× ≈300 tracks ≈ 1200 callbacks), arriving in ≈7 burst frames. The callback
bodies themselves are cheap (a `std::map` insert for volume/pan, a `++counter`,
an O(selection) linked-list walk for selected). So the per-add stall is not the
surface *executing* the callbacks.

## Leading hypothesis

The ≈280–340 ms per-add stall is REAPER's main-thread cost of (a) performing
the track-add and (b) generating + dispatching the full `Set*` re-broadcast to
the registered surface. With no surface registered, REAPER skips the broadcast
→ fast. The surface's own code (`Run()` + callback bodies) is not the
contributor.

Open sub-question that could still implicate the surface: does the surface
**itself** call `TrackList_UpdateAllExternalSurfaces()` during/after an add,
which would make REAPER re-broadcast again (a self-induced cascade)?
Candidates to check:

- `src/modes/multitrack/MultiTrackMode.cpp:57` — `frameUpdate()` calls
  `TrackList_UpdateAllExternalSurfaces()` when `clampCurrentGlobalOffset()`
  returns true. Adding tracks changes the valid offset range, so this can fire.
- `src/modes/multitrack/MultiTrackMode.cpp:280` and `:459`
- `src/core/csurf_mcu.cpp:293`

## Decisive next test (when we resume)

**Primary approach: profile with the Visual Studio CPU sampler.** This is
better than another variant build because it attributes the ~280 ms add stall
across the *whole* process — REAPER-internal track-add work vs. dispatching
the surface callbacks vs. our callback bodies — without guessing. The VS
profiler shows module-level CPU (`reaper_csurf_mcu_klinke_x64.dll` vs
`reaper.exe`) plus, with our PDB, our function names and call stacks.

Two prep steps are required first (both confirmed against the tree):

1. **Enable symbols in an optimized build.** `CMakeLists.txt:264` only sets
   `/Zi` under `$<CONFIG:Debug>`, so the current Release DLL has no PDB (none
   found under `build_win/`). Build `RelWithDebInfo`, or add `/Zi` (compile)
   and `/DEBUG` (link) to the Release flags, so the PDB lands next to the DLL
   and VS resolves our symbols.
2. **Disable the `MCU_TIMING` probes for the profile run.** The instrumentation
   committed in `77332d3` writes a log line per frame (`fopen`/`fclose`), which
   shows up as noise in the profile (hot `mcu_log_write`/`fopen`). Flip the
temporary CMake block off (or pass the option as OFF) for the profile build.

Workflow: VS → Performance Profiler → CPU Usage, launch `reaper.exe` with the
surface DLL loaded, reproduce (load the big project, add 2–3 tracks), stop.
Look at module attribution and the hot path:

- If `reaper.exe` dominates during the add and our `Set*` callbacks have high
  *inclusive* but near-zero *exclusive* time → REAPER's broadcast dispatch is
  the cost; our callback bodies are cheap (confirms the hypothesis).
- If our DLL functions are genuinely hot → fix the hot callback/path.
- If the hot path is REAPER-internal track/TCP work that runs only because a
  surface is registered → little the surface can do directly (consider
  `PreventUIRefresh` around batch surface updates, or declining unwanted `Set*`
  notifications at registration time).

## Fallback / complementary test (only if profiling is inconclusive)

Build a "neuter-callbacks" variant: every `Set*` callback (`SetSurfaceVolume`,
`SetSurfacePan`, `SetSurfaceSelected`, `SetSurfaceSolo`, `SetSurfaceMute`,
`SetSurfaceRecArm`, `SetTrackListChange`) early-returns immediately, leaving
`Run()` untouched. Re-measure the per-add gap.

- **Gap → ~0** → cost is REAPER dispatching the callbacks. Investigate whether
  `reaper_csurf_reg_t` / `csurf.h` can decline volume/pan/solo notifications,
  and whether the surface triggers `TrackList_UpdateAllExternalSurfaces` itself
  (`clampCurrentGlobalOffset` path, `MultiTrackMode.cpp:57`, is prime suspect).
- **Gap stays ≈280 ms** → REAPER-internal track-add work conditional on a
  surface existing.

## Tree state right now

Committed (in `04af225`, keep):

- `src/core/Tracks.h`, `src/core/Tracks.cpp` — the `m_tracksByPointer` O(1)
  index and the `getTrackStateForMediaTrack()` rewrite + 4 synced mutation
  sites. Correct; kills the O(n²) graph build.

Uncommitted in the working tree (measurement instrumentation — revert or
formalize when done):

- `CMakeLists.txt` — temporary `target_compile_definitions(... MCU_DEBUG_LOG
  MCU_TIMING)` block (forces probes on in every config).
- `src/core/McuDebugLog.h` — `MCU_TIMING` block (Scope/logMs macros).
- `src/core/csurf_mcu.cpp` — `@run` timestamp, `FRAME` header with callback
  counters, phase scopes, file-static `g_cb_*` counters, `++g_cb_*` in the
  `Set*` callbacks.
- `src/hardware/display/DisplayHandler.cpp` — `#ifndef MCU_TIMING` guard on the
  ROW0/ROW1 log.
- `VERSION.txt` — auto-bumped build counter (harmless, leave).

To revert the instrumentation but keep a permanent, inert profiling hook later:
remove the temporary CMake block (so `MCU_TIMING` is off by default) and keep
the gated macros/probes (they compile to nothing when `MCU_TIMING` is
undefined).

## Next-session checklist

1. **Profile with the VS CPU sampler** (see "Decisive next test"). First:
   enable symbols in an optimized build and disable `MCU_TIMING`.
2. Interpret the profile: module attribution + hot path (DLL vs `reaper.exe`,
   inclusive vs exclusive time of our `Set*` callbacks).
3. Only if profiling is inconclusive: run the neuter-callbacks variant test.
4. Check whether the surface calls `TrackList_UpdateAllExternalSurfaces`
   during an add (`clampCurrentGlobalOffset` path, `MultiTrackMode.cpp:57`) —
   log those call sites under `MCU_TIMING` if the profile points there.
5. Based on the evidence: gate the surface's self-induced re-broadcast, decline
   unwanted `Set*` notifications at registration, or accept it is
   REAPER-internal.
