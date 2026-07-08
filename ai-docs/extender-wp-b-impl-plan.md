# WP-B — Complete Multi-Unit Configuration — Implementation Plan

> Implementation plan for work package WP-B of the extender-support effort.
> Master plan: `ai-docs/extender-support.md` (sections 5–6). WP-A reference:
> `ai-docs/extender-wp-a-impl-plan.md`.
>
> **This is a plan, not code.** No source changes have been made.
>
> **Revision 3 (2026-07-08):** rewritten. Key design decisions:
> - Full 8-unit dialog now, with fixed rows — no deferred dialog work package.
> - Device type is **per-unit** via a combo on each row. The selected type
>   encodes both protocol role (main/extender) and model (Mackie/QCon ProX).
>   The global `IDC_PROX` checkbox and `CONFIG_FLAG_PROX` flag become derived
>   compatibility artifacts (unit 0's model).
> - Config format: 8 fixed space-separated entries, no `units=` / `uN=` cruft.
> - Runtime constructs all configured units; input from unit 1+ is silently
>   dropped until WP-C + WP-F widen the channel bounds.

---

## Goal

Deliver a **complete, user-visible 8-unit configuration dialog** plus a
versioned, forward-compatible parser/serializer for the surface config string.
The dialog shows all 8 units, each with a device-type combo and MIDI
input/output combos. Units 2–8 default to MIDI None and `Mackie Extender`.
Unit 1 is always a main-capable unit (at least one main is required). The
device type selected in the combo is the user-facing source of truth; it is
converted to `UnitConfig::isMain` and `UnitConfig::model` before constructing
`HardwareUnit`.

At runtime, `CSurf_MCU` constructs a `HardwareUnit` for **every** unit that
has real (non-`-1`) MIDI devices configured — ports open, reset SysEx sent,
multi-unit MIDI-open sequence exercised from day one. MIDI input from unit 1+
is silently dropped (with a debug log) until later WPs widen the channel
bounds.

---

## Golden thread

WP-B is a **configuration milestone**, not the "extenders now work" milestone.

Three states coexist:

1. **Legacy config:** `"0 8 <midiIn> <midiOut> <flags>"` — parsed, never
   re-emitted. Becomes unit 1 populated, units 2–8 at defaults.
2. **Single-unit KLINKE2 config:** unit 1 configured, units 2–8 at MIDI None.
3. **Multi-unit KLINKE2 config:** real MIDI devices on N rows → all N units
   constructed at runtime, but only unit 1's input reaches the modes.

The dialog always shows 8 fixed rows. No compile-time gate. No developer-only
flags. The dialog is honest and complete from day one.

---

## Dialog layout

```
┌────────────────────────────────────────────────────┐
│ Mackie Control Protocol (Klinke)   [_] [□] [X]    │
├────────────────────────────────────────────────────┤
│                                                    │
│  Unit 1  [Mackie Main       ▼]  In [Device 1 ▼]  Out [Device 2 ▼] │
│  Unit 2  [Mackie Extender   ▼]  In [None     ▼]  Out [None     ▼] │
│  Unit 3  [Mackie Extender   ▼]  In [None     ▼]  Out [None     ▼] │
│  Unit 4  [Mackie Extender   ▼]  In [None     ▼]  Out [None     ▼] │
│  Unit 5  [Mackie Extender   ▼]  In [None     ▼]  Out [None     ▼] │
│  Unit 6  [Mackie Extender   ▼]  In [None     ▼]  Out [None     ▼] │
│  Unit 7  [Mackie Extender   ▼]  In [None     ▼]  Out [None     ▼] │
│  Unit 8  [Mackie Extender   ▼]  In [None     ▼]  Out [None     ▼] │
│                                                    │
│  ☐ Emulate blinking LEDs                           │
│  ☐ Use keyboard modifier (right Alt → OPTION)     │
│  ☐ Fake fader touch                                │
│  ☐ Swap left/right arrow on zoom                  │
│                                                    │
│                         [Open Manual]  [Donate]    │
└────────────────────────────────────────────────────┘
```

- **8 fixed rows**, always all visible — no scrolling, no add/remove buttons.
- Each row: `Unit N` label + **device type combo** + `In` / `Out` combos.
- **Unit 1 device type combo:** only the two **main** presets —
  `Mackie Main` and `QCon ProX`. At least one main unit is required;
  restricting Unit 1 to main-only enforces this without a separate warning.
- **Units 2–8 device type combo:** all four presets:
  `Mackie Main`, `Mackie Extender`, `QCon ProX`, `QCon ProX Extender`.
  Default = `Mackie Extender`.
- **No global `IDC_PROX` checkbox.** QCon ProX support is a per-unit
  property, selected via the device type combo on each row. The
  `CONFIG_FLAG_PROX` bit in the global `flags` int is a **derived
  compatibility value** — it mirrors unit 1's model at parse/save time so
  that existing mode code (`IsFlagSet(CONFIG_FLAG_PROX)`) continues to work.
  It is not exposed as a dialog control.
- Dialog dimensions in `res.rc` and `res.rc_mac_dlg`: widened from 268×114 to
  ~350×310.

---

## Config format

The dialog always shows exactly 8 unit rows at fixed positions. The config
string mirrors this 1:1: **always 8 space-separated entries, position =
unit index**. No `units=` token, no `uN=` prefixes.

```text
KLINKE2 flags=<flags> <in>,<out>,<type> <in>,<out>,<type> ... (8 entries total)
```

| Field | Values | Notes |
|---|---|---|
| `flags` | integer bitset | `CONFIG_FLAG_*` values. `PROX` bit is derived from entry 0's model. |
| `in` | `-1` or REAPER MIDI input index | `-1` = None. |
| `out` | `-1` or REAPER MIDI output index | `-1` = None. |
| `type` | `mackie-main` / `mackie-ext` / `prox-main` / `prox-ext` | Encodes both role and model. `*-main` derives device id `0x14` + transport; `*-ext` derives `0x15`, strips only. `prox-*` derives QCon ProX quirks + 2 LCD panels. |

Each unit entry is one token: comma-separated values, no spaces inside the
token. The parser reads `flags=` then exactly 8 entries. Fewer than 8 → parse
error → safe default.

The serializer **always emits all 8 entries**, even when most are default
(`-1,-1,mackie-ext`). Predictable format, trivial to parse, matches the
dialog's fixed layout.

Example (one main unit, seven idle extenders):

```text
KLINKE2 flags=12 -1,-1,mackie-main -1,-1,mackie-ext -1,-1,mackie-ext -1,-1,mackie-ext -1,-1,mackie-ext -1,-1,mackie-ext -1,-1,mackie-ext -1,-1,mackie-ext
```

The serialized type is the same concept as the dialog preset, just with a
stable lowercase token. There is intentionally no separate serialized `role`
field: role is an implementation detail derived from the selected unit type.

Maximum length (8 entries, 4-digit MIDI IDs): ~290 bytes — well within the
existing `m_configtmp[1024]` buffer.

### Compatibility rules

- Legacy parse: `"0 8 <in> <out> <flags>"` → `SurfaceConfig` with unit 1 =
  `{in, out, main, flags&PROX ? QConProX : Mackie}`, units 2–8 =
  `{-1, -1, ext, mackie}`.
- Legacy strings are **never re-emitted**; saving always writes `KLINKE2`
  with all 8 entries.
- Malformed `KLINKE2` (missing `flags=`, fewer than 8 entries, bad values) →
  `makeDefaultSurfaceConfig()` with `valid = false`.
- `offset` and `size` from the legacy format are ignored (forced to 0/8).
- `CONFIG_FLAG_PROX` in the global `flags` int is **derived from unit 1's
  model** at parse time and during dialog save. This is a backward-compat
  shim for mode code that still reads the flag globally; it will be removed
  once all mode code is per-unit-aware.
- Future incompatible config changes should use a new magic prefix
  (`KLINKE3`, etc.) instead of trying to make the `KLINKE2` parser accept a
  different tuple shape.

### Why 8 fixed entries?

- The dialog shows exactly 8 rows at fixed positions — the config format
  mirrors the UI 1:1.
- No conditional parsing (`if has u3 then u4…`), no `units=` count to
  validate against actual entries, no `uN=` prefix parsing.
- Parser: read `flags=`, then read exactly 8 space-separated unit tokens. Done.
- Serializer: emit `KLINKE2 flags=N`, then emit all 8 unit entries. Done.
- The string is ~290 bytes worst case — negligible.

---

## Build / verify contract

Every step must build clean on Linux and the dialog must open in REAPER.

```bash
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j"$(nproc)")
cp build/reaper_csurf_mcu_klinke.so ~/.config/REAPER/UserPlugins/
```

Then fully restart REAPER, open Preferences → Control/OSC/web, edit the
surface, verify all 8 rows are present and round-trip correctly.

---

## Steps

### Step 1 — Add `SurfaceConfig` data model

**Goal:** introduce the typed config layer as an unused stub. No behavior
change.

- **New files:**
  - `src/core/SurfaceConfig.h`
  - `src/core/SurfaceConfig.cpp`
- **Types:**
  - Reuse `DeviceModel` and `UnitConfig` from `src/hardware/HardwareUnit.h`.
  - `struct SurfaceConfig { int flags; UnitConfig units[8]; bool valid; };`
  - Fixed-size array (not `std::vector`) — always 8 entries, matching the
    dialog and config format.
  - The config string stores a combined unit type token. The parser converts
    that token into `UnitConfig::isMain` and `UnitConfig::model`; the serializer
    converts those two fields back into the stable type token.
  - No `numUnits` field — the array always has 8 entries. `createFunc()`
    constructs a `HardwareUnit` for every entry with real MIDI devices;
    entry count is implicit.
  - No `isLegacy` field — legacy strings are parsed into the same 8-entry
    structure (units 1–7 at defaults) and never re-emitted. The format on
    disk is determined by the serializer (always `KLINKE2`), not by a flag.
- **Constants:**
  - `MAX_SURFACE_UNITS = 8`
- **Default:** unit 1 = Mackie main, MIDI None; units 2–8 = Mackie extender,
  MIDI None; `flags = 0`; `valid = true`.
- **Helper function** `unitTypeToken(const UnitConfig &cfg)` — added in
  `SurfaceConfig.cpp`. Converts `isMain`/`model` to the stable type token
  (`"mackie-main"`, `"mackie-ext"`, `"prox-main"`, `"prox-ext"`). Used by
  the serializer in Step 2 and as a reference for the dialog's
  `CB_SETITEMDATA` encoding in Step 5.
- **`SurfaceConfig.h` includes `HardwareUnit.h`** for `UnitConfig` and
  `DeviceModel`. No cycle risk — `HardwareUnit.h` does not include config
  headers.
- **Verify:** build only. No caller uses it yet.
- **CMakeLists.txt:** no source-list edit is required. The project already
  uses `file(GLOB_RECURSE MCU_SOURCES CONFIGURE_DEPENDS src/*.cpp)`.

### Step 2 — Implement parser and serializer

**Goal:** one parser handles both legacy and `KLINKE2` strings. Serializer
emits `KLINKE2` only, always 8 entries.

- **Files:** `src/core/SurfaceConfig.{h,cpp}`.
- **Functions:**
  - `SurfaceConfig parseSurfaceConfig(const char *str);`
  - `std::string serializeSurfaceConfig(const SurfaceConfig &cfg);`
  - `SurfaceConfig makeDefaultSurfaceConfig();`
- **Legacy parsing:**
  - Empty or whitespace-only string → default config.
  - String starting with a digit → legacy 5-int format.
  - Clone current `parseParms()` behavior: scan up to 5 ints, force
    `offset=0`, `size=8`, populate unit 1 from `<in>, <out>, <flags>`.
    `isMain = true`, model from `flags & PROX`. Units 2–8 = default.
    The resulting `SurfaceConfig` is structurally identical to a
    single-unit `KLINKE2` parse — 8 entries, units 1–7 at defaults.
- **New format parsing:**
  - Only entered when string starts with `KLINKE2`.
  - Whitespace-tokenise. First token must be `flags=<N>`. Then exactly 8
    comma-split unit tokens.
  - Each unit token: `in,out,type`. Split on commas, exactly 3 fields.
  - Validation:
    - MIDI device IDs: `-1` is valid (None); do not validate against current
      device count.
    - Valid type tokens: `mackie-main`, `mackie-ext`, `prox-main`, `prox-ext`.
    - Invalid type → `mackie-main` for entry 0, `mackie-ext` for entries 1–7.
    - Fewer than 9 tokens total (1 `flags=` + 8 entries) → parse error.

- **Error recovery:** any parse failure returns `makeDefaultSurfaceConfig()`
  with `valid = false`.
- **Serializer:**
  - `snprintf(buf, size, "KLINKE2 flags=%d", cfg.flags);`
  - Then for each of the 8 units: append `" %d,%d,%s"` with
    `midiInDev, midiOutDev, unitTypeToken(cfg.units[i])`.
- **Test vectors** (validate via standalone `main()` linked against
  `SurfaceConfig.cpp`, or manually in REAPER with debug logs):

  | Input | Expected unit 1 model | Expected unit 2 | flags |
  |---|---|---|---|
  | `""` | Mackie, MIDI None | ext, Mackie, None | 0 |
  | `"0 8 3 4 12"` | Mackie, in=3 out=4 | ext, Mackie, None | 12 |
  | `"0 8 -1 -1 16"` | QConProX, MIDI None | ext, Mackie, None | 16 |
  | `"KLINKE2 flags=0 -1,-1,mackie-main -1,-1,mackie-ext ..."` (8×) | round-trip identical | — | 0 |
  | `"KLINKE2 flags=12 3,4,prox-main -1,-1,mackie-ext ..."` (8×) | QConProX, in=3 out=4 | — | 12 |
  | `"KLINKE2 flags=0"` (only 1 token) | default config, `valid=false` | — | — |
  | `"garbage"` | default config, `valid=false` | — | — |

- **Verify:** build; run test vectors. No caller uses the functions yet.

### Step 3 — Wire `createFunc()` to `SurfaceConfig`

**Goal:** `createFunc()` parses via `SurfaceConfig`, constructs
`HardwareUnit`s for all configured units, gates input from unit 1+.

- **Files:** `src/core/csurf_mcu.{h,cpp}`.
- **Changes:**
  1. `createFunc()` calls `parseSurfaceConfig(configString)` instead of
     `parseParms()`.
  2. Store `m_surfaceConfig = cfg;` on the `CSurf_MCU` instance.
  3. **Unit construction:** iterate `cfg.units[0..7]`. For each entry with
     `midiInDev != -1 || midiOutDev != -1`, construct a `HardwareUnit`.
     Unit 1 (index 0) is **always** constructed even with MIDI None
     (backward compat — a surface opened without devices must still
     function).
  4. **Duplicate MIDI device warning:** before constructing units, scan
     `cfg.units[0..7]` for duplicate MIDI input or output device IDs
     (excluding `-1`). If found, log a warning:
     `MCU_LOG("WP-B: duplicate MIDI device %d on units %d and %d — multi-open may fail")`.
     Do not block construction — the `HardwareUnit` ctor handles
     `CreateMIDIInput`/`CreateMIDIOutput` failure gracefully (returns NULL).
  5. **Input gate:** after construction and reset, if `m_units.size() > 1`,
     set an internal flag. In `Run()` or the MIDI input dispatch, drop
     events from `unitIndex > 0` with
     `MCU_LOG("WP-B: dropping input from unit %d", idx)`.
  6. Keep existing cached `m_midiout` / `m_midiin` pointers to unit 1's
     ports (from WP-A).
  7. `GetDescString()`: if `m_units.size() > 1`, append ` [N units]`.
- **Reset/start sequence:** make the sequence explicit, because current
  `MCUReset()` still routes most global output through unit 1's cached
  `m_midiout`.
  1. Construct all configured `HardwareUnit`s first.
  2. Call `unit->reset()` for every constructed unit so each port receives the
     Mackie reset SysEx with the unit's own derived device id (`0x14` for
     `*-main`, `0x15` for `*-ext`).
  3. Run the existing surface-level `MCUReset()` once for unit 1 only. This
     preserves today's button-state reset, splash display, assignment digits,
     zoom/scrub LEDs, and other global shims while WP-E is still deferred.
  4. Start every constructed unit's MIDI input with `unit->startInput()`.
     Events from units 2–8 are still dropped by the input gate before they
     reach `OnMIDIEvent()`/`CCSManager`.
  5. Call `forceAllLEDsOff()` for every constructed unit, not just unit 1, so
     additional configured devices do not retain stale LEDs after opening.
- **Destructor/close:** reset and close every constructed unit. The existing
  goodbye display, meter-off, and fader-bottom shims may remain unit-1-only in
  WP-B; per-unit/global routing is WP-E/WP-F.
- **`parseParms()` shim:** keep as a **parse-time only** thin wrapper that
  calls `parseSurfaceConfig()` and fills `parms[5]` from `cfg.units[0]`.
  Used by `dlgProc()` during transition (Step 5). The write path
  (`WM_USER+1024`) goes through `serializeSurfaceConfig()` directly —
  `parseParms()` is never involved in serialization.
- **Verify:** build; one-unit behavior identical. With real MIDI devices on
  unit 2, verify its port opens and receives reset SysEx. Input from
  unit 2 is dropped with a debug log — no assertions fire.

### Step 4 — `GetConfigString()` emits `KLINKE2`

**Goal:** saving the dialog writes the new 8-entry format.

- **Files:** `src/core/csurf_mcu.h` (the inline `GetConfigString()`).
- **Changes:**
  1. Replace `sprintf(m_configtmp, "%d %d %d %d %d", ...)` with
     `snprintf(m_configtmp, sizeof(m_configtmp), "%s",
              serializeSurfaceConfig(m_surfaceConfig).c_str())`.
  2. `m_surfaceConfig` is already stored from Step 3.
- **Verify:** after accepting the dialog, REAPER persists a `KLINKE2` string
  with 8 entries. A legacy surface saved for the first time after WP-B
  migrates to `KLINKE2`.

### Step 5 — Build the full 8-unit dialog

**Goal:** `dlgProc()` shows 8 unit rows exactly as laid out above.

- **Files:** `src/core/csurf_mcu.cpp` (`dlgProc()`, `layoutDlgControls()`),
  `resources/resource.h`, `resources/res.rc`, `resources/res.rc_mac_dlg`.
- **`resources/res.rc` controls:** only the dialog dimensions change from
  `268, 114` to `350, 310`. All new row controls are created dynamically in
  `WM_INITDIALOG`.
- **`resources/res.rc_mac_dlg`:** must be updated to match the larger dialog
  dimensions. Linux includes this file through `src/res_linux.cpp`, and macOS
  uses the SWELL resource directly; leaving it at `268x114` would clip the
  dynamically created rows.
  - **Preferred:** regenerate from the updated `res.rc` via
    `perl reaper-sdk/WDL/swell/swell_resgen.pl resources/res.rc > resources/res.rc_mac_dlg`.
  - **Fallback if regeneration is impractical:** manually edit the two
    dimension literals in the file. Locate the
    `SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_SURFACEEDIT_MCUMAIN,…,"",268,114,…)`
    line (currently near line ~38) and change `268,114` to `350,310`.
    Verify: the same line appears in `#ifdef SET_IDD_SURFACEEDIT_MCUMAIN_SCALE`
    and the non-scale branch — update both instances.
- **Runtime size guard:** in `WM_INITDIALOG`, after all controls are created
  and before `layoutDlgControls()`, enforce the dialog client size as a
  fallback for SWELL hosts that keep the compiled resource dimensions:
  ```cpp
  RECT rc;
  GetClientRect(hwndDlg, &rc);
  if (rc.right < 350 || rc.bottom < 310) {
    SetWindowPos(hwndDlg, NULL, 0, 0, 350, 310, SWP_NOMOVE | SWP_NOZORDER);
  }
  ```
- **`resources/resource.h`:** add ID ranges for the 8 rows' dynamic controls
  (sequential blocks, easy to loop over):
  ```c
  #define IDC_UNIT_TYPE_0  1200  // .. 1207
  #define IDC_UNIT_IN_1    1211  // .. 1217  (row 0 reuses IDC_COMBO2)
  #define IDC_UNIT_OUT_1   1221  // .. 1227  (row 0 reuses IDC_COMBO3)
  ```

#### 5a — `WM_INITDIALOG`

1. Parse: `SurfaceConfig cfg = parseSurfaceConfig((const char *)lParam);`

2. **MIDI device list — populate once, reuse for all rows.**
   Iterate `GetNumMIDIInputs()`/`GetNumMIDIOutputs()` once and build a
   local list of `{id, name}` pairs (two `std::vector`s or small arrays).
   All 8 rows' MIDI combos are populated from this single list — avoids
   calling the REAPER API 8 times for identical data.

3. **Row 0 (Unit 1) — reuse existing controls + add type combo:**
   - Populate `IDC_COMBO2` (MIDI input), `IDC_COMBO3` (MIDI output) from
     the pre-built device list. Add `"None"` entry (item data = `-1`) at
     index 0. Select the entry matching `cfg.units[0].midiInDev`/
     `cfg.units[0].midiOutDev`.
   - Create device type combo `IDC_UNIT_TYPE_0` dynamically.
     **Contents: only `"Mackie Main"` and `"QCon ProX"`** — Unit 1 is
     always a main unit. Store an encoded integer via `CB_SETITEMDATA`:
     `"Mackie Main"` → `0`, `"QCon ProX"` → `2` (encoding matches
     `unitTypeToken` table: 0=mackie-main, 1=mackie-ext, 2=prox-main,
     3=prox-ext). Select based on `cfg.units[0]`.

4. **Rows 1–7 (Units 2–8) — all controls created dynamically.**
   For each `i = 1..7`:
   - Static label: `"Unit N"` (N = i+1), left-aligned, ~35px wide.
   - Device type combo (`IDC_UNIT_TYPE_i`): all four presets:
     `"Mackie Main"` (data=0), `"Mackie Extender"` (data=1),
     `"QCon ProX"` (data=2), `"QCon ProX Extender"` (data=3).
     Default = `"Mackie Extender"` (data=1).
   - MIDI input combo (`IDC_UNIT_IN_i`): populated from the pre-built
     device list + `"None"` (data=`-1`). Default = `"None"`.
   - MIDI output combo (`IDC_UNIT_OUT_i`): same as input.
   - If `cfg.units[i]` differs from defaults, use parsed values.

5. **Existing checkboxes:** populate from `cfg.flags` — but **skip**
   `IDC_PROX` (the control still exists in `res.rc` but is hidden via
   `ShowWindow(GetDlgItem(hwndDlg, IDC_PROX), SW_HIDE)`. It is no longer
   used or shown. PROX state is per-unit via the device type combos.)
   All other checkboxes (`IDC_EMULATE_BLINKING`, `IDC_KEYBOARD_MODIFIER`,
   `IDC_FAKE_TOUCH`, `IDC_CHECK2`) work as before.

6. **Call order:** create all dynamic controls first (rows 0–7), then call
   `layoutDlgControls(hwndDlg)` to position them alongside the existing
   static controls. `layoutDlgControls` uses `GetDlgItem` to retrieve
   each HWND — controls must exist before it runs.

7. **Layout function** `layoutDlgControls(hwndDlg)`:
   ```cpp
   int y = 8;
   const int rowHeight = 22, gap = 4;
   const int labelW = 35, typeW = 130, midiW = 80;
   const int xLabel = 8;

   for (int i = 0; i < 8; ++i) {
     int x = xLabel;
     // "Unit N" label
     SetWindowPos(label, x, y, labelW, rowHeight, ...);
     x += labelW + 4;
     // device type combo
     SetWindowPos(typeCombo, x, y, typeW, rowHeight * 8, ...);
     x += typeW + 8;
     // "In" label
     SetWindowPos(inLabel, x, y, 14, rowHeight, ...);
     x += 16;
     // MIDI input combo
     SetWindowPos(inCombo, x, y, midiW, rowHeight * 8, ...);
     x += midiW + 8;
     // "Out" label
     SetWindowPos(outLabel, x, y, 18, rowHeight, ...);
     x += 20;
     // MIDI output combo
     SetWindowPos(outCombo, x, y, midiW, rowHeight * 8, ...);
     y += rowHeight + gap;
   }
   y += gap;
   // existing checkboxes positioned below (same order as today)
   // buttons positioned at the bottom
   ```
   Total width ≈ 8 + 35+4 + 130+8 + 14+16 + 80+8 + 18+20 + 80 ≈ 350px. ✓

#### 5b — `WM_USER + 1024` (apply)

1. Read all controls:
   - Unit 1: `IDC_UNIT_TYPE_0`, `IDC_COMBO2`, `IDC_COMBO3`.
   - Units 2–8: `IDC_UNIT_TYPE_i`, `IDC_UNIT_IN_i`, `IDC_UNIT_OUT_i`.
2. Convert combo selections to `SurfaceConfig`:
   - **Device type:** use `CB_GETITEMDATA` on the type combo to retrieve
     the encoded integer, then decode to `isMain`/`model`:
     `0`→main+Mackie, `1`→ext+Mackie, `2`→main+QConProX, `3`→ext+QConProX.
     No string comparison — the encoding is stable and language-independent.
   - **MIDI devices:** use `CB_GETITEMDATA` on the in/out combos to
     retrieve the device ID (or `-1` for None).
3. Build `SurfaceConfig cfg` with exactly 8 entries.
   - `cfg.flags`: read from the remaining checkboxes (emulate blinking,
     keyboard modifier, fake touch, swap zoom). **Derive `CONFIG_FLAG_PROX`**
     from `cfg.units[0].model == QConProX`.
   - `cfg.units[0..7]` from row controls.
4. `serializeSurfaceConfig(cfg)` → `lstrcpyn((char *)lParam, str.c_str(), wParam)`.

#### 5c — Control cleanup

- `WM_DESTROY`: destroy all dynamically created controls to avoid handle
  leaks (row 0 type combo, rows 1–7 labels + combos).

- **Verify:**
  - Dialog opens with 8 rows as shown in the layout diagram.
  - Unit 1 shows the current MIDI devices and type preset (main-only combo).
  - Units 2–8 show MIDI None, `Mackie Extender`.
  - Legacy config string opens with Unit 1 populated; saving produces
    8-entry `KLINKE2`.
  - 8-entry `KLINKE2` round-trips identically.
  - Setting a unit 2 type to `QCon ProX Extender` persists correctly.
  - `IDC_PROX` is hidden; the remaining checkboxes work as before.
  - Linux SWELL dialog opens at non-zero size with all 8 rows.
  - Windows `rc.exe` succeeds.
  - macOS dialog opens (SWELL).

### Step 6 — Documentation

- **Files:** `ai-docs/extender-support.md` (update §5.3, §6).
- **Document:**
  - `KLINKE2` config string format (8 fixed entries, space-separated).
  - Unit type tokens (`mackie-main`, `mackie-ext`, `prox-main`, `prox-ext`) and
    their conversion to `UnitConfig::isMain`/`UnitConfig::model`.
  - Legacy → `KLINKE2` migration.
  - Input gating: unit 1+ MIDI ports open but input dropped until WP-C+WP-F.
  - `IDC_PROX` removed from dialog; PROX is per-unit via device type combo.
    `CONFIG_FLAG_PROX` derived from unit 1 for backward compat.
  - WP-B does **not** complete extender support — still requires:
    WP-C (`Tracks`), WP-E (global routing), WP-F (`CCSManager`/VPOT/meter/
    buttons), then per-mode design.

---

## New-type reference

```cpp
// In SurfaceConfig.h — reuses UnitConfig and DeviceModel from HardwareUnit.h

struct SurfaceConfig {
  int flags;               // CONFIG_FLAG_* bits (PROX derived from units[0].model)
  UnitConfig units[8];     // always 8 entries — position = unit index
  bool valid;              // false if parse error → safe default used
};

SurfaceConfig makeDefaultSurfaceConfig();
SurfaceConfig parseSurfaceConfig(const char *str);
std::string serializeSurfaceConfig(const SurfaceConfig &cfg);
const char *unitTypeToken(const UnitConfig &cfg);
```

---

## In-scope vs deferred

**In-scope for WP-B:**

- `SurfaceConfig` model with parser and serializer.
- Legacy 5-int format parse compatibility.
- Full 8-unit dialog with dynamic row creation.
- `GetConfigString()` emits 8-entry `KLINKE2`.
- `createFunc()` constructs all configured `HardwareUnit`s.
- Input gating for unit 1+ (dropped with debug log).
- `IDC_PROX` removed from dialog; PROX is per-unit via device type combo.

**Explicitly not in-scope:**

- Removing the input gate (needs WP-C + WP-F).
- `Tracks` refactor to `N*8` (WP-C).
- `CCSManager` widening beyond channels `0..8` (WP-F).
- `VPOT_LED` array/routing changes (WP-F).
- Meter routing changes (WP-F).
- Per-unit `ButtonManager` state (WP-F).
- Mixed Mackie/QCon display correctness across units (per-mode).
- Per-mode N-unit behavior changes.
- Dynamic "release extenders" feature.

---

## Risks and mitigations

### R1 — SWELL dialog layout with 8 dynamic rows

The dialog dimensions are updated in both `res.rc` and `res.rc_mac_dlg`
(268×114 → 350×310). `layoutDlgControls()` positions all controls
dynamically. The existing `SET_…STYLE=1` fix in `CMakeLists.txt` handles the
zero-size issue on Linux. A runtime `SetWindowPos` size guard in
`WM_INITDIALOG` (specified in Step 5) catches any SWELL host that keeps the
old compiled resource size. If the dialog still renders incorrectly on one
platform, only `layoutDlgControls()` needs adjustment — the resource files
are a one-time dimension change.

### R2 — Unit 1+ input gating fails → assertion

The input gate drops events from `unitIndex > 0` before they reach
`CCSManager`. If it fails, `CHECKMODEANDCHANNEL` asserts for channel 9+.
Mitigation: straightforward early-return — low risk. Test with 2 units
configured and verify debug log shows "dropping input" but no assertions.

### R3 — `CONFIG_FLAG_PROX` de-synced from unit 1 model

Mitigation: the flag is **derived** at exactly two points — parse time
(`parseSurfaceConfig`) and dialog save (`WM_USER+1024`). Both derive it
from `cfg.units[0].model`, which itself is derived from the row's unit type
token. There is no independent dialog control that can change the model
without updating the flag. Existing mode code that reads
`IsFlagSet(CONFIG_FLAG_PROX)` always sees the correct value for unit 1.

### R4 — Parser accepts invalid MIDI device IDs

Mitigation: parser is machine-independent. Validation at two points:
(a) `WM_INITDIALOG` — invalid saved ID → combo shows "None";
(b) `createFunc()` → `CreateMIDIInput`/`CreateMIDIOutput` returns NULL
for invalid IDs → `HardwareUnit` constructor handles it (portless unit).

### R5 — Dynamic control creation fails on one platform

Mitigation: `CreateWindow` with `WC_COMBOBOX`/`WC_STATIC` works on Win32
and SWELL. Existing code already uses `GetDlgItem` for all controls. If
SWELL on macOS/Linux behaves differently, the fix is localised to the
`WM_INITDIALOG` dynamic-creation block.

### R6 — Duplicate MIDI device IDs across units

If the user assigns the same MIDI device to two units, the second
`CreateMIDIInput`/`CreateMIDIOutput` may fail on some drivers (Windows ASIO/MME
in particular; Linux ALSA/JACK typically supports multi-open). Mitigation:
`createFunc()` logs a warning for duplicate device IDs (Step 3 item 4).
`HardwareUnit` ctor handles NULL port gracefully. The dialog does not block
duplicates — this is a configuration error, not a crash risk. Duplicate
prevention in the dialog UI is deferred until multi-unit operation is
user-facing.

---

## Exit criteria

WP-B is done when:

- Legacy `"0 8 …"` strings load and produce a working surface.
- Dialog shows 8 rows exactly as in the layout diagram.
- Saving writes an 8-entry `KLINKE2` string; reopening round-trips all fields.
- Unit 1 device type combo is main-only; units 2–8 offer all four presets.
- Unit 1 MIDI devices and type preset persist correctly.
- Units 2–8 default to MIDI None, `Mackie Extender`.
- Setting real MIDI devices on unit 2+ opens those ports and gates input.
- `IDC_PROX` is hidden; remaining checkboxes work as before.
- No compile-time gate (`MCU_DEV_MULTI_UNIT_CONFIG`) exists anywhere.
- Linux, Windows, and macOS all compile and the dialog opens.
