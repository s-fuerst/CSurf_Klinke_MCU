# Critical Review: extender-wp-sendrecv-impl-plan.md

> Review date: 2026-07-12.
> Scope: line-by-line audit of the Send/Receive widening plan against the
> actual source tree (`src/modes/sends/`, `src/hardware/display/`,
> `src/core/CCSManager.cpp`, `src/core/Tracks.cpp`).
> Method: read-only verification. **No code was changed.**

## Overall assessment

The plan is solid, well-researched, and the core widening strategy is correct.
I found **one real correctness bug** (the ProX assignment-display routing at
N>1), **three factual errors / oversights**, and several minor items that
should be fixed before implementation begins.

The widening primitive (mirroring `getNumberOfChannelStrips()` exactly like
MultiTrackMode) is sound. The step ordering keeps the build compilable after
each step. The N=1 equivalence guarantee holds by construction.

---

## 🔴 Finding 1 — ProX assignment-display field 9 is misrouted with MultiDisplay (CORRECTNESS BUG)

**Plan claim (Step 5):**
> The assignment-field write at the end of `updateDisplayProX`
> (`changeField(2, 9, …)`, selected-track name + master vol) is the per-unit
> assignment slot (field 9 = "8 strips + assignment" per unit), **not** a
> global strip — leave it at `9`.

**Why this is wrong:** When `m_pDisplay` is a `MultiDisplay` (N>1),
`MultiDisplay::changeField` routes **all** fields by the same global→local
mapping:

```cpp
// MultiDisplay.cpp:33-44
void MultiDisplay::changeField(int row, int field, ...) {
  int numStrips = (int)m_children.size() * 8;
  if (field < 1 || field > numStrips)
    return;
  int unitIndex = (field - 1) / 8;      // field 9 → unit 1
  int localField = (field - 1) % 8 + 1; // field 9 → local field 1 ← WRONG
  ...
}
```

Field 9 maps to **unit index 1, local field 1** — the extender's first strip,
not any assignment slot. The selected track name and master volume end up on
the wrong unit, on the wrong display position.

There is no special-casing for field 9. The correct way to write to **every**
unit's assignment slot is `MultiDisplay::broadcastField(2, 9, ...)` — which
calls `child->changeField(row, field, ...)` with `field=9` on every child.

**Both writes in `updateDisplayProX` are affected:**
- `m_pDisplay->changeField(2, 9, ...)` → should be broadcast
- `m_pDisplay->showDB(3, 9, vol)` → calls `changeField(3, 9, ...)` internally
  → same routing problem → should use broadcast

**Note:** The same bug exists in `MultiTrackMode.cpp:502`
(`changeField(2, 9, "Master")`), so this is a systemic issue, not
SendRecv-specific. But the plan explicitly *affirms* the wrong behavior — that
must be corrected.

**Recommended fix:** Either add a `MultiDisplay::changeAssignmentField()` or
use `broadcastField`. At minimum, add a `// FIXME` comment and a note in
"Risks" or "Deferred" acknowledging the routing breaks at N>1 for
assignment-display writes.

---

## 🟡 Finding 2 — `updateFaders` master-fader block is INSIDE the loop, not outside it (FACTUAL ERROR)

**Plan claim (Step 3):**
> The master-fader block (`channel == 0`, selected-track volume) is outside the
> loop and unchanged.

**Reality** — the master block is the last statement inside every loop
iteration:

```cpp
for (unsigned int iInfo = 1; iInfo < 9; iInfo++) {
    if (m_startWithSend + iInfo <= m_sendInfos.size()) { /* strip fader */ }
    else
      m_pCCSManager->setFader(this, iInfo, 0);

    // set master fader to selectedTrack value   ← INSIDE the loop
    if (selectedTrack() != NULL) {
      m_pCCSManager->setFader(this, 0, ...);
    } else {
      m_pCCSManager->setFader(this, 0, 0);
    }
}
```

At N=2, the master fader is set **16 times per frame** instead of once.
Functionally harmless (idempotent, `WP-A` broadcasts `setFader(0)` to all
units, last write wins), but wasteful. More importantly, the plan's
description is simply wrong. Since the plan claims this code is "unchanged,"
a reader might trust that and skip verifying — but the code should actually be
restructured to move the master block outside the loop while widening.

**Recommended fix for Step 3:** Move the master-fader block after the loop
closing brace. Add to Step 3 scope.

---

## 🟡 Finding 3 — `updateDisplay` has a scoped-variable shadow that amplifies with widening (CODE SMELL)

The `updateDisplay` method (line 182–235) has an outer loop
`for (iInfo = 0; ... ; iInfo++)` and, inside the fader-touched branch, an
inner loop `for (unsigned int iInfo = 0; iInfo < 8; iInfo++)` that **shadows
the outer `iInfo`**.

The plan correctly widens the inner loop to `nStrips`, but doesn't note that:
- The inner loop now runs `nStrips` iterations **per outer-loop iteration**
  when faders are touched (N=2 → 256 automode field writes instead of 64, all
  hitting the same 16 fields → last write wins → functionally correct but 4×
  the pre-existing waste).
- The code after the inner loop
  (`int sendIdx = calcSendIdxGet(m_startWithSend + iInfo)`) uses the **outer**
  `iInfo` — this is the **only** thing that makes the code correct, and the
  scoping is invisible without careful reading.

**Recommended fix:** Add a `// NOTE: inner loop shadows outer iInfo` comment
in Step 5 so the next person isn't confused. Consider renaming the inner
loop variable `iInfo → iInfo2` for clarity.

---

## 🟡 Finding 4 — Banking footnotes need minor fixes (CLARITY)

**Plan text (Step 8, footnotes):**
> "Optional hardening: a track with zero sends would underflow `size()-1` to
> `-1`, but the mode is unreachable then (CCSManager gates `B_VPOT_SEND` on
> `getNumSends() > 0`); leave as-is to keep N=1 equivalence airtight."

This footnote is **wrong about the mechanism:** `m_sendInfos.size()` returns
`size_t` (unsigned), but the cast `(int)m_sendInfos.size()` makes it signed.
When size is 0, `(int)0 - 1 = -1`, and the clamp `m_startWithSend < 0 → 0`
handles it. However, the mode is gated on `getNumSends() > 0` anyway, so this
is academic. **Delete or correct the footnote.**

**Also:** The plan should explicitly state that when a track has **exactly**
`nStrips` sends (e.g., 16 sends at N=2), bank-up is blocked — you can't page
to an empty window. This is symmetric with the old N=1 behavior (8 sends →
bank-up blocked), but worth calling out since it's the most subtle behavioral
change in the banking step.

---

## 🟢 Finding 5 — `updateFaders` uses `<=` guard (OK, just different)

The `updateFaders` method uses `m_startWithSend + iInfo <= m_sendInfos.size()`
(one-indexed loop base + `<=`), while every other update method uses
`m_startWithSend + iInfo < m_sendInfos.size()` (zero-indexed). The plan
preserves this. At N=2 with a narrow send list, it works correctly — strips
beyond the send count get fader 0 via the `else` branch. ✓

---

## 🟢 Finding 6 — Minor verifications (all PASS)

| Claim | Verdict |
|---|---|
| `Display::changeField` ASSERT `field < 9` / `field < 10` does not need widening (MultiDisplay routes global→local first) | ✓ Correct |
| All input handlers (`buttonRec`, `buttonMute`, `buttonSolo`, `fader`, `vpotMoved`, `buttonSelect`) already N-safe via `sendNr = m_startWithSend + channel - 1` | ✓ Correct |
| `getSendInfo` returns NULL for out-of-bounds send indices (REAPER API handles it) | ✓ Correct |
| No new `#include` needed (`<vector>` already in header, `Tracks.h` already in .cpp) | ✓ Correct |
| No `CMakeLists.txt` change required | ✓ Correct |
| `updateEverything()` at end of `buttonFaderBanks` preserved → triggers all widened methods | ✓ Correct |
| `getChannelOffset()` returns `m_startWithSend` (send index, not channel) — correct as-is | ✓ Correct |
| `SendReceiveMeterBridge::updateMeterBridge` already has `ensureStripMeterState` | ✓ Correct |
| Step 6 resize guard: `m_recButtonPressed.resize(channel, false)` pads correctly with `false` | ✓ Correct |

---

## 🟢 Finding 7 — The static grep check is slightly incomplete

The plan's final verification grep:

```bash
grep -nE "iInfo < 8|iInfo < 9|i < 8|i < 9|\[8\]|< 9;" src/modes/sends/
```

This will catch all loop bounds and array declarations after widening, but it
won't catch the old `ASSERT(channel < 9)` on line 342 since `< 9;` matches
the ASSERT pattern. It **will** catch the `for (unsigned int i = 1; i < 9;
i++)` in `setAutoMode` until Step 7 replaces it — order-dependent. Works for
manual use, but add an explicit note:

```bash
# Also verify the one ASSERT is gone:
grep -n "channel < 9" src/modes/sends/
```

---

## 🟢 Finding 8 — Inner automode display loop implicitly depends on clearLine

In `updateDisplay` (line 188–210), the inner automode loop:

```cpp
for (unsigned int iInfo = 0; iInfo < 8; iInfo++) {
    if (m_startWithSend + iInfo < m_sendInfos.size()) {
        // write automode text
    }
    // no else → empty strips get no write
}
```

After widening to `nStrips`, strips beyond the send count silently get no
write (the `if` guard skips them). The field is NOT explicitly cleared — but
the outer loop's `m_pDisplay->clearLine(0)` right before the inner loop
handles that. ✓ This works, but the plan could note the implicit dependency on
`clearLine(0)` for strip blanking — without it, stale automode text from a
previous track would persist on empty strips.

---

## 🟢 Finding 9 — `vector<bool>` choice is safe (confirmed)

The plan notes that `std::vector<bool>` proxy issues are a risk if anyone
takes `&m_recButtonPressed[i]`. Audit of all accesses:
- `SendReceiveModeBase.cpp`: `m_recButtonPressed[i] = false` (by value)
- `buttonRec`: `m_recButtonPressed[channel - 1] = pressed` (by value)
- `setAutoMode`: `if (m_recButtonPressed[i])` (by value copy)

No address-of operator, no `auto&`, no iterator. `vector<bool>` is safe. ✓

---

## 🟢 Finding 10 — The plan's step count (10), file claims, and dependency note are accurate

- 10 steps, all in `src/modes/sends/` → correct (SendReceiveMeterBridge lives there)
- Depends on WP-A, WP-B, WP-C, WP-D, WP-EF → correct
- MultiTrackMode as a precedent → correct (8 sites already widened with `// WP-F:`)

---

## Summary

| Severity | # | Action |
|---|---|---|
| 🔴 Critical (wrong behavior at N>1) | Finding 1 | Fix ProX assignment routing — `changeField(2, 9, ...)` + `showDB(3, 9, ...)` must use `broadcastField` (also a systemic MultiTrackMode bug) |
| 🟡 Important (factual errors) | Findings 2–4 | Move master fader out of `updateFaders` loop; add inner-loop shadow comment in `updateDisplay`; fix/clarify banking footnotes |
| 🟢 Minor / polish | Findings 5–10 | Optional; no corrective action needed |

**Verdict:** The plan is implementation-ready after Finding 1 is resolved. The
widening strategy (mirroring `getNumberOfChannelStrips()` like MultiTrackMode)
is sound, the step ordering preserves buildability, and the N=1 equivalence
guarantee holds. Fix the ProX assignment routing, move the master-fader block
outside the loop, and add a comment about the `updateDisplay` shadow before
implementing.
