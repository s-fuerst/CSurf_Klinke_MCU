# WP-PlugMode — Multi-Unit PlugMode (FX Mode) Widening

> Implementation plan for the Plugin/FX mode multi-unit support.
> Master plan: `ai-docs/extender-support.md` (§7, item 4).
>
> **Date:** 2026-07-13 (revised after an independent critical review — see
> `## Provenance` at the end)
> **Status:** PLANNING — self-contained design rationale (code facts, chosen
> approach, and rejected alternatives are inlined below). No code changes yet.

## What this work package delivers

**Core deliverable:** PlugMode works across multiple hardware units. Each
unit shows a different Bank/Page from the 8×8 PlugMap grid, with independent
per-unit cursors and per-unit display sets.

**User-visible behaviour (N=3 units):**
- Unit 0 shows Bank 0 / Page 0 (default), Unit 1 shows Bank 0 / Page 1,
  Unit 2 shows Bank 0 / Page 2 — page-spreading within the same bank, mapped
  onto the **used-page sequence** of the bank (sparse maps are honoured).
- Each unit independently scrolls its own Bank (SOLO strip) and Page (MUTE
  strip).
- Control+SOLO or Control+MUTE on unit K triggers cascade: units K..N-1 are
  reassigned sequential positions in the used-page sequence starting from the
  selected one; units 0..K-1 keep their state.
- Transport Bank/Channel UP-DOWN operate in **lock-step page-spread**: all
  units share one Bank and display a contiguous window of width N over the
  **used-page sequence**. Bank UP/DOWN moves every unit to the next/previous
  *used* bank and resets the window to positions `[0 … N−1]`; Page UP/DOWN
  shifts the window by `+N` positions. Clamp at the sequence/bank boundary,
  no wrap.
- `followChanges` follows the unit chosen by the `PMO2_FOLLOW_CHANGE` option
  (`OFF | Unit 0 … Unit N−1`); `OFF` disables it.
- Name/Value (touched parameter detail) shows per-unit on that unit's display.
- MeterBridge renders per-unit on that unit's hardware VU meters (already
  multi-channel via the existing bridge — no per-unit bridge instances).
- All units share the same plugin (no per-unit plugin selection).

**What it does NOT deliver:**
- Per-unit plugin selection (explicitly out of scope — overcomplicates
  MCU/GUI-follow options and chain monitoring).
- Changes to the PlugMap 8×8×8 structure.
- Changes to the JUCE editor (the on-screen GUI keeps reflecting the active
  unit; full per-unit editor support is a later task).
- `CONFIG_FLAG_PROX` removal outside PlugMode (routing layer, other modes).
  PlugMode only stops *using* the flag; the define itself is owned by WP-EF.
- On-LCD meter rendering (`alsoOnDisplay()` is currently unused — see R12).

## Design decisions at a glance

| # | Decision | Choice | Rationale |
|---|---|---|---|
| D1 | Per-unit parameter resolution | Explicit Bank/Page overloads in `PlugAccess` | R1 |
| D2 | `followChanges` target | Multi-valued option `OFF \| Unit 0 … Unit N−1`; invalidate param cache on map change | R2 |
| D3 | Transport UP/DOWN | Lock-step page-spread over the used-page sequence (no per-unit identity) | R3, R11 |
| D4 | `vpotPressed` | Per-unit dispatch to the owning unit's selector | R4 |
| D5 | Display writes | Child-targeted; **anchor policy**; clear non-anchor children on global messages | R5 |
| D6 | `switchDisplay` | Per-unit switch decision | R6 |
| D7 | ProX layout | Per-unit `HardwareUnit::isProX()` | R7 |
| D8 | Selectors | N `BankPagePlugSelector` instances + a single pinned `m_activeUnit` | R8 |
| D9 | Persistence | Versioned `<UNIT_STATES>` block + legacy reader, copyable array value types | R9 |
| D10 | Extender input | `PlugMode` opts in via `supportsExtendedChannels()` | R10 |
| D11 | Page math | Operate on the used-page sequence, never raw page indices | R11 |
| D12 | MeterBridge | Keep the single existing bridge (already multi-channel); no per-unit instances | R12 |

## Design Rationale & Rejected Alternatives

> Each item states the **code fact** that constrains the design, the **chosen
> approach**, and the **alternatives that were rejected** and why.

### R1 — Parameter resolution is the spine (D1)

**Code fact.** `PlugAccess::ElementDesc` has two constructors
(`PlugAccess.h`):

```cpp
ElementDesc(int bank, int page, eType type, int channel);          // explicit
ElementDesc(PlugAccess *pPA, eType type, int channel) {
  m_bank = pPA->getSelectedBank();              // reads GLOBAL state
  m_page = pPA->getSelectedPageInSelectedBank();// reads GLOBAL state
}
```

Every convenience overload constructs the second form, so the **entire**
parameter API — `getParamNameShort/Long`, `getParamValueShort/Long/Int/Double`,
`getParamSteps`, `setParamValueInt/Double`, `getParamID`, `getPMParam` —
resolves against the single global selected Bank/Page. "Make Bank/Page
per-unit" therefore really means **make parameter resolution per-unit**, not
just per-unit state storage.

**Chosen.** Add Bank/Page-explicit overloads that construct the explicit
`ElementDesc(bank, page, type, channel)` ctor (which already exists —
`followChanges` uses it today). The existing selected-based overloads stay
and delegate to the explicit ones with the **active unit's** Bank/Page, so the
editor, the selectors, and the touched-detail view of the active unit compile
unchanged. No hidden transient state.

**Guard:** the selected-based `ElementDesc` ctor reads `getSelectedBank()` →
`m_selectedBankPerUnit[m_activeUnit]`. If `m_activeUnit` is stale, parameter
resolution silently targets the wrong unit. Add an
`ASSERT(m_activeUnit >= 0 && m_activeUnit < numUnits)` in the selected-based
ctor so a missing `m_activeUnit` pin crashes deterministically instead of
producing silent wrong behaviour.

**Rejected.**
- *Push/pop the global selected Bank/Page around each per-unit render (RAII).*
  Smallest diff, but introduces transient global state and reentrancy risk.
- *Thread `int unit` through the entire param API + `ElementDesc`.* Most
  explicit, but the largest diff — touches every call site including the
  editor and selectors.

### R2 — `followChanges` follows a chosen unit; cache invalidation (D2)

**Code fact.** `PlugMode::followChanges()` (`PlugMode.cpp`) scans the whole
8×8×8 grid via the **explicit** `ElementDesc(bank, page, …)` form, and on
**exactly one** changed value does:

```cpp
m_pAccess->setSelectedBank(changeInBank);          // GLOBAL
m_pAccess->setSelectedPage(changeInBank, changeInPage);
```

After this WP there is no single global cursor, so this is incoherent for
N>1. Also, `lastFaderValues`/`lastVPotValues` (`double[8][8][8]`, `PlugMode.h`)
are **never initialised** in the ctor (first scan vs garbage), and
`accessPlugin` loads a new map without invalidating them (cache becomes stale
on every plugin/map change).

**Chosen.**
- `PMO2_FOLLOW_CHANGE` becomes multi-valued `OFF | Unit 0 … Unit N−1`. The
  grid scan is unchanged; in the `numChangedValues == 1` branch the chosen
  unit's cursor moves to `(changeInBank, changeInPage)`. `OFF` / out-of-range
  does nothing.
- Switch the arrays to `std::vector` and **invalidate (refill from current
  params) on the first scan AND on every plugin/map change** (in
  `accessPlugin` after `loadMapForPlug`), not only at construction.
- The option works through the **existing cyclic VPOT selection** — no
  `Options.cpp` change (see the option-mechanism note at the end of this section).

**Rejected.**
- *Disable for N>1 (no-op).* Drops an opt-in feature for every multi-unit user.
- *Only the unit(s) currently showing that Bank/Page follow.* Disproportionate
  attribution logic, rarely useful.
- *Keep a single global follow cursor (unit 0).* Incoherent with the per-unit
  model.

> **Option mechanism (correction to an earlier draft).** `Options`
> (`Options.cpp`) displays **4 options × their currently-selected value** in 4
> text columns (`displaySelectedOptions`: `changeText(1, i*14, …)` for `i<4`);
> the 8 V-Pots change option values via **cyclic** selection
> (`select(index)`: `numOpt=index/2`, `index%2 ? -- : ++`, wraps at attribute
> count). There is **no 8-field attribute limit** — `OFF | Unit 1 … Unit 8`
> (9 attributes) is reachable today. Therefore: **do not modify `Options.cpp`**;
> keep the new attributes local to `PlugMode2ndOptions`.

### R3 — Transport UP/DOWN are lock-step page-spread (D3)

**Code fact.** `CCSManager::buttonFaderBanks(int button, bool pressed)` and
`PlugMode::buttonFaderBanks(int button, bool pressed)` carry **no channel and
no unit** — these are transport buttons on the main unit. There is no
per-strip identity to read, so "BANK_UP on unit K" is both unimplementable and
conceptually wrong.

**Chosen.** Transport UP/DOWN operate on a **shared window**, not a unit
identity. All units share one Bank and display a contiguous window of width N
over that bank's **used-page sequence** (R11):
- **Bank UP/DOWN:** reference Bank = the anchor unit's Bank (R5); find the
  next/previous **used** bank; set **all** units to that Bank and reset the
  page window to sequence positions `[0 … N−1]`.
- **Page UP/DOWN:** shift the window start by `±N` sequence positions; each
  unit `u` shows `pageAtUsedOffset(bank, windowStart + u)`. Clamp, no wrap.

Per-unit SOLO/MUTE strip buttons stay independent; transport deliberately
collapses per-unit divergence back to the shared window.

**Rejected.**
- *Per-unit identity for these buttons.* Impossible (no unit in the signature).
- *Scroll only the main unit.* Leaves extenders unergonomic.
- *Lock-step over raw page indices.* Selects unused pages on sparse maps (R11).

### R4 — `vpotPressed` must dispatch per-unit (D4)

**Code fact.** `PlugMode::vpotPressed(channel, pressed)` routes a VPOT press
through the **global** selector state:

```cpp
switch (m_pBankPagePlugSelector->getWhatToSelect()) {
  case BANK:  buttonSolo(channel, true); break;
  case PAGE:  buttonMute(channel, true); break;
  case PLUG:  buttonSelect(channel, true); break;
  ...
}
```

**Chosen.** Dispatch on the **owning unit's** selector (`getWhatToSelect()`
becomes per-instance once R8 lands) and call `buttonSolo`/`buttonMute`/
`buttonSelect` for that unit. Correctness fix, not a choice.

### R5 — Display writes are child-targeted + anchor policy + no stale content (D5)

**Code facts.**
- `createDisplay(int numRows)` already returns a `MultiDisplay` for N>1 (WP-A).
- `MultiDisplay::changeField(row, globalField, …)` already **routes by global
  field number** (`field 1..8 → unit 0`, `9..16 → unit 1`, …).
- `MultiDisplay::changeText` / `changeTextFullLine` / `clearLine` **broadcast
  to all children** — touched detail and selector output must write to a
  specific child via `MultiDisplay::children()[unit]`.
- `MultiDisplay::mainChild()` returns the main unit's child or **NULL** — and
  **zero-main-unit configs are valid** (the "at least one main unit"
  requirement was removed). So `mainChild()` cannot be used unchecked.

**Chosen.**
- Widen param-overview loops to `nStrips` (auto-routing via `changeField`);
  write touched-detail and selector output to `children()[unit]`.
- **Anchor policy:** define `int anchorUnit()` = a main-capable unit if one
  exists, else unit 0. Use it everywhere a single "global" unit is needed
  (global messages, the global `PlugSelector` display, the transport
  reference, the editor). Prefer `children()[anchorUnit()]` over
  `mainChild()` to avoid the NULL case.
- **No stale content:** when a global message ("no single track" / "no FX" /
  "no selected FX") is active, **clear or placeholder every non-anchor child**
  so extenders do not keep showing their previous PlugMode view.

**Rejected.**
- *Build new per-unit display creation/routing.* Already delivered by WP-A.
- *Write touched detail via the composite.* Broadcasts one unit's detail to all.
- *Show global messages only on `mainChild()`.* NULL-deref on zero-main configs
  and leaves stale content on other units.

### R6 — `switchDisplay` decides per unit (D6)

**Code fact.** `PlugMode::switchDisplay()` chooses a single `Display*` via a
priority chain and activates it on every unit. With per-unit selectors and
per-unit touched state, units can legitimately be in different display states.

**Chosen.** Make a **per-unit** decision: global-message conditions gate all
units uniformly (and clear non-anchor children per R5); otherwise each unit
branches on its own selector/touch state and switches its own child on its own
handler.

**Rejected.** *Keep the single global pick.* Cannot represent per-unit
divergence.

### R7 — ProX layout via per-unit `isProX()` (D7)

**Code facts.** Per-unit ProX already exists from WP-EF:
`HardwareUnit::isProX()` and `DisplayHandler::m_isProX`. `updateParamsDisplay`
and `updateTouchedDisplay` currently branch on the **global**
`IsFlagSet(CONFIG_FLAG_PROX)`. There is **no** `Display::getNumRows()`.

**Chosen.** Gate the ProX layout on the owning unit's `isProX()` inside the
per-unit render loop. Mixed MCU-main + ProX-extender then gets the right row
count automatically. **Do not** synthesise a `getNumRows()`. Strip the flag
*usage* from PlugMode files; **do not delete the define** (WP-EF owns it).

**Rejected.** *Add `Display::getNumRows()`.* Unnecessary; per-unit `isProX()`
already exists.

### R8 — Per-unit selectors + a pinned `m_activeUnit` (D8)

**Code facts.** `PlugModeSelector(DisplayHandler*, PlugMode*)` and the
`Selector` base own a single `Display`/`DisplayHandler`. The selector bodies
read the **global** `getSelectedBank()`. `BankPagePlugSelector::select()` is a
**no-op stub** (`return true;`) — Bank/Page selection happens directly in
`buttonSolo`/`buttonMute`, so selector work is display-side only. Also, the
editor callbacks (`selectedBankChanged/selectedPageChanged/selectedChannelChanged`)
carry no unit.

**Chosen.**
- `PlugMode` owns `BankPagePlugSelector* m_pBankPagePlugSelectorPerUnit[N]`,
  each constructed with its unit's `DisplayHandler` and reading that unit's
  Bank/Page; `getWhatToSelect()` is per-instance. `PlugSelector` (plugin
  selection) stays **global** and renders on the anchor unit.
- **Pin one explicit `int m_activeUnit`** in `PlugMode`. Set it **before** any
  unit-specific callback: strip handlers set it from `(channel-1)/8`; the
  transport/selector/editor paths use `anchorUnit()`. Document its update
  rules. The selected-based compatibility API resolves against `m_activeUnit`.
  The editor reflects `m_activeUnit`.

**Rejected.** *One instance retargeted via an `m_activeUnit` field on the
selector.* Mutating shared selector state per interaction is fragile.

### R9 — Persistence with a versioned block + legacy reader (D9)

**Code fact.** `tSlotState` is today
`boost::tuple<String /*plugName*/, int /*bank*/, boost::array<int,8> /*pages*/>`.
`accessPlugin` resets Bank=0 / all Pages=0, then overwrites from
`m_knownSlotStates` if the plug name matches. Native arrays are **not**
copyable/assignable and cannot be `boost::tuple` elements. (`MAX_SURFACE_UNITS`
**is** already defined as `#define MAX_SURFACE_UNITS 8` in `SurfaceConfig.h` —
include it, or hoist the constant to a shared header if cleaner.)

**Chosen.** `tSlotState` →
`(String plugName, boost::array<int, MAX_SURFACE_UNITS> banksPerUnit,
  boost::array<boost::array<int,8>, MAX_SURFACE_UNITS> pagesPerUnit)` — all
copyable value types. `writeSlotStatesToProjectConfig` emits a **versioned**
`<UNIT_STATES>` block; the reader loads it if present, **else** maps legacy
single-bank + 8-pages data into unit 0 and leaves units 1..N−1 to the default
page-spread. The reader **bounds-checks** units, pages, and child-node counts
(the legacy reader today assumes the expected number of `PAGE` elements).

**Rejected.** *Mutate the old format in place.* Ambiguous to read.

### R10 — Opt into extender channels (D10)

**Code fact.** `CCSMode::supportsExtendedChannels()` defaults to `false`
(`CCSMode.h:108`). `CCSManager` drops every channel-specific event with
`if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;`
at ~13 dispatch sites (`buttonRec/Select/Mute/Solo`, `fader`, `vpot*`,
`faderTouched`, … — `CCSManager.cpp`). `MultiTrackMode` overrides it to
`true`; **PlugMode does not** — so today no extender event (channel > 8)
reaches PlugMode at all.

**Chosen.** Add
`bool supportsExtendedChannels() const override { return true; }` to `PlugMode`.
This is a hard prerequisite for Phase 2 (per-unit input) and lands in Phase 0.
Add regression coverage for every extender input path.

**Rejected.** None — it is a required capability flag, not a design choice.

### R11 — Page math uses the used-page sequence, never raw indices (D11)

**Code fact / logic.** A bank's used pages can be sparse (e.g. `{0, 2, 5}`).
Raw-index logic like `clamp(u, lastUsedPage)` assigns unit 1 → page 1, which is
**unused**. The same error would infect default spreading, transport page
moves, and Control cascade.

**Chosen.** Introduce one shared helper pair that operates on the ordered list
of used pages:
```cpp
std::vector<int> usedPages(int bank);                 // ascending used pages
int usedPageCount(int bank);
int pageAtUsedOffset(int bank, int offset);           // offset into the sequence, clamped
```
- Default spreading: unit `u` → `pageAtUsedOffset(bank0, u)`.
- Transport page window: window **offset** (into the used sequence) shifts by
  `±N`; unit `u` → `pageAtUsedOffset(bank, windowOffset + u)`.
- Cascade: `pageAtUsedOffset(bank, baseOffset + (u - unit))`.

The page window is defined over **positions in the used-page sequence**, the
only useful interpretation for sparse maps.

**Rejected.** *Raw-index clamp.* Selects unused pages.

### R12 — MeterBridge is already multi-channel (D12)

**Code fact.** `PlugModeMeterBridge::updateMeterBridge` already loops
`for (int x = 1; x <= pMCU->availableChannels(); x++) updateMeter(x, …)` and
the routing layer sends each meter to its owning hardware unit.
`alsoOnDisplay()` returns `true` but is **consumed nowhere** — there is no
on-LCD meter rendering; meters are `0xD0` hardware VU only.

**Chosen.** **Do not** create per-unit MeterBridge instances (that would
duplicate every meter message once per unit) and do not retarget a
non-existent LCD render. Keep the single existing bridge; it already handles
N>1 via `availableChannels()`. Phase 7 is reduced to "verify the existing
bridge meters correctly for N>1".

**Rejected.** *Per-unit MeterBridge instances.* Duplicates meter traffic.

## Current State Inventory

### PlugMode data — what must become per-unit

| Current field | Type | Becomes |
|---|---|---|
| `m_selectedBank` (in PlugAccess) | `int` | `boost::array<int, MAX_SURFACE_UNITS> m_selectedBankPerUnit` |
| `m_selectedPage` (in PlugAccess) | `boost::array<int, 8>` (page per bank) | `boost::array<boost::array<int,8>, MAX_SURFACE_UNITS> m_selectedPagePerUnit` |
| `m_pParamsDisplay` | `Display*` | already `MultiDisplay` for N>1 via `createDisplay()` (WP-A) |
| `m_pTouchedDisplay` | `Display*` | ditto |
| `m_pValueDisplay` | `Display*` | ditto |
| `m_pSingleTrackMessage` / `m_pNoPlugMessage` / `m_pNoPlugSelectedMessage` | `Display*` | stay single (global message) → render on `children()[anchorUnit()]`; **clear non-anchor children** when active |
| `m_iSingleFaderTouched` / `m_iSingleVPotTouched` | `int` | stay global (one touch at a time); determine which unit shows detail |
| `m_buttonNameValuePressed` | `bool` | stays global |
| `m_pPlugSelector` | `PlugSelector*` | stays single (global plugin selection); render on anchor unit |
| `m_pBankPagePlugSelector` | `BankPagePlugSelector*` | per-unit: `BankPagePlugSelector* m_pBankPagePlugSelectorPerUnit[MAX_SURFACE_UNITS]` |
| `m_pMeterBridge` | `PlugModeMeterBridge*` | **unchanged** — already multi-channel (R12); no per-unit instances |
| `m_activeUnit` | — (new) | `int m_activeUnit` in `PlugMode`, pinned before unit-specific callbacks (R8) |
| `lastFaderValues` / `lastVPotValues` | `double[8][8][8]` | `std::vector` 3-D, resized `[8][8][8]`, **initialised + invalidated on first scan and every plugin/map change** (R2) |

### Code facts that constrain the design

- **Extender input is gated by `supportsExtendedChannels()`** — PlugMode does
  not override it, so channel > 8 events are dropped today (R10).
- **`ElementDesc` is the spine, not the storage** — R1.
- **`MultiDisplay::changeField` routes by global field number;**
  `changeText`/`changeTextFullLine`/`clearLine` broadcast; `mainChild()` may be
  NULL — R5.
- **Per-unit ProX already exists** via `HardwareUnit::isProX()` /
  `DisplayHandler::m_isProX`; there is **no** `Display::getNumRows()` — R7.
- **`BankPagePlugSelector::select()` is a no-op stub** — selector work is
  display-side only — R8.
- **`vpotPressed` routes via the global selector → per-unit after Phase 3 — R4.
- **`buttonFaderBanks` has no unit** (transport button) — R3.
- **Selectors own one `DisplayHandler`/`Display`** — per-unit needs N
  instances — R8.
- **`PlugModeMeterBridge` already iterates `availableChannels()`** and
  `alsoOnDisplay()` is unused — R12.
- **`Options` shows 4 options × value with cyclic VPOT selection** — no
  attribute-count limit, no `Options.cpp` change — R2.
- **`MAX_SURFACE_UNITS` is defined** (`SurfaceConfig.h:19`) — use it, do not
  redefine.

### Channel → Unit mapping (already available)

- Unit index = `(channel - 1) / 8`, local channel = `(channel - 1) % 8`.
  `CSurf_MCU::numUnits()`, `unitForChannel()`, `availableChannels()`
  (`csurf_mcu.h`). `Tracks::getNumberOfChannelStrips()` = `numUnits()*8`.
- WP-F already widened MultiTrack/Command/Send modes along this pattern.

## Implementation Phases

> Sequenced around the R1 dependency: parameter resolution must exist before
> any per-unit render path can be correct. Phase 0 is the foundation and also
> contains the two hard prerequisites (R10 input opt-in, R8 activeUnit pin).
> `N=1` must stay functionally equivalent after every phase.

### Phase 0 — Per-unit state model, explicit Bank/Page overloads, persistence, input opt-in

**Goal:** `PlugAccess` stores per-unit Bank/Page and exposes the explicit
Bank/Page param overloads (R1). `PlugMode` opts into extender input (R10) and
pins `m_activeUnit` (R8). State persists with a backward-compatible reader.
No caller is migrated yet — N=1 unchanged.

**Depends on:** nothing (foundation).

**Steps:**

0a. **Opt into extender input (R10).** Add to `PlugMode`:
   `bool supportsExtendedChannels() const override { return true; }`. Without
   this, Phase 2 receives no channel > 8 events.

0b. **Pin `m_activeUnit` (R8).** Add `int m_activeUnit` to `PlugMode` (default
   `anchorUnit()`). Document update rules: strip handlers set it from
   `(channel-1)/8` before doing anything; transport/selector/editor paths use
   `anchorUnit()`.

0c. **Per-unit state in `PlugAccess`** (copyable value types, R9):
   ```cpp
   boost::array<int, MAX_SURFACE_UNITS> m_selectedBankPerUnit;        // −1 = unassigned
   boost::array<boost::array<int,8>, MAX_SURFACE_UNITS> m_selectedPagePerUnit;
   ```
   Keep the existing names as **active-unit aliases** so editor/selectors/
   selected-based `ElementDesc` compile unchanged:
   ```cpp
   int  getSelectedBank()               { return m_selectedBankPerUnit[m_activeUnit]; }
   int  getSelectedPageInSelectedBank() { return m_selectedPagePerUnit[m_activeUnit][m_selectedBankPerUnit[m_activeUnit]]; }
   ...
   ```
   (PlugAccess needs the active unit from PlugMode; thread it or store it.)
   Add per-unit accessors `selectedBankForUnit(u)`, `selectedPageForUnit(u)`,
   `setSelectedBank(bank, u)`, `setSelectedPage(bank, page, u)`,
   `setSelectedPageInSelectedBank(page, u)`.

0d. **Explicit Bank/Page param overloads (R1 — the spine).** Add the
   explicit-bank/page overloads (getParamNameShort/Long, getParamValue*,
   getParamSteps, setParamValue*, getParamID) implemented via
   `ElementDesc(bank, page, type, channel)`. Refactor existing selected-based
   overloads to delegate to them with the active unit's Bank/Page.

0e. **Used-page helpers (R11).** Add `usedPages(bank)`, `usedPageCount(bank)`,
   `pageAtUsedOffset(bank, offset)` to `PlugAccess` (they wrap
   `PlugMap::getBank(bank)->getPage(i)->isUsed()`).

0f. **Default page-spread on `accessPlugin()` (over the used-page sequence).**
   After reset + stored-state restore, for each unit `u` in `0..N−1` with no
   stored per-unit state:
   `m_selectedBankPerUnit[u] = 0;`
   `m_selectedPagePerUnit[u][0] = pageAtUsedOffset(0, u);`
   Stored state wins.

0g. **Persistence (R9).** `tSlotState` →
   `(String, boost::array<int,MAX_SURFACE_UNITS>, boost::array<boost::array<int,8>,MAX_SURFACE_UNITS>)`.
   Versioned `<UNIT_STATES>` writer; legacy reader maps old single-bank +
   8-pages into unit 0, defaults units 1..N−1. **Bounds-check** units, pages,
   child-node counts.

**Verification:**
- N=1: build clean; plugin switch restores Bank/Page exactly as before.
- N=3: plugin switch → units 0/1/2 land on used-page offsets 0/1/2 of Bank 0.
- N=1 extender regression: with a 2-unit surface, channel-9 fader/VPOT/SOLO/
  MUTE/SELECT now **reach** PlugMode (previously dropped).
- Reaper save/load: per-unit state round-trips.
- Legacy `.rpp` (no `<UNIT_STATES>`): loads; unit 0 keeps its old Bank/Page.
- Malformed/short `<PAGE>` lists: handled without overread.

**Files:** `PlugMode.h` (+ supportsExtendedChannels, m_activeUnit, anchorUnit),
`PlugAccess.h`, `PlugAccess.cpp`.

---

### Phase 1 — Per-unit child-targeted display + per-unit switchDisplay

**Goal:** Displays render per-unit. Touched detail and selector output write
to a specific child; global messages use the anchor child and clear the
others. `switchDisplay` decides per unit.

**Depends on:** Phase 0.

**Steps:**

1a. **Display accessors + anchor helper in `PlugMode`.**
   ```cpp
   MultiDisplay* asMulti(Display* d);
   Display* childForUnit(Display* d, int u);      // children()[u] or d (N=1)
   int anchorUnit();                              // main unit, else 0 (R5)
   Display* anchorChild(Display* d);              // childForUnit(d, anchorUnit())
   ```

1b. **`updateParamsDisplay()` per-unit.** Loop `nStrips = numUnits()*8`;
   `unit=iChannel/8`, `localCh=iChannel%8`, bank/page = `selectedBank/PageForUnit(unit)`;
   `changeField` auto-routes by global field; values via explicit overloads.
   ProX 4-row chosen per-unit by `unitForChannel(globalCh)->isProX()` (full in
   Phase 6).

1c. **`updateTouchedDisplay()` per-unit (child-targeted).** Touched unit
   `u = (touched>0) ? (touched-1)/8 : 0`; write via
   `childForUnit(m_pTouchedDisplay, u)->changeText(...)` (NOT the composite);
   use that unit's bank/page + local channel + explicit overloads. Non-touched
   units keep their param content.

1d. **`switchDisplay()` per-unit + global-message clearing (R5/R6).** Per-unit
   decision; when a global message is active, switch the **anchor** child to
   the message and **clear/placeholder every non-anchor child** (no stale
   PlugMode view on extenders). Otherwise switch each unit's own child on its
   own handler by that unit's selector/touch state.

1e. **N=1 path:** `asMulti` NULL; `childForUnit` returns the plain display;
   behaviour byte-for-byte identical to today.

**Verification:**
- N=1: display output identical.
- N=3: each unit's LCD shows a different page's params.
- Touch a fader on unit 2 → unit 2 LCD shows detail; units 0/1 keep params.
- "No single track": anchor shows the message; units 1/2 are cleared (not
  stale). Also test the zero-main-unit config (anchor = unit 0, no NULL deref).

**Files:** `PlugMode.cpp`, `PlugMode.h`.

---

### Phase 2 — Per-unit strip buttons, LED/Fader/VPOT updates

**Goal:** SOLO/MUTE strip buttons, LED updates, and fader/VPOT control operate
on the owning unit. `vpotPressed` is deferred to Phase 3 (needs the per-unit
selector array). (Events now reach PlugMode thanks to Phase 0a.)

**Depends on:** Phase 0, Phase 1.

**Steps:**

2a. **`buttonSolo` per-unit (Bank).** `channel → m_activeUnit=(ch-1)/8,
   localCh=(ch-1)%8`; `setSelectedBank(localCh, m_activeUnit)`. Control →
   cascade (Phase 4). Editor callback fires for `m_activeUnit`.

2b. **`buttonMute` per-unit (Page).** Same derivation;
   `setSelectedPageInSelectedBank(localCh, m_activeUnit)`. **Shift+Page** is
   limited to the triggering unit (set the page for every bank of *that* unit).

2c. **`updateSoloLEDs` / `updateMuteLEDs` per-unit.** Loop `nStrips`; LED state
   from that unit's bank/page; `setSoloLED`/`setMuteLED(this, globalCh, state)`.

2d. **`updateFaders` / `updateVPOTs` per-unit.** Loop `nStrips`; bank/page =
   `selectedBank/PageForUnit(unit)`; explicit overloads; `setFader`/`getVPOT(
   this, globalCh, …)`. Master fader (channel 0 = dry/wet) → anchor unit only.

2e. **`fader` / `vpotMoved` per-unit.** `channel → unit+localCh`; resolve on
   that unit's bank/page via explicit overloads.

**Verification:**
- N=1: SOLO/MUTE LED + fader/VPOT identical.
- N=3: SOLO 3 on unit 1 → unit 1 Bank changes; unit 1 ch-3 SOLO LED blinks;
  unit 0/2 unchanged. Fader on unit 2 ch 3 → unit 2's bank/page param changes.

**Files:** `PlugMode.cpp`, `PlugMode.h`, `PlugAccess.h/.cpp`.

---

### Phase 3 — Per-unit selectors + `vpotPressed`

**Goal:** `BankPagePlugSelector` exists per unit; activation routing and
display target the owning unit. `vpotPressed` dispatches on the per-unit
selector (R4). `PlugSelector` stays global.

**Depends on:** Phase 1 (displays per-unit), Phase 2 (button handlers that
`vpotPressed` dispatches to).

**Steps:**

3a. **N selector instances (R8).** Replace the single pointer with
   `BankPagePlugSelector* m_pBankPagePlugSelectorPerUnit[MAX_SURFACE_UNITS]`,
   each constructed with its unit's `DisplayHandler`
   (`getMCU()->unitForChannel(u*8+1)` → unit → handler) and owning unit `u`.

3b. **Selector bodies read per-unit state.** `writePlugBankPageTopLine` /
   `updateDisplay` read `selectedBankForUnit(m_unit)` /
   `selectedPageForUnit(m_unit)`; `getWhatToSelect()` is per-instance.

3c. **Activation routing** in `buttonSolo`/`buttonMute`/`switchDisplay`
   uses `m_pBankPagePlugSelectorPerUnit[unit]`.

3d. **`select()` stays a no-op stub** — Bank/Page selection is in the button
   handlers.

3e. **`vpotPressed` per-unit (R4).** `channel → m_activeUnit`; dispatch on
   `m_pBankPagePlugSelectorPerUnit[m_activeUnit]->getWhatToSelect()` to
   `buttonSolo`/`buttonMute`/`buttonSelect` for that unit. (Formerly Phase 2c;
   moved here because the selector array must be finalized first.)

3f. **`PlugSelector` (PLUG) stays global**; renders on the anchor unit.

**Verification:**
- N=3: SOLO 3 on unit 2 → BANK selector on unit 2's LCD; SOLO 5 on unit 0 →
  selector on unit 0's LCD. Selector content reflects the owning unit's
  bank/page names.
- N=3: VPOT-press on unit 1 with PAGE selector active → page selection on
  unit 1 only.

**Files:** `PlugModeSelectors.h`, `PlugModeSelectors.cpp`, `PlugMode.h`,
`PlugMode.cpp`.

---

### Phase 4 — Transport lock-step page-spread + Control+cascade

**Goal:** Bank/Channel UP-DOWN implement D3 over the used-page sequence.
Control+cascade works from any unit.

**Depends on:** Phase 0, Phase 2.

**Steps:**

4a. **`buttonFaderBanks` lock-step (R3, R11).** No new parameters.
   - **B_BANK_UP/DOWN:** `refBank = selectedBankForUnit(anchorUnit())`; find
     next/previous **used** bank `B'`; for every unit `u`:
     `setSelectedBank(B', u); setSelectedPage(B', pageAtUsedOffset(B', u), u)`
     → window reset to sequence positions `[0 … N−1]`.
   - **B_CHANNEL_UP/DOWN:** `windowStart` (sequence offset) shifts by `±N`;
     for every unit `u`:
     `setSelectedPage(selectedBankForUnit(u), pageAtUsedOffset(bank, windowStart+u), u)`.
     Clamp to the sequence, no wrap.
   - Selector activation on B_BANK/B_CHANNEL fires on the anchor unit's
     selector.

4b. **Control+cascade.** `cascadeFromUnit(unit, bank, baseOffset)`:
   for `u = unit..N−1` `setSelectedBank(bank, u)` and
   `setSelectedPage(bank, pageAtUsedOffset(bank, baseOffset + (u-unit)), u)`.
   Units `0..unit-1` unchanged.

4c. Document: transport lock-step deliberately collapses per-unit Bank
   divergence back to the shared window.

**Verification:**
- N=3 with a sparse bank (used pages {0,2,5,7}): Bank-UP → all units same
  next used bank, units show pages at offsets 0/1/2 of the sequence
  (i.e. pages {0,2,5}); Page-UP → offsets 3/4/5 (pages {7,7,7} clamped);
  Control+SOLO cascades along the sequence.

**Files:** `PlugMode.cpp`, `PlugMode.h`, `PlugAccess`.

---

### Phase 5 — `followChanges` unit option + dynamic `lastFaderValues`

**Goal:** `followChanges` follows the chosen unit (D2); the param cache is a
vector, initialised and invalidated correctly.

**Depends on:** Phase 0.

**Steps:**

5a. **`PMO2_FOLLOW_CHANGE` multi-valued (R2).** In `PlugMode2ndOptions` ctor
   add `PMO2A_OFF` (default) and `"Unit 1" … "Unit 8"`. The existing cyclic
   VPOT selection reaches all values — **no `Options.cpp` change**. Helper:
   `int followChangeUnit()` → the unit whose attribute is selected and
   `< numUnits()`, else −1.

5b. **`followChanges` action.** Scan unchanged (explicit ElementDesc). In the
   `numChangedValues == 1` branch, if `fu = followChangeUnit() >= 0`:
   `setSelectedBank(changeInBank, fu); setSelectedPage(changeInBank, changeInPage, fu);`
   else no action.

5c. **`lastFaderValues`/`lastVPotValues` → vector + invalidation (R2).** 3-D
   vector resized `[8][8][8]`; **refill from current params on first scan AND
   in `accessPlugin` after `loadMapForPlug`** (every plugin/map change), not
   only at construction.

**Verification:**
- N=1: "Unit 1" reproduces today's behaviour.
- N=3: option "Unit 2" + one external change → unit 2 cursor jumps; others
  unchanged. `OFF` → nothing.
- Switch plugin → cache refilled (no spurious "change" from the old map's
  values); first-run-after-load no garbage.

**Files:** `PlugMode2ndOptions.h`, `PlugMode2ndOptions.cpp`, `PlugMode.h`,
`PlugMode.cpp`.

---

### Phase 6 — Stop using `CONFIG_FLAG_PROX` in PlugMode (per-unit `isProX()`)

**Goal:** No PlugMode file references `CONFIG_FLAG_PROX`. ProX layout decided
per-unit by the owning unit's `isProX()`.

**Depends on:** Phase 1.

**Steps:**

6a. **Replace the flag checks (R7).** `updateParamsDisplay` /
   `updateTouchedDisplay` `IsFlagSet(CONFIG_FLAG_PROX)` →
   `unitForChannel(globalCh)->isProX()` inside the per-unit render loop.

6b. **Inline `updateTouchedDisplayProX`** (chosen by the owning unit's
   `isProX()`); delete the separate method.

6c. **MeterBridge ProX.** Audit `PlugModeMeterBridge`; if it branches on the
   flag, switch to the owning unit's `isProX()`.

6d. **Strip the `#include`** of the flag from PlugMode files. **Do not delete
   the define** (WP-EF owns it).

**Verification:**
- N=1 (MCU 2-row / ProX 4-row): identical to today.
- N=3 mixed (MCU main + ProX extender): MCU units 2-row, ProX unit 4-row,
  automatically from `isProX()`.

**Files:** `PlugMode.cpp`, `PlugMode.h`, `PlugModeMeterBridge.cpp`.

---

### Phase 7 — Verify MeterBridge for N>1 (no per-unit instances)

**Goal:** Confirm the existing single bridge meters correctly for N>1.

**Depends on:** Phase 0.

**Steps:**

7a. **Verify, do not multiply (R12).** The existing `PlugModeMeterBridge`
   already loops `availableChannels()` and routes each meter to its owning
   unit. No per-unit instances, no `alsoOnDisplay` retargeting (it is unused).
   Only fix anything ProX-related found in 6c.

**Verification:**
- N=1: meter output identical.
- N=3: each unit's 8 hardware VU meters show its channels' levels.

**Files:** none expected (audit only; touch `PlugModeMeterBridge.cpp` only if
6c finds a flag usage).

---

### Phase 8 — Integration, edge cases, N=1 regression

**Goal:** Wire everything together, fix edge cases, verify N=1 regression.

**Steps:**

8a. **`updateEverything` / `activate` / `deactivate`.** All per-unit update
   paths called; `activate` syncs known states for all units; `deactivate`
   closes windows (global).

8b. **Global events.** `trackListChange` / `trackSelected` → full
   `updateEverything` (now per-unit render).

8c. **Global toggles stay global.** `buttonGView` (followTrack),
   `buttonNameValue`, `buttonFlip` (bypass/drywet/delta) — verify they affect
   the shared plugin and render appropriately on all units / anchor unit.

8d. **Editor = `m_activeUnit` (R8).** `PlugModeComponent` reflects
   `m_activeUnit`. Document; no structural editor work.

8e. **N=1 full regression** (build + deploy Linux): plugin select, bank/page
   scroll (strip + transport), fader/VPOT control, preset store/recall +
   favorites, followTrack, bypass/drywet/delta, display output, meter bridge,
   MCU-follow, GUI-follow, chain monitoring, floating-window limiting,
   followChanges. Compare **functionally** (not byte-for-byte) vs the pre-WP
   build.

8f. **N=3 manual matrix** incl. mixed MCU+ProX, a sparse-page map, and a
   zero-main-unit config.

**Verification:** Full manual N=1 test; N=3 spot-check matrix.

**Files:** `PlugMode.cpp`.

---

## Risks and Open Items

| Risk | Mitigation |
|---|---|
| Extender events dropped (no `supportsExtendedChannels`) | Phase 0a opts in; N=2 input regression in Phase 0 verification |
| `ElementDesc` selected-alias must stay consistent | Phase 0c keeps aliases; explicit overloads additive |
| `m_knownSlotStates` format change breaks old `.rpp` | Versioned `<UNIT_STATES>` + legacy reader → unit 0 (Phase 0g); bounds-checked |
| Raw page indices select unused pages | Used-page-sequence helpers everywhere (R11/Phase 0e/4) |
| `mainChild()` NULL on zero-main configs | Anchor policy `anchorUnit()` = main else 0 (R5/Phase 1a) |
| Global messages leave stale extender content | Clear/placeholder non-anchor children (Phase 1d) |
| `m_activeUnit` ambiguity | Pinned in Phase 0b; set before every unit-specific callback |
| Per-unit selectors referenced before Phase 3 | Resolved: `vpotPressed` moved to Phase 3 (after selector construction); Phase 2 now only uses selectors indirectly via button handlers |
| `lastFaderValues` stale after plugin/map change | Refill in `accessPlugin` after `loadMapForPlug` (Phase 5c) |
| Native arrays in `tSlotState` not copyable | `boost::array` value types (Phase 0c/0g) |
| `CONFIG_FLAG_PROX` define deletion owned by WP-EF | PlugMode only stops *using* it (Phase 6d) |
| N=1 byte-identical promise unrealistic | Verify functional equivalence, not byte identity (Phase 8e) |

## Files Summary

| File | Phase | Nature of change |
|---|---|---|
| `PlugMode.h` | 0, 1, 2, 4, 5 | `supportsExtendedChannels`, `m_activeUnit`, `anchorUnit()`, display accessors, per-unit selector array, cascade helper, vector cache types |
| `PlugMode.cpp` | 1, 2, 3, 4, 5, 6, 8 | Per-unit display/switchDisplay (+ non-anchor clearing), buttons, LEDs, faders, VPOTs (Phase 2), selectors + `vpotPressed` (Phase 3), transport lock-step, followChanges, ProX removal, integration |
| `PlugAccess.h` | 0, 2, 4 | Per-unit state (boost::array), active-unit aliases, explicit Bank/Page overloads, per-unit setters, used-page helpers |
| `PlugAccess.cpp` | 0, 4, 5 | Per-unit state, persistence (versioned + bounds-checked), page-spread default, `accessPlugin`/`storeActualSlotState`, cache invalidation on map change |
| `PlugModeSelectors.h/.cpp` | 3 | Per-unit instances, per-unit bank/page reads, `vpotPressed` dispatch (moved from Phase 2) |
| `PlugMode2ndOptions.h/.cpp` | 5 | `PMO2_FOLLOW_CHANGE` multi-valued (`OFF | Unit 0..N−1`); **no `Options.cpp` change** |
| `PlugModeMeterBridge.cpp` | 6 (only if ProX flag found) | Per-unit `isProX()` if needed; no per-unit instances |

**Estimated effort:** 4-5 days. The hardest per-mode WP — PlugMap indirection,
per-unit parameter resolution, display child-targeting, used-page-sequence
math, ProX merging, transport lock-step, and 8×8×8 cursor state.

## Verification Checklist (to be ticked during implementation)

- [ ] N=1: all existing PlugMode behaviour functionally unchanged
- [ ] N=2: extender input (fader/VPOT/SOLO/MUTE/SELECT/touch on ch 9-16) reaches PlugMode
- [ ] N=3: units show different used-page offsets on plugin select
- [ ] N=3: per-unit bank scroll (SOLO strip) and page scroll (MUTE strip)
- [ ] N=3: Control+cascade bank/page along the used-page sequence from any unit
- [ ] N=3: transport Bank UP/DOWN = lock-step, window resets to offsets [0..N−1]
- [ ] N=3: transport Page UP/DOWN = window shift ±N (sequence offsets), clamped, no wrap
- [ ] N=3 sparse-page map: spreading/window never select an unused page
- [ ] N=3: per-unit fader/VPOT control (owning unit's bank/page)
- [ ] N=3: per-unit display (params + touched detail on the touched unit only)
- [ ] N=3: meter bridge per unit via the single existing bridge (no duplication)
- [ ] N=3: plugin select/favorites global; bypass/drywet/delta global; presets global
- [ ] N=3: followTrack toggle global
- [ ] N=3: `followChanges` follows the chosen unit; `OFF` disables; option reachable via cyclic VPOT (no `Options.cpp` change)
- [ ] N=3: state persists to `.rpp` and restores (incl. legacy → unit 0, malformed bounds-checked)
- [ ] N=3 mixed MCU+ProX: ProX unit 4-row, MCU units 2-row (auto via `isProX()`)
- [ ] N=3 zero-main-unit config: anchor = unit 0, no NULL deref, no stale extender content
- [ ] No `CONFIG_FLAG_PROX` *usage* remains in PlugMode files (define stays)
- [ ] `lastFaderValues`/`lastVPotValues` initialised + invalidated on plugin/map change
- [ ] `m_activeUnit` set before every unit-specific callback

## Provenance

This plan was made self-contained on 2026-07-13 (the original critical review
was folded in and its file removed) and then revised after an **independent
critical review** (`extender-wp-plugmode-impl-plan_critical-review.md`,
2026-07-13). The review's findings were incorporated as follows:

| Review finding | Disposition |
|---|---|
| 1 `supportsExtendedChannels` missing | Accepted → R10 / D10 / Phase 0a (critical input prerequisite) |
| 2 MeterBridge already multi-channel | Accepted → R12 / D12 / Phase 7 reduced |
| 3 Sparse-page selection bug | Accepted → R11 / D11 / Phase 0e + 4 (used-page helpers) |
| 4 `mainChild()` NULL on zero-main config | Accepted → anchor policy in R5 / Phase 1a |
| 5 Native arrays not copyable | Accepted → `boost::array` in R9 / Phase 0c+0g |
| 5 `MAX_SURFACE_UNITS` "not defined" | **Rejected** — it IS defined (`SurfaceConfig.h:19`); the plan uses it |
| 6 `activeUnit()` underspecified | Accepted → pinned `m_activeUnit` in R8 / Phase 0b |
| 7 Options paging is unnecessary | Accepted → no `Options.cpp` change; cyclic selection in R2 / Phase 5a |
| 8 Stale extender content | Accepted → clear non-anchor children in R5 / Phase 1d |
| + invalidate cache on map change | Accepted → R2 / Phase 5c |
| + bounds-check persistence reader | Accepted → R9 / Phase 0g |
| + sparse-page tests | Accepted → Verification checklist |

The review's overall verdict ("architectural direction sound; explicit-overload
foundation is the right choice") is retained.



---
[Graphify] Doc/plan file read: extender-wp-plugmode-impl-plan.md. For architecture context, use:
graphify_query({ question: "What system concepts are connected to extender-wp-plugmode-impl-plan.md?", budget: 1200 })
