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

## Patterns

- User can run sudo commands when requested — just tell them what to
  run and they'll execute it. No sudo password prompt needed from
  agent side.
- Version source = VERSION.txt file (repo root), NOT
  hard-coded. Format "<version> <build-count>". Bump version manually
  (reset count); count auto-increments per `cmake`
  configure. Generated build/Version.h → MCU_VERSION_STRING in
  csurf_mcu.h GetDescString. AGENTS.md §4 documents it.
- Deploy is the AGENT's job, not CMake's: after a successful Linux
  build, `cp build/reaper_csurf_mcu_klinke.so
  ~/.config/REAPER/UserPlugins/`. No CMake auto-deploy (keeps
  CI/Windows/macOS clean). Reaper needs full restart to reload.
- Logging architecture: MCU_DEBUG_LOG = standalone CMake OPTION
  (default ON), NOT tied to CMAKE_BUILD_TYPE → currently EVERY build
  logs, including Release. When OFF, MCU_LOG(...) compiles to
  ((void)0) (see src/core/McuDebugLog.h #else) → zero runtime cost. 16
  call sites in 6 files; most verbose: sendToHardware logs every text
  write (ROW0/ROW1 snd). Release goal: default OFF, ON only for debug
  builds.
- Build-flow rule (always run both steps, otherwise no build-count
  increment): `cd build && cmake .. -DCMAKE_BUILD_TYPE=Release &&
  cmake --build . -- -j$(nproc)`. The increment happens ONLY in the
  configure step (`cmake ..`), NOT in the build step. Running only
  `cmake --build` (incremental) without `cmake ..` first produces a
  build without an incremented counter. Mnemonic: "configure =
  increments, build = links".
- Commit messages always in English (user requirement).
- No commits without explicit user instruction (user requirement).
- Always write links out in full (https://...), not as Markdown
  hyperlinks [text](URL) — the user cannot click Markdown links.
- HIGH-PRIORITY LANGUAGE RULE: the dialog with the user may be in
  German, but EVERYTHING written into a file (source, comments,
  strings, logs, commit messages, docs, AGENTS.md, manual, MEMD.md,
  generated code, configs, scripts) MUST be in English. Never write
  German text into a file, not even comments. Anchored at the very top
  of AGENTS.md as "⚠️ Language rule (HIGH PRIORITY)".
- macOS case-insensitivity hazard: project files at repo root (on the
  -I path) collide case-insensitively with system headers on
  APFS. Found: VERSION→<version> (libc++), Assert.h→<assert.h>. The
  clang -Wnonportable-include-path warning ("differs in case from file
  name on disk") is the tell. Mitigation: unique file names
  (VERSION.txt, McuAssert.h). Watch for new ones whenever a new
  root-level header is added.
- macOS build: NEVER set CC to Homebrew gcc — JUCE 8 juceaide needs
  Apple Clang. CMakeLists.txt forces Clang on Darwin. Override with
  -DCMAKE_C_COMPILER if ever needed.
- macOS JUCE event dispatch is automatic via NSRunLoop (driven by
  REAPER main thread); do NOT call dispatchNextMessageOnSystemQueue on
  macOS (no impl in JUCE 8). Linux/X11 needs the manual pump (#if
  JUCE_LINUX in csurf_mcu.cpp Run()).
- macOS SWELL stub: each platform has a DIFFERENT swell-modstub
  variant. macOS = swell-modstub.mm (NSApp delegate constructor),
  Linux = swell-modstub-generic.cpp (SWELL_LOAD_SWELL_DYLIB dlopens
  libSwell.so, OR non-dlopen path with SWELL_dllMain getfunc), Windows
  = none (native Win32). Never use the generic stub on macOS; REAPER
  does not pass the getfunc through SWELL_dllMain there.
- Portable Linux build gotchas (docker/release-linux.Dockerfile, all
  container-scoped): (1) JUCE 8 needs cmake ≥3.22 but Debian 11 ships
  3.18 → install the cmake.org binary tarball. (2) juceaide (JUCE's
  codegen helper) is built by a RECURSIVE cmake subprocess whose
  PASSTHROUGH_ARGS does NOT include CMAKE_C[XX]_FLAGS — so forcing
  -I/usr/include/freetype2 via CMAKE_CXX_FLAGS does NOT reach
  juceaide. (3) juceaide compiles juce_graphics.cpp which needs
  <ft2build.h> (flat header under /usr/include/freetype2/) and
  <fontconfig/fontconfig.h>. Fix: symlink /usr/include/ft2build.h →
  freetype2/ft2build.h AND /usr/include/freetype →
  freetype2/freetype. Plus install libfontconfig1-dev. (4) Debian gcc
  enables -Wl,--as-needed by default → drops unused -l libs from
  NEEDED (X11 gets dropped; harmless — JUCE loads X11 via dlopen). (5)
  Need libxcursor/xinerama/xrandr/xrender/xcomposite-dev +
  libglu1-mesa-dev for JUCE pkg-config module deps. (6) Static
  libstdc++.a on Debian 11 is built with -fPIC, so -static-libstdc++
  works for a SHARED lib.
- TOOL BUG: the `memd_write` tool with mode "append" OVERWRITES THE
  ENTIRE FILE instead of appending to a section (observed 2026-07-21:
  MEMD.md went 198→17 lines on a single append). Do NOT trust
  memd_write for this project — use the `edit` tool to insert entries
  at the right section anchors, or `bash` `cat >>` / `printf
  >>`. Always `git checkout HEAD -- MEMD.md` to restore if a
  memd_write call truncates it.
- Debugging freezes / hangs: since ptrace requires root on this system
  (kernel.yama.ptrace_scope=1), the GDB backtrace command is `sudo gdb
  -p $(pidof reaper) -batch -ex "thread apply all bt"`. This MUST be
  run in the user's terminal where sudo password entry works. The
  agent cannot drive this through the bash tool.
- Reaper GUI needs GDK_BACKEND=x11 on this system (Wayland
  host). Start with: `GDK_BACKEND=x11
  /home/fuerst/opt/REAPER/reaper`. For GDB: `GDK_BACKEND=x11 gdb -ex
  run --args /home/fuerst/opt/REAPER/reaper` — but GDB must be
  interactive (user's terminal) since ptrace requires sudo.
- CRITICAL: NEVER attempt `sudo` commands via the bash tool. The agent
  has no access to the user's password and repeated failed sudo
  attempts lock the user's account. When root privileges are needed
  (e.g. `gdb -p` due to ptrace_scope=1), instruct the user to run the
  command in their own terminal.
- ASan (AddressSanitizer) workflow: `rm -rf build_asan && mkdir
  build_asan && cd build_asan && cmake .. -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address" && cmake --build
  . -j$(nproc) && cp reaper_csurf_mcu_klinke.so
  ~/.config/REAPER/UserPlugins/ && LD_PRELOAD=/usr/lib/libasan.so
  GDK_BACKEND=x11 timeout 15 /home/fuerst/opt/REAPER/reaper 2>&1 | tee
  /tmp/reaper_asan.log`. Buffer overflows are detected during init —
  no user interaction needed.


## Open work

- ReaPack packaging has not been implemented.
- `PerformanceMode` remains an intentional, unreachable placeholder until a
  real Reaper performance readout is designed.
- The directory-restructure proposal in `docs/directory-restructure-plan3.md`
  remains optional future maintenance work.
