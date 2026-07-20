# Critical Review — WP-PlugMode Multi-Unit Implementation Plan

**Reviewed plan:** `extender-wp-plugmode-impl-plan.md`  
**Review date:** 2026-07-13  
**Verdict:** The architectural direction is sound, but the plan is not yet
implementation-ready. Resolve the blocking items before starting Phase 0.

## Blocking findings

### 1. PlugMode currently rejects all extender input

`PlugMode` does not override `CCSMode::supportsExtendedChannels()`. Consequently,
`CCSManager` drops all channel-specific events above channel 8 before they reach
the mode: faders, V-Pots, SOLO, MUTE, SELECT, REC, and touch events. Phase 2
assumes these events are already delivered, but the required mode capability is
not mentioned anywhere in the plan.

**Required change:** Add
`bool supportsExtendedChannels() const override { return true; }` to
`PlugMode`, and add regression coverage for every relevant extender input path.

### 2. Phase 7 mischaracterises PlugModeMeterBridge

The existing `PlugModeMeterBridge` already loops over
`CSurf_MCU::availableChannels()` and sends each meter through its global channel.
The routing layer then sends it to the owning hardware unit. Creating one bridge
per unit and invoking all of them every frame would duplicate every meter message
once per unit.

Furthermore, `alsoOnDisplay()` is not currently consumed by LCD-rendering code.
There is no existing "MeterBridge rendering on the display" to retarget to a
child display.

**Required change:** Remove Phase 7 from this work package. If LCD meters are
desired, define them as a separate feature with a rendering model, display
ownership, and protocol verification.

### 3. Page spreading can select unused pages

The planned `clamp(u, lastUsedPage)` logic does not map an ordinal to a used
page. For a bank with used pages `{0, 2, 5}`, unit 1 would be assigned page 1,
which is unused. The same error affects default spreading, transport page moves,
and Control cascade.

**Required change:** Introduce one shared helper that operates on an ordered
list of used pages, for example `usedPages(bank)` plus
`pageAtUsedOffset(bank, offset)`. Specify whether the page window refers to
physical page indices or positions in the used-page sequence; the latter is the
only useful interpretation for sparse maps.

### 4. A main unit is optional, but the plan assumes one exists

Surface configurations with no main unit are valid. `MultiDisplay::mainChild()`
returns `NULL` in that configuration. The plan uses it for global messages,
the global plug selector, and the transport reference, creating a null-pointer
and invisible-display risk.

**Required change:** Define a shared anchor policy: use a main unit when one
exists, otherwise use unit 0. Apply it consistently to display routing,
transport actions, editor state, and global selectors.

### 5. The proposed persistence type is not a usable C++ value type

Native arrays such as `int banksPerUnit[8]` and `int pagesPerUnit[8][8]` cannot
be safely stored, copied, and assigned as elements of the existing
`boost::tuple` slot-state type. In addition, `MAX_SURFACE_UNITS` is not defined
in the current source tree.

**Required change:** Use explicit copyable value types, such as nested
`boost::array` or `std::array`, and either introduce a project-wide constant
or use the existing fixed surface limit in one named location. Define XML
versioning and validation together with this representation.

### 6. `activeUnit()` is underspecified and phase boundaries are inconsistent

The plan alternates between "main" and "last-touched" semantics without
defining ownership, update timing, or callback order. Existing editor callbacks
are invoked before the bank/page mutation. If selected-based compatibility APIs
depend on an implicit active unit, they can update the editor or parameter
resolution for the wrong unit.

Phase 1 and Phase 2 also refer to per-unit selectors that do not exist until
Phase 3. The proposed transitional state is not a complete, independently
buildable design despite the stated per-phase N=1 guarantee.

**Required change:** Store one explicit `m_activeUnit` in `PlugMode`, set it
before any unit-specific callback, and document its update rules. Reorder the
work so that the state model, per-unit selectors, and all selector consumers
land together, or provide a complete compatibility adapter.

### 7. The option-display paging work is based on a false premise

`Options` displays four option values in text columns. The eight V-Pots change
the four option values backward or forward; the number of attributes of a single
option is not limited to eight display fields. Its existing cyclic selection
already supports `OFF | Unit 1 ... Unit 8`.

**Required change:** Do not modify `Options.cpp` for pagination. Keep the new
attributes local to `PlugMode2ndOptions` and retain the existing selection
mechanism.

### 8. Main-unit-only global messages leave stale extender content

Showing "no track", "no FX", or "no selected FX" only on the anchor unit
leaves the other units on their previous displays. That is a visible state leak,
not merely a routing detail.

**Required change:** Choose and document one policy: broadcast global states to
all units, or explicitly clear/placehold every non-anchor display. Do not leave
their previous PlugMode view active.

## Additional findings

- Invalidate or initialise `lastFaderValues` and `lastVPotValues` after every
  plugin or map change, not only on the first scan after construction.
- The new project-state reader must bounds-check units, pages, and child-node
  counts. The legacy reader currently assumes the expected number of `PAGE`
  elements.
- Test page selection with sparse pages, not only contiguous pages 0 through 7.
- Add focused automated tests for page-window selection, new and legacy project
  persistence, malformed project state, and the no-main-unit anchor policy.
- The N=1 regression target should remain functional equivalence, not
  byte-identical MIDI/display traffic.

## Sound design choice

The choice to add explicit Bank/Page parameter-resolution overloads is the
right architectural foundation. It avoids temporary mutation of global cursor
state during display or control updates and gives multi-unit call sites an
unambiguous resolution path. Retain that direction after the blockers above are
addressed.
