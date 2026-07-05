# Project Memory

## Decisions

- 2026-07-05: Windows build is driven from WSL via `scripts/build-windows.sh`. Key choices:
- 2026-07-05: **VS generator (MSBuild), NOT Ninja.** Ninja's stat-based dependency model breaks on the `\\wsl.localhost`/9P mount (CMake try-compile source shows up as "missing"); MSBuild tolerates the mapped drive. Generator auto-picked from the installed VS major version (16=2019, 17=2022; `MCU_VS_GENERATOR` overrides).
- 2026-07-05: **`pushd \\wsl.localhost\<distro>\...`** maps the repo UNC to a temp drive (Z:), so MSVC/rc.exe/juceaide read source straight off the WSL filesystem — no copy to /mnt/c.
- 2026-07-05: **Portable CMake 3.31.6** cached under `~/.cache/csurf-klinke-mcu/` (JUCE 8 needs >=3.22; VS2019 bundles 3.20).
- 2026-07-05: **`CMAKE_SUPPRESS_REGENERATION=ON`** silences MSB8064/65 regen-dependency warnings (unreliable over 9P); script always re-runs configure, so no cost.
- 2026-07-05: **build dir is `build_win/`** (multi-config VS generator → `build_win/Release|Debug/`), kept separate from the Linux `build/`. `.gitignore`d.
- 2026-07-05: `scripts/build-windows.sh` incremental model: the default (no flags) SKIPS the CMake configure step and only builds — configure runs once initially, or on `--clean`/`--reconfigure`. Rationale: configure is ~40s (re-runs the JUCE/juceide setup) and bumps the `VERSION.txt` build counter, so running it every build was wasteful and inconsistent with the Linux `cmake ..`-once / `cmake --build`-repeatedly model. `--reconfigure` is required after editing `CMakeLists.txt` or the source list (CMAKE_SUPPRESS_REGENERATION=ON means MSBuild will not auto-reconfigure).

## Active Context

- Windows incremental-build speed is a KNOWN LIMITATION, intentionally left as-is (2026-07-05, maintainer will revisit if it becomes a pain). In-place (WSL ext4) incremental via `scripts/build-windows.sh` is ~45s because MSBuild rebuilds ALL sources every run: its FastUpToDateCheck fails over the `\\wsl.localhost`/9P source mount (it stats thousands of JUCE/Boost headers; jittery 9P mtime + a juceaide custom step → always "not up to date"). Putting only the build outputs on NTFS does NOT help (inputs stay on UNC). Verified working: configure-skip (saves ~26s); the rebuild-all remains. The UNIMPLEMENTED path to Linux-like (~15s) incremental is a `/mnt/c` build mode: rsync deps (JUCE/Boost/reaper-sdk, ~300-400 MB) once + source (~5 MB) per build onto native NTFS and build there (rsync -a preserves mtimes → correct MSBuild incremental).

## Bugs & Fixes

- 2026-07-05: `csurf_main.cpp` (line ~201): the macOS-port load-marker block (commit 4311da3) used `__attribute__((constructor))` + `/tmp/klinke_*.txt` writes. `__attribute__` is GCC/Clang-only — MSVC parses `constructor` as an undeclared identifier (errors C2065/C4430/C2062/C2143/C2447), blocking the Windows build. Wrapped the ctor block in `#if !defined(_WIN32)` (CRLF preserved via Python edit); macOS/Linux behaviour unchanged. The `/tmp/klinke_entry.txt` writes inside `REAPER_PLUGIN_ENTRYPOINT` are harmless no-ops on Windows (fopen fails, `if(f)` guards) and were left as-is. This scaffolding should eventually be cleaned up or gated behind `MCU_DEBUG_LOG`.

## Changelog

- 2026-07-05: Added `scripts/build-windows.sh`: builds `reaper_csurf_mcu_klinke_x64.dll` from WSL using the native MSVC toolchain (VS2019) and auto-deploys it to `%APPDATA%\REAPER\UserPlugins\`. Verified: PE32+ x86-64 DLL, exports `ReaperPluginEntry`, clean build ~1m30s, incremental ~1m14s. Documented in AGENTS.md §4 ("Building from WSL"). `.gitignore` now ignores `build_win/`.

## Patterns

