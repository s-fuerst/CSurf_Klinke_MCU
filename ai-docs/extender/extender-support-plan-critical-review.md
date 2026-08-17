# Extender Support — General Plan Critical Review

> Review date: 2026-07-08.
> Scope: general architecture decisions and work-package split in
> `ai-docs/extender-support.md`, with context from the WP-A design and
> implementation-plan documents.

## Summary

The main architecture decision is sound: one logical `CSurf_MCU` owning
multiple physical hardware units is the right direction for this codebase.
The main weakness is staging. The current work-package split treats several
deeply coupled areas as separable:

- hardware-unit extraction,
- logical channel sizing,
- button/touch/LED state,
- `Tracks` channel mapping,
- display routing,
- configuration exposure.

That split is too clean on paper and too loose against the real source tree.
The plan should define stricter gates before multi-unit configuration becomes
user-visible.

## Architecture Decisions

### One logical surface owning N hardware units

This is the right choice.

The alternative, multiple `CSurf_MCU` instances like the Cockos original, would
fight this codebase:

- `Tracks` is a singleton and currently stores one `CSurf_MCU*`.
- modes share global/logical state.
- Klinke-specific mode behavior is much richer than the Cockos stock MCU
  implementation.
- global features such as selected track state, project config, actions,
  plug mode, and transport state are already modeled as one surface-level
  system.

A single logical surface with `HardwareUnit` children is cleaner and gives one
place to reason about linked modes, bank size, selected tracks, and global
routing.

### Linked modes first

This is also the right decision.

Independent modes per unit would be a major feature, not a small extension of
the scaffolding. Starting with linked modes keeps the first multi-unit behavior
understandable: more strips for the same logical mode.

The risk is over-designing APIs for a future independent-mode system. It is
worth keeping unit context in low-level events, but the mode layer should not
be made more complex than linked mode requires yet.

### Implicit offsets from unit order

This is a good decision.

Manual offsets would add user-facing configuration complexity and make invalid
states easy. For a single logical surface, unit order should define channel
order:

- unit 0: channels 1-8,
- unit 1: channels 9-16,
- unit 2: channels 17-24,
- etc.

This should remain the default model.

### Position and main capability are orthogonal

This is conceptually correct, but it has implementation cost.

The rule "position defines channel order only; transport capability defines
global routing" is flexible and publishable. It also means the code cannot rely
on a single `main unit` or `unit 0 is main` shortcut. Global routing must be
capability-based from the beginning of the real N>1 path.

This is a good long-term model, but it should not be partially implemented.
Either keep WP-A strictly N=1, or implement capability-based routing properly
before exposing multiple units.

### Capability model and device presets

The capability/preset approach is good in principle, but the current plan
underestimates the cost of mixed models.

`Mackie` vs `QConProX` is not only a `DisplayHandler` detail. It affects:

- LCD row count and row semantics,
- VPOT ring encoding,
- meter bridge behavior,
- LED blink behavior,
- assignment display suppression,
- mode-level display layout decisions.

If mixed Mackie/QCon configurations are allowed in the UI, the mode-facing
display abstraction and hardware routing need to be ready for it. Otherwise
WP-B should temporarily restrict configurations to one model family, or clearly
mark mixed models as unsupported until later.

## Work-Package Critique

### WP-A is overloaded and under-bounded

The current WP-A tries to be both:

- a safe N=1 extraction, and
- the foundation after which WP-B can feed N units.

Those are different promises.

As an N=1 extraction, WP-A is reasonable:

- move MIDI I/O into `HardwareUnit`,
- keep `CSurf_MCU` forwarding shims,
- preserve current behavior,
- keep legacy config strings,
- build and regress often.

As an N-generic foundation, it is incomplete unless it includes at least part
of channel sizing, button state, display routing, and per-unit output routing.

Recommended correction: make WP-A explicitly N=1 only. Do not delete legacy
compatibility APIs until their users are replaced.

### WP-B should not expose multi-unit config before the engine handles N>1

The current sequencing puts config dialog and config string early. That is
risky.

If the UI can store multiple units before the internal routing is truly
multi-unit capable, the project gains user-visible states that cannot be
handled correctly. That makes debugging harder and creates half-supported
configurations.

Recommended correction: defer user-facing multi-unit configuration until an
internal 2-unit path works end to end. A hidden hardcoded or debug-only 2-unit
setup is safer for early testing than exposing config UI too soon.

### WP-C and WP-F are too coupled to WP-A

`Tracks`, `CCSManager`, VPOT state, touch state, fader caches, strip LED state,
metering, and button dispatch are not cleanly separable from `HardwareUnit` if
the goal is N>1.

The plan can still split the work, but the split should be honest:

- WP-A can be N=1-only.
- The first N>1 WP must include minimal channel sizing through `CCSManager`,
  input routing, output routing, and display routing.
- `Tracks` and banking must be made N-aware before user-facing configuration
  is enabled.

### Dynamic unit activation should come later

The "release extenders" feature is useful, but it is cross-cutting:

- configured units vs active units,
- active channel count,
- anchors out of active range,
- bank size,
- output suppression,
- input suppression,
- display and meter clearing,
- reflow on toggle.

Adding this before the static multi-unit path is stable will make every
intermediate step harder to reason about.

Recommended correction: treat dynamic activation as a later feature after
static N>1 linked mode works.

### Per-mode work should start with MultiTrack only

The mode ordering is sensible:

1. MultiTrack,
2. Send/Receive,
3. Command,
4. Plug.

MultiTrack should be the first real N>1 proof because it most naturally maps
to "more strips". PlugMode should remain last. Its parameter maps, page/bank
logic, touched display, dry/wet/master-slot behavior, and ProX rows are likely
to need a mode-specific design instead of a simple `N*8` expansion.

## Suggested Revised Work Packages

### WP-A — N=1 `HardwareUnit` extraction

Scope:

- introduce `HardwareUnit`,
- move MIDI I/O, reset, raw send, and possibly unit-local display output,
- keep `CSurf_MCU` shims,
- keep legacy config string behavior,
- keep compatibility methods such as `GetOffset()` and `GetSize()` until
  replacement call sites exist,
- prove behavior is identical for one unit.

Explicit non-goal: enabling N>1.

### WP-B — Hidden internal multi-unit model

Scope:

- represent a vector of units internally,
- add unit-aware polling and unit-aware raw output paths,
- keep UI/config single-unit or use a developer-only hardcoded test path,
- do not expose multiple units to users yet.

Goal: make it possible to test two units internally without claiming product
support.

### WP-C — Logical channel sizing and routing

Scope:

- replace `CCSManager` fixed `[9]` assumptions where required,
- make channel assertions use `availableChannels()`,
- add unit-aware strip LED routing,
- add unit-aware fader, VPOT, meter routing,
- make button state either per-unit or explicitly routed through a unit-aware
  adapter.

Goal: channels above 8 do not assert, collide, or route to unit 0.

### WP-D — Tracks and banking

Scope:

- make `Tracks::adjust()` and `m_channelTracks` handle `N*8`,
- remove the `m_pMCU->GetOffset()` dependency,
- define bank size from the active logical channel count,
- update anchor and quick-jump behavior,
- handle out-of-range anchors without destructive clamping.

Goal: MultiTrack can see and move a 16/24-channel logical window.

### WP-E — Static global routing and display behavior

Scope:

- route transport LEDs, SMPTE/timecode, assignment display, automation LEDs,
  flip/global-view/drop/save/undo/metronome by unit capability,
- define exactly what multiple transport-capable units do,
- define model-mixing policy for Mackie/QCon display rows.

Goal: global hardware features behave predictably with more than one unit.

### WP-F — User-facing config and persistence

Scope:

- introduce the new config string,
- migrate legacy 5-int strings,
- update the config dialog,
- enforce any temporary restrictions such as one model family only, if needed.

Goal: users can configure the multi-unit path only after the engine can
actually run it.

### WP-G — Dynamic unit activation

Scope:

- active/inactive unit flag,
- input suppression,
- output suppression and clear/reset behavior,
- reflow on activation changes,
- anchor behavior when active range shrinks or expands.

Goal: "release extenders" after static multi-unit behavior is stable.

### WP-H — Mode-by-mode enablement

Suggested order:

1. MultiTrackMode,
2. SendMode/ReceiveMode,
3. CommandMode,
4. PlugMode.

Each mode should get a short design note before implementation. The key
question for each mode is whether additional units extend the view, mirror the
main unit, or need a mode-specific layout.

## Recommended Gates

Before exposing multi-unit config:

- two hardcoded units can be polled,
- channel 9+ input reaches `CCSManager` without asserts,
- channel 9+ fader/LED/VPOT/meter output routes to unit 1,
- `Tracks` exposes at least 16 logical channels,
- MultiTrack bank movement behaves predictably,
- display writes for channels 9-16 route to the second unit,
- global transport/timecode routing is capability-based.

Before enabling mixed device models:

- mode display code no longer relies on one global `CONFIG_FLAG_PROX`,
- ProX rows are handled per child display,
- VPOT/meter/blink quirks are per unit.

Before dynamic activation:

- static N>1 linked mode works,
- out-of-range anchors are preserved,
- deactivated units are cleared or left in a documented state.

## Bottom Line

The general decisions are technically reasonable. The plan needs harder
boundaries and later exposure of user-facing configuration. The safest path is:

1. extract N=1 cleanly,
2. make a hidden N>1 path work internally,
3. make logical channels, tracks, displays, and routing truly N-aware,
4. expose configuration,
5. add dynamic activation and complex modes.

This avoids creating user-visible configurations before the engine can honor
them, and it keeps each implementation checkpoint testable.
