# Project Memory

This file records durable project decisions and operational constraints. Do
not use it as a work-package log; implementation history belongs in Git and
the archived planning documents under `ai-docs/`.

## Architecture

- The CMake build is the source of truth on Linux, Windows, and macOS. The
  legacy Visual Studio projects are archived and are not maintained.
- One Reaper control-surface instance owns up to eight physical MCU-family
  units. Unit positions are dense and define the flat channel order:
  channels 1..N*8 are channel strips, while channel 0 represents the shared
  master-fader slot.
- A unit's position and its main/extender role are independent. Every main
  unit receives global controls and displays; a configuration without a main
  unit is valid and falls back to unit 0 for global output.
- `UnitConfig` stores MIDI ports, `isMain`, and `DeviceModel`. QCon ProX is a
  per-unit model capability, not a surface-wide flag. It controls the second
  display and its protocol quirks.
- `MultiDisplay` is the composite display owned by modes. It routes global
  strip fields to the corresponding unit child; callers must use a real child
  for a unit-local field such as the ProX master column.
- Strip input is translated from a unit-local channel to a flat global channel
  at the MIDI boundary. Global controls are accepted from the selected main
  unit, while strip controls are accepted from every configured unit.
- Extender support is complete. The current linked-mode behaviour is shared
  across units; independent unit modes and temporarily releasing extenders are
  possible future features, not partial implementations.

## Persistence and compatibility

- Surface configuration is global to Reaper. `KLINKE2` is the canonical format
  and stores eight fixed unit entries. Legacy five-integer strings are accepted
  and converted on the next save.
- The old ProX bit in legacy configuration is read only to infer the first
  unit's device model, then discarded. No global ProX setting is persisted.
- Project state is the `MCU_KLINKE` XML chunk. Anchors and quick-jump slots use
  absolute surface channels; invalid channels remain inactive rather than
  being clamped and overwritten.
- Plug Mode stores bank/page selection per unit and spreads pages across units
  by default. Transport bank/page controls operate as a lock-step page window;
  Control+Solo and Control+Mute cascade from the pressed unit to its right.

## Build and runtime constraints

- `VERSION.txt` is the only version source. CMake increments its build count
  during configuration, so configure only when a build will follow.
- Deploy a successful local build to REAPER's `UserPlugins` directory and fully
  restart REAPER before testing.
- The portable Linux release is built in the Debian 11 container with static
  libstdc++ and libgcc. Build old to support newer glibc systems.
- The WSL Windows build uses the Visual Studio generator, not Ninja, because
  Ninja's dependency checks are unreliable on the WSL UNC mount.
- JUCE 8 requires Apple Clang on macOS. Do not force a Homebrew GCC toolchain.
- On Linux, JUCE's manual event pump is required; macOS uses the NSRunLoop and
  must not call `dispatchNextMessageOnSystemQueue`.

## Open work

- ReaPack packaging has not been implemented.
- `PerformanceMode` remains an intentional, unreachable placeholder until a
  real Reaper performance readout is designed.
- The directory-restructure proposal in `docs/directory-restructure-plan3.md`
  remains optional future maintenance work.
