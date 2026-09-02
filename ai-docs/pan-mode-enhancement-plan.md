# Pan-Mode Enhancement Plan — REAPER Pan Modes (Stereo/Dual Pan)

Status: DRAFT (2026-09-02), decided NOT to implement yet — design note only.
Trigger: the Pan-Mode currently controls only the classic balance pan;
REAPER offers per-track pan modes (stereo pan, dual pan) that are neither
reflected on the display nor controllable from the surface.

## 1. Current state

`src/modes/multitrack/PanMode.cpp`:

- VPOT turn → `CSurf_OnPanChange(tr, numSteps / 40.0, true)` — balance pan
  only (×5 when the VPOT is pressed).
- Flip mode: fader ↔ VPOT swap (fader controls pan, VPOT volume).
- Display: `Display::showPan(row, ch, GetSurfacePan(tr))` — 6-char field,
  formats `L/R <n>%%` or `center` (`Display.cpp` `showPan()`, `char text[7]`).
- No handling of REAPER's newer per-track pan modes anywhere.

## 2. REAPER pan model

Per track, `I_PANMODE` (`GetTrackInfo`/`GetSetTrackInfo`, see pinned SDK
`reaper-sdk/sdk/reaper_plugin_functions.h`):

| Mode | Meaning | Values |
|---|---|---|
| `0` | classic balance (REAPER 3 era) | `D_PAN` |
| `3` | "new balance" (default since REAPER 4) — the only mode we handle today | `D_PAN` |
| `5` | stereo pan: whole stereo field moves | `D_PAN` (position) + width |
| `6` | dual pan: left/right sides independent | `D_DUALPANL` + `D_DUALPANR` |

Related attributes: `D_PAN` (-1..1), `D_DUALPANL`/`D_DUALPANR` (only when
`I_PANMODE==6`), `D_PANLAW` (<0 = project default), `D_WIDTH` (stereo width).

Convenience reader: `bool GetTrackUIPan(MediaTrack*, double* pan1Out,
double* pan2Out, int* panmodeOut)` — returns the effective mode and both
values in one call.

csurf-layer API: `CSURF_EXT_SETPAN_EX` (0x0001000E) in `reaper_plugin.h`:
`parm1=(MediaTrack*)track, parm2=(double*)pan, parm3=(int*)mode`; for modes
5 and 6 `pan` points to an array of two doubles. SDK comment: "if a csurf
supports CSURF_EXT_SETPAN_EX, it should ignore CSurf_SetSurfacePan".

Also available (not used by the Pan-Mode, listed for completeness):
`CSurf_OnPanChangeEx` (extra `allowGang` flag), `CSurf_OnWidthChange`,
`CSurf_OnSendPanChange` / `CSurf_OnRecvPanChange` (send/receive pan —
relevant for SendMode/ReceiveMode, not Pan-Mode).

## 3. REAPER version floor

The new pan modes and the APIs above postdate our current hard load floor
of REAPER 6.37 (`TrackFX_GetParamFromIdent`). Community sources put the
introduction around REAPER 6.64 — **exact version still to be verified
against the official SDK commit history** (one commit per release) before
anything is implemented.

Consequence per AGENTS.md: all of these functions must be **resolved lazily
with a fallback** (balance-only behaviour when missing), NOT added as hard
`IMPAPI` entries in `src/csurf_main.cpp` — the floor stays 6.37.

## 4. Proposed changes (4 steps, each independently shippable)

### Step 1 — Display: show the real pan mode (correctness first)

Read the per-track mode via `GetTrackUIPan` (lazily resolved; on REAPERs
without it, fall back to today's `GetSurfacePan`-based display, i.e.
assume balance).

- Balance (0/3): unchanged (`L25 R`, `center`, ...).
- Stereo pan (5): show position and width in some compact 6-char form.
- Dual pan (6): show both side values compactly.
- Optional mode indicator so the user can see a track is not in balance
  mode.

Open question D1 — 6-char field formats (current field is `char[7]` in
`Display.cpp`):
- dual pan: `L12 R5`? `L-12+5`? `L12R5 `? (sign handling + separator)
- stereo pan: e.g. `P12 W50`?
- mode indicator: prefix char (`S`/`D`) costs one of the 6 chars.
Decision needed before implementation.

### Step 2 — `CSURF_EXT_SETPAN_EX` in `Extended()`

Handle 0x0001000E in `CSurf_MCU::Extended()` (check
`src/core/csurf_mcu.cpp` for the existing `Extended` switch) so REAPER
syncs stereo/dual pan GUI changes into the surface state/display. Per the
SDK comment, when supported we must then ignore
`CSurf_SetSurfacePan` for the affected tracks — interaction with the
existing `SetSurfacePan` path needs to be checked (today
`CSurf_SetSurfacePan` feeds `GetSurfacePan`, which `updateDisplay()` reads
every frame).

This is the base for steps 3–4: without it the surface state and REAPER go
out of sync for non-balance tracks.

### Step 3 — VPOT controls the second parameter

Per track mode, the VPOT turn acts on different values:

- Dual pan (6): plain turn = left side; **Shift+turn** = right side
  (mapping direction is a taste decision — open question D2).
- Stereo pan (5): plain turn = position; **Shift+turn** = width.
- Balance (0/3): unchanged, keeps `CSurf_OnPanChange`.

Writing goes through `GetSetTrackInfo` (`D_PAN`/`D_DUALPANR`/`D_WIDTH`),
because `CSurf_OnPanChange` only knows the balance value. Note:
`GetSetTrackInfo` writes bypass REAPER's csurf volume/pan bookkeeping, so
after a write we must refresh our own cached surface state (check whether
`GetSurfacePan`/`GetSurfaceVolume` cache or read live, and call
`CSurf_ResetAllCachedVolPanStates` if needed). Also decide how this
interacts with flip mode (fader controls pan when flipped — open question D3:
does the fader then control the second parameter, or does flip simply not
apply to non-balance tracks?).

Shift-detection for turns: check what the existing turn path sees (MIDI CC
shift state via modifier tracking, see `isModifierPressed` used in
`vpotPressed`).

### Step 4 — Cycle the pan mode: Shift+Ctrl+VPOT press

Cycles `I_PANMODE` Balance (3) → Stereo (5) → Dual (6) → Balance, flashes
the new mode on the display. Fits the existing modifier-command-scheme
(`ai-docs/modifier-command-scheme.md`) — note the current
`vpotPressed()` intercept is `VK_CONTROL && !VK_SHIFT`; a
`CTRL+SHIFT+VPOT` binding is a new layer and must not clash with the
existing CTRL commands (Insert/Duplic/Clear/Remove on VPOTs 1–4). Open
question D4: does Shift+Ctrl+VPOT 1–4 pre-empt the plain CTRL commands,
and what do VPOTs 5–8 do?

## 5. Not in scope (separate feature)

Send/Receive pan (`CSurf_OnSendPanChange`/`CSurf_OnRecvPanChange`) in
SendMode/ReceiveMode — the VPOT would pan the send/receive instead of its
level. Different mode, different plan.

## 6. Implementation checklist (when it starts)

- [ ] Verify exact REAPER version of `I_PANMODE`/`GetTrackUIPan`/
      `CSURF_EXT_SETPAN_EX` via official SDK commit history
      (https://github.com/justinfrankel/reaper-sdk) — lazy resolve
      guaranteed, floor stays 6.37.
- [ ] Decide D1 (display formats), D2 (dual-pan turn mapping), D3 (flip
      mode interaction), D4 (mode-cycle command slot).
- [ ] Step 2 first (sync), then Step 1 (display), then 3, then 4.
- [ ] Manual update: `manual/text_en/panmode.tex` (pan-mode section +
      commands list).
- [ ] VERSION.txt bump per the versioning rules when shipping.
