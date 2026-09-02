# Record Mode / DROP Button Analysis (Issue #3)

> Analysis for GitHub issue #3 "Improve record mode" — focused on the
> DROP button and the global record-mode handling.
> https://github.com/s-fuerst/CSurf_Klinke_MCU/issues/3
>
> Status: research only, no code changed yet.

## 1. What the manual says

The manual documents the DROP button in `manual/text_en/misc.tex`:

> \item \drop: Toggles Record Mode \footnote{Please be aware that \mcu
>     resets the Record mode to normal at initialization of the MCU
>     and that changes made in \reaper itself are not reflected by the
>     MCU and will be overwritten when the Record button on the MCU is
>     pressed.} \bemod
> \begin{itemize}
>     \item LED off:   "Record mode: normal"
>     \item LED on:    "Record mode: time selection auto punch"
>     \item LED flashing: "Record mode: auto-punch selected items"
> \end{itemize}

The footnote is an explicit admission of two (really three) defects in
the current implementation, described below.

## 2. Current implementation

### 2.1 The state machine

`DropState` in `src/core/csurf_mcu.h` (~line 230) is a local 3-state
counter owned by `CSurf_MCU` as member `m_dropstate`:

- `DROP_NORMAL (0)` → `ID_RECORD_MODE_NORMAL` ("record mode: normal")
- `DROP_TIME (1)`   → `ID_RECORD_MODE_TIME` ("record mode: time-based")
- `DROP_ITEM (2)`   → `ID_RECORD_MODE_ITEM` ("record mode: item-based")

It is applied to REAPER exclusively via
`SendMessage(g_hwnd, WM_COMMAND, ID_RECORD_MODE_*, 0)` in
`DropState::updateReaper()`. The LED mapping is:
off / on / blinking.

**The plugin is the source of truth. REAPER's actual record mode is
never read.**

### 2.2 The three problem sites

1. **Init forces "normal"** — `CSurf_MCU::MCUReset()`
   (`src/core/csurf_mcu.cpp` ~line 246) calls
   `m_dropstate.updateReaper()`. `MCUReset()` runs at plugin
   initialization and on every MCU hardware reset SysEx
   (`OnMCUReset()`), so REAPER's project record mode is unconditionally
   reset to "normal". This is the first sentence of the footnote.

2. **Record button overwrites REAPER** — `CSurf_MCU::OnTransport()`
   (csurf_mcu.cpp ~line 526) calls `m_dropstate.updateReaper()` before
   *every* record-button press. Whatever the user changed in REAPER's
   UI is silently clobbered by the plugin's stale local state. This is
   the second sentence of the footnote.

3. **LED does not track REAPER** — the DROP LED is only written in
   `OnDropButton()`, `OnMCUReset()` and the config-change path
   (csurf_mcu.cpp lines ~288, ~527, ~752-754, ~943). REAPER-side
   changes never update the LED, so the controller and REAPER can
   silently diverge, and the next DROP press lands in an unexpected
   mode (the local counter, not REAPER's state, is incremented).

### 2.3 Related code (context, not part of the DROP bug)

- Transport record button: `Transport::recordButton()`
  (`src/core/Transport.cpp` line 244) → plain `CSurf_OnRecord()`,
  no modifier variants.
- Stop button: no modifier → `ID_STOP_AND_SAVE_MEDIA`, with any
  modifier → `CSurf_OnStop()` (Transport.cpp ~line 250).
- Per-channel rec arming already works properly via
  `MultiTrackMode::buttonRec()` (arm / Shift=monitor /
  Option=Input-None / Alt=output modes) and is polled from
  `I_RECARM` per frame in `updateRecLEDs()`.
- `CSurf_MCU::SetSurfaceRecArm()` is an empty stub
  (csurf_mcu.cpp ~line 1443); harmless because LEDs are polled per
  frame, but the clean path would be the callback.
- `ClearAllRecArmed` is resolved in `src/csurf_main.cpp` (line 303)
  but never used anywhere.

## 3. API findings (pinned SDK)

The pinned REAPER SDK in the repo provides everything needed:

- **`GetToggleCommandState(int command_id)`** — returns the
  toggle/checkmark state of a command. Available since the REAPER 4.0
  era.
- **`GetToggleCommandStateEx(int section_id, int command_id)`** —
  same, section-aware; returns 0=off, 1=on, -1=NA.
  (see `reaper-sdk/sdk/reaper_plugin_functions.h` ~line 3449-3475)

Both are far below the current REAPER 6.37 floor, so plain mandatory
`IMPAPI` resolution is fine — **no floor change, no manual version
note needed**.

Reading the global record mode: query
`ID_RECORD_MODE_TIME` and `ID_RECORD_MODE_ITEM`:

- `TIME == 1`  → time-based
- `ITEM == 1`  → item-based
- neither      → normal

(The three menu entries are mutually exclusive checkmarks, so at most
one can report "on".)

Notes:

- There is **no** `GetSetProjectInfo`/`GetSetProjInfo` key for the
  global record mode in the pinned SDK docs — the toggle-state query
  is the way.
- Whether `GetToggleCommandState` reliably reports the record-mode
  checkmarks must be verified on the real REAPER install (menu
  checkmarks normally go through exactly this mechanism, so this is
  expected to work). Fallback if it returns -1/0 for all three: keep
  the local counter as state, still stop clobbering REAPER (see below).

## 4. Proposed fix

REAPER becomes the source of truth; `m_dropstate` degrades to a cache.

1. **New helper** on `CSurf_MCU`, e.g.
   `int queryRecordMode()`:
   - returns 0/1/2 from `GetToggleCommandState` queries
   - on API failure (NA/-1) falls back to the cached `m_dropstate`
2. **`MCUReset()`**: replace `m_dropstate.updateReaper()` with
   *read* REAPER → update `m_dropstate` + LED. No more forcing
   "normal" at init or hardware reset.
3. **`OnDropButton()`**: read current mode → increment → apply → set
   LED. (LED always ends up in the state that was actually applied.)
4. **`OnTransport()` B_RECORD**: delete the `m_dropstate.updateReaper()`
   call — no more clobbering on record press.
5. **Optional (needs maintainer decision)**: re-sync `m_dropstate` +
   LED from REAPER in `Run()` (cheap: two `GetToggleCommandState`
   calls per frame, same pattern as the per-frame REC-LED polling in
   `MultiTrackMode::updateRecLEDs()`). This makes REAPER-UI changes
   visible on the controller live and removes the *entire* footnote,
   not only the overwrite part.
6. **Manual**: delete the footnote in `manual/text_en/misc.tex`
   (behavior no longer matches it).

### Impact / risk

- Behavior change: the record mode of an opened project is no longer
  reset to "normal" at plugin load. This is a visible, user-facing
  change — the manual footnote documents the *current* (arguably
  wrong) behavior, so removing it is the point, but worth a mention in
  the commit message / release notes.
- `OnMCUReset()` already re-publishes the DROP LED after reset
  (csurf_mcu.cpp ~line 288), so the LED stays consistent if step 2 is
  implemented.
- No new config, no new resources, no csurf registration changes.

## 5. Open questions (for the maintainer)

1. Do we want the per-frame re-sync in `Run()` (option 5), or the
   minimal version (read only at init / drop press)?
2. If `GetToggleCommandState` turns out to be NA for the record-mode
   commands on the test system: accept "keep local state, stop
   clobbering" as the outcome, or dig further (e.g. poll via
   `Main_OnCommand` round-trip is not viable; `GetToggleCommandState2`
   with a section pointer could be a second attempt)?
3. Should the issue also cover the transport record-button modifier
   variants (record from start / at marker / punch) and the unused
   `ClearAllRecArmed`, or keep issue #3 strictly about the DROP
   button / global record mode?
