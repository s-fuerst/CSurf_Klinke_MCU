# WP-PlugMode — Work State (2026-07-13 end of day)

## Phases completed

| Phase | Status | What |
|-------|--------|------|
| 0 | ✅ done | Per-unit state model, explicit overloads, persistence, input opt-in, m_activeUnit |
| 1 | ✅ done | Per-unit display + switchDisplay (MultiDisplay routing) |
| 2 | ✅ done | Per-unit strip buttons, LEDs, faders, VPOTs |
| 3 | ✅ done | Per-unit BankPagePlugSelector array, vpotPressed dispatch |
| 4 | ✅ done | Transport lock-step page-spread + Control+cascade |
| 5 | ✅ done | followChanges unit option + lastFaderValues vector |
| 6 | ✅ done | CONFIG_FLAG_PROX removal (per-unit isProX) |
| 7 | ✅ done (audit) | MeterBridge verify — no changes needed |
| 8 | ✅ done (code) | Integration, edge cases, N=1 regression — code review + build; manual test pending |

## Bugs — OPEN (N>1, 2-unit surface)

### B1: Extender display blank / shows previous mode content
- When switching TO PlugMode → display stays on previous mode's content
- After plugin selection → display is blank
- **Investigation so far:** Traced the `switchDisplay()` → `switchToAll()` → `DisplayHandler::switchTo()` chain. For N>1, my code calls `md->switchToAll()` directly (bypasses `CCSManager::switchToDisplay` guard). `switchToAll()` switches each child on its unit's handler. `DisplayHandler::switchTo()` calls `child->activate()` → `resendAllRows()` → `resendRow()` → `sendDifferences()`. This SHOULD send data to hardware. **The mechanism looks correct in theory but doesn't work in practice.** Possible causes still unexplored:
  - `DisplayHandler::switchTo()` early-returns if `m_pActualDisplay == pDisplay` already — maybe handlers are stuck on previous mode's displays
  - MultiDisplay children might not have correct `DisplayHandler` pointers
  - Hardware state comparison in `sendDifferences` might suppress the update

### B2: Crash when touching/moving extender fader
- N=1 works fine, crash only on extender (channel > 8)
- Could be on touch, could be on movement — unclear
- **Investigation so far:** Traced the full event path:
  1. `CCSManager::fader(channel, value)` → passes `supportsExtendedChannels` gate
  2. `PlugMode::fader(channel, value)` → `setActiveUnit(unit)` → `setParamValueInt(bank, page, FADER, localCh, value)` — safe when `!plugExist()`
  3. `CCSManager::faderTouched(channel, touched)` → `elementTouched(FADER, channel, touched)` → `singleFaderTouched(9)` 
  4. `PlugMode::singleFaderTouched(9)` → `localCh=0`, `m_iSingleFaderTouched=9`, `switchDisplay()`
  5. `switchDisplay()` → global message path if no plugin → `switchToAll() + clearNonAnchorChildren`
- **No obvious NULL deref or array OOB found.** The path appears valid. Need AddressSanitizer build (`build_asan/` exists, needs running inside REAPER).
- Also noted: `updateRecLEDs()` → `plugExist()` returns true when `m_iSlot == -1` but track has FX → `TrackFX_GetFXGUID(track, -1)` called → potential crash. Pre-existing bug, but now called for ALL units (not just first 8 channels).

### B3: Select buttons on extenders (was fixed, untested)
- `buttonSelect(channel, …)` was using `channel` (1-16) as slot, now uses `localCh`
- `updateSelectLEDs` now replicates to all units
- `setSelectedBank/setSelectedPage` now always update LEDs (removed `unit == activeUnit` guard)
- **Not yet verified by user** — was deployed but display/crash bugs prevented testing.

## Plan corrections applied

1. ASSERT guard in `ElementDesc` selected-based ctor (R1)
2. Fixed `getSelectedPageInSelectedBank` pseudocode (Phase 0c)
3. `vpotPressed` moved from Phase 2c to Phase 3e (selector array must exist first)

## Phase 4 — implementation notes (2026-07-20)

- **buttonFaderBanks rewritten** to the lock-step window model (R3/R11):
  - BANK_UP/DOWN: reference bank = `selectedBankForUnit(anchorUnit())`; find
    next/prev USED bank; set ALL units to that bank with pages spread over
    sequence offsets `[0..N-1]` (`pageAtUsedOffset`). Window reset on bank
    change. No longer routes through `buttonSolo`/`buttonMute` strip handlers.
  - CHANNEL_UP/DOWN: window start derived from anchor unit's current page via
    the new `pageUsedOffsetForPage()` inverse helper; shifted by ±N sequence
    positions; each unit u shows `pageAtUsedOffset(bank, newOffset + u)`;
    clamped, no wrap.
  - Selector activation (BANK/PAGE) still fires on the anchor unit's selector.
- **N=1 note (intended, minor):** CHANNEL_UP/DOWN stays behaviour-equivalent
  (one used page per press). BANK_UP/DOWN now resets each unit's page to the
  first used page of the new bank (the window position) instead of the legacy
  per-bank remembered page. Documented in the method comment.
- **Control+cascade (4b):** `buttonSolo` now branches on `VK_CONTROL`:
  `cascadeFromUnit(unit, localCh, 0)` spreads bank + sequence pages over
  units `unit..N-1`; units `0..unit-1` unchanged. Without Control the legacy
  single-unit bank select is preserved.
- **New helpers:** `PlugAccess::pageUsedOffsetForPage(bank, page)` (inverse of
  `pageAtUsedOffset`); `PlugMode::cascadeFromUnit(unit, bank, baseOffset)`.
- Build: clean, zero warnings on Linux. Deployed.
- Still pending: Phases 5-8. The open bugs B1/B2/B3 were NOT addressed (per
  maintainer decision: continue with Phase 4, defer hardware-test bugs).

## Next steps (when resuming)

1. **Fix the crash (B2) first** — run the AddressSanitizer build to find exact crash location
2. **Fix the display (B1)** — add MCU_LOG traces around `switchToAll()`, `switchTo()`, `sendDifferences()` to see if displays actually switch
3. Then continue with Phase 4

## Phase 5 — implementation notes (2026-07-20)

- **5a — `PMO2_FOLLOW_CHANGE` multi-valued (R2):** Replaced the old OFF/ON
  attributes in `PlugMode2ndOptions` with OFF + "Unit 1" … "Unit 8"
  (MAX_SURFACE_UNITS). The cyclic VPOT selector reaches every value; no
  `Options.cpp` change needed. New public helper
  `PlugMode2ndOptions::followChangeUnit(numUnits)` parses the selected
  attribute ("Unit N" → N-1) and returns -1 when OFF or when the chosen
  unit is >= numUnits() (so values beyond the live unit count are no-ops).
  `PlugMode::followChangeUnit()` delegates, passing `numUnits()`.
  `m_pPlugMode2ndOptions` retyped `Options *` → `PlugMode2ndOptions *`
  (header include added); `get2ndOptions()` still returns `Options *`.
  The `frameUpdate` trigger switched from
  `isOptionSetTo(PMO2_FOLLOW_CHANGE, PMO2A_ON)` to `followChangeUnit() >= 0`.
- **5b — `followChanges` action:** The `numChangedValues == 1` branch now
  calls the explicit-unit overloads `setSelectedBank(bank, fu)` /
  `setSelectedPage(bank, page, fu)` on the chosen follow unit `fu` instead
  of the active-unit overloads. With OFF (`fu < 0`) no cursor jump happens.
  N=1 with "Unit 1" reproduces the legacy behaviour.
- **5c — `lastFaderValues`/`lastVPotValues` → vector + invalidation (R2):**
  The fixed `double[8][8][8]` arrays became flat `std::vector<double>`
  (size 512, `[bank][page][channel]` via `paramCacheIndex()`). A new
  `m_paramCacheValid` flag + `invalidateParamCache()` / `refillParamCache()`
  pair guards against stale-map false changes:
  - `followChanges()` early-returns after a refill when the cache is invalid,
    so the first scan after a map load does not count as a change.
  - `invalidateParamCache()` is called after **every** `m_pAccess->accessPlugin(...)`
    call site in PlugMode.cpp (3 sites) and the cache starts invalid in the
    ctor, so the very first scan refills instead of seeding from zeros.
- Build: clean, zero warnings on Linux. Deployed.
- Bugs B1/B2/B3 still open (need 2-unit hardware); not touched here.

## Phase 6 — implementation notes (2026-07-20)

- **6a — `updateParamsDisplay` per-unit isProX (R7):** Merged the two
  separate ProX/MCU render loops into one. Each channel now picks the 4-row
  ProX layout (VPOT rows 0/1 + FADER rows 2/3) or the 2-row MCU layout
  (name/value touch switching) from
  `unitForChannel(iChannel+1)->isProX()` instead of the global
  `IsFlagSet(CONFIG_FLAG_PROX)`. The "Wet" field-9 column is now rendered on
  the anchor unit only when the anchor unit itself is ProX (N=1 ProX =
  byte-identical; non-ProX anchors skip it).
- **6b — `updateTouchedDisplayProX` inlined:** The separate method is gone;
  `updateTouchedDisplay` now branches once on the touched element's owning
  unit `isProX()` (target child resolution hoisted to the top, shared by
  both branches). Declaration removed from `PlugMode.h`.
- **6c — MeterBridge audit:** `PlugModeMeterBridge.{h,cpp}` has no
  `CONFIG_FLAG_PROX` / `IsFlagSet` usage — nothing to change (R12, Phase 7).
- **6d — includes:** `CONFIG_FLAG_PROX` reached PlugMode via `csurf_mcu.h`,
  which stays included (still need `numUnits`/`unitForChannel`/`isProX`).
  No PlugMode code references the symbol anymore; the define itself is
  untouched (WP-EF owns it).
- Build: clean, zero warnings on Linux. Deployed.
- Verification gap: N=1 MCU vs ProX equivalence is structural (same field
- writes per branch); mixed-unit N>1 needs real hardware.

- **5a — `PMO2_FOLLOW_CHANGE` multi-valued (R2):** Replaced the old OFF/ON
  attributes in `PlugMode2ndOptions` with OFF + "Unit 1" … "Unit 8"
  (MAX_SURFACE_UNITS). The cyclic VPOT selector reaches every value; no
  `Options.cpp` change needed. New public helper
  `PlugMode2ndOptions::followChangeUnit(numUnits)` parses the selected
  attribute ("Unit N" → N-1) and returns -1 when OFF or when the chosen
  unit is >= numUnits() (so values beyond the live unit count are no-ops).
  `PlugMode::followChangeUnit()` delegates, passing `numUnits()`.
  `m_pPlugMode2ndOptions` retyped `Options *` → `PlugMode2ndOptions *`
  (header include added); `get2ndOptions()` still returns `Options *`.
  The `frameUpdate` trigger switched from
  `isOptionSetTo(PMO2_FOLLOW_CHANGE, PMO2A_ON)` to `followChangeUnit() >= 0`.
- **5b — `followChanges` action:** The `numChangedValues == 1` branch now
  calls the explicit-unit overloads `setSelectedBank(bank, fu)` /
  `setSelectedPage(bank, page, fu)` on the chosen follow unit `fu` instead
  of the active-unit overloads. With OFF (`fu < 0`) no cursor jump happens.
  N=1 with "Unit 1" reproduces the legacy behaviour.
- **5c — `lastFaderValues`/`lastVPotValues` → vector + invalidation (R2):**
  The fixed `double[8][8][8]` arrays became flat `std::vector<double>`
  (size 512, `[bank][page][channel]` via `paramCacheIndex()`). A new
  `m_paramCacheValid` flag + `invalidateParamCache()` / `refillParamCache()`
  pair guards against stale-map false changes:
  - `followChanges()` early-returns after a refill when the cache is invalid,
    so the first scan after a map load does not count as a change.
  - `invalidateParamCache()` is called after **every** `m_pAccess->accessPlugin(...)`
    call site in PlugMode.cpp (3 sites) and the cache starts invalid in the
    ctor, so the very first scan refills instead of seeding from zeros.
- Build: clean, zero warnings on Linux. Deployed.
- Bugs B1/B2/B3 still open (need 2-unit hardware); not touched here.

## Key files modified

- `src/modes/plugin/PlugMode.h` — m_activeUnit, anchorUnit(), per-unit selector array
- `src/modes/plugin/PlugMode.cpp` — all Phase 1-3 methods
- `src/modes/plugin/PlugAccess.h` — per-unit state arrays, explicit overloads, used-page helpers
- `src/modes/plugin/PlugAccess.cpp` — per-unit persistence, default spread, explicit implementations
- `src/modes/plugin/PlugModeSelectors.h` — BankPagePlugSelector m_unit
- `src/modes/plugin/PlugModeSelectors.cpp` — per-unit state reads, m_unit construction
- `ai-docs/extender-wp-plugmode-impl-plan.md` — 3 corrections applied

## Phase 7 — audit notes (2026-07-20)

- **No code changes (R12).** The existing single `PlugModeMeterBridge`
  already handles N>1 correctly:
  - `updateMeterBridge` loops `x = 1..availableChannels()` (= numUnits()*8).
  - `ensureStripMeterState(availableChannels())` sizes `m_stripMeterPos`
    dynamically; no fixed `[8]` anywhere.
  - Each strip routes via `sendStripMeter(pos+1, meter)` (0xD0) to the
    **owning unit** (WP-EF Step 6). Master meters via `updateMasterLEDs`
    (global, 0xD1).
  - No `CONFIG_FLAG_PROX` / `IsFlagSet` usage (confirmed in 6c).
- `alsoOnDisplay()` returns `true` — just a flag, not a retargeting; left
  as-is per the plan.
- Verification gap: N>1 meter routing is structural; needs real multi-unit
  hardware to confirm each unit's 8 VU meters show its channels.

## Phase 8 — integration notes (2026-07-20)

- **8a activate/deactivate:** `activate()` → trackChanged + switchDisplay (per-unit
  render) + syncKnownStates; `deactivate()` → closeAll() windows + mixer toggle
  (all global). No per-unit work needed on deactivate (windows are global).
- **8b global events:** `trackListChange`/`trackSelected` → `updateEverything()`,
  which since Phase 3 loops all units' selector displays + switchDisplay. Correct.
- **8c global toggles:** `buttonGView` (followTrack), `buttonNameValue`,
  `buttonFlip` (bypass/drywet/delta) all affect the shared plugin / shared flag
  and re-render via switchDisplay/updateEverything → per-unit render. Verified.
- **8d editor = m_activeUnit (R8):** Added documenting comment to
  `createEditorComponent()`. PlugAccess' active-unit alias layer already reads
  the last-pinned unit's state; no structural editor change.
- **Audit grep (all plugin/ files):** no `m_is_mcuex`/`GetOffset`/`g_mcu_list`/
  `m_midiout->Send`/`IsFlagSet(CONFIG_FLAG_PROX)` left. Only local-channel
  `ElementDesc` assert `< 9` (local 0-7, correct) and per-unit-child 8-channel
  render loops in the ProX touched branches (correct).
- Build: clean, zero warnings on Linux. Deployed.
- **8e/8f (manual N=1 regression + N=3 matrix):** CANNOT be completed without
  running REAPER + multi-unit hardware. Code-level review done; functional
  testing deferred to maintainer with hardware. This is the only open item.
