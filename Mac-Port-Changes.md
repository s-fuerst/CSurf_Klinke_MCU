# Mac-Port-Changes.md — Complete change log for the macOS port

> Reference: state before this work = `cross-platform` branch HEAD (Linux
> building/working, macOS CMake branch **wired but never compiled on a Mac**).
>
> Result: `reaper_csurf_mcu_klinke.dylib` — arm64 (Apple Silicon native),
> `NOUNDEFS`, deployed to `~/Library/Application Support/REAPER/UserPlugins/`.
>
> Date: 2026-07-03 — **first ever successful macOS build**.

> Note on language: this file is in English to comply with the project's
> high-priority rule in `AGENTS.md` ("everything written into a file MUST be
> in English"). The sibling `Linux-Port-Changes.md` predates that rule.

---

## 0. Starting point

The macOS CMake branch (`elseif(APPLE)`) was already wired on 2026-06-25:
it used Clang + JUCE 8 modules, the SWELL `SWELL_dllMain` binding path
(`swell-modstub-generic.cpp` + `res_linux.cpp`), default SWELL autogen dialog
flags (no Linux-style overrides), `CMAKE_OSX_DEPLOYMENT_TARGET=10.15`, and
output a `.dylib`. It had **never been compiled on a real Mac**. This work
made it actually compile, link, and load.

---

## 1. Dominant theme: case-insensitive APFS collisions

macOS's default filesystem (APFS) is **case-insensitive but case-preserving**.
The repo root is on the `-I` include search path (so `#include "csurf.h"`
works). Any repo-root file whose name matches a system header
case-insensitively **shadows** that header. The clang warning
`non-portable path to file '<X>'; specified path differs in case from file
name on disk` is the tell-tale sign. Two such collisions were found and fixed.

### 1.1 `VERSION` → `VERSION.txt`

libc++'s `<version>` header (a real C++20 standard-library header, pulled in
transitively by JUCE 8) matched our repo-root `VERSION` file
case-insensitively. `#include <version>` from inside JUCE/libc++ resolved to
our text file instead of the system header → "invalid preprocessing
directive" cascade (our `# version bump MANUALLY ...` comment lines parsed as
C preprocessor directives).

- `git mv VERSION VERSION.txt`
- `CMakeLists.txt`: `MCU_VERSION_FILE` path → `VERSION.txt`; updated header
  comment explaining the rename.
- No behaviour change (CMake still reads/writes the version + build counter).

### 1.2 `Assert.h` → `McuAssert.h`

Our project header `Assert.h` defines the project macros `ASSERT` / `ASSERT_M`
/ `DBOUT` (and has **no include guard**). It does **not** define the standard
`assert()`. On case-insensitive APFS, every `#include <assert.h>` (angle,
from boost/libc++) and the quoted `#include "assert.h"` resolved to our file
→ the real system `assert()` was never defined → `error: use of undeclared
identifier 'assert'` cascade across boost and our own code.

- `git mv Assert.h McuAssert.h`
- All 16 `#include "Assert.h"` → `#include "McuAssert.h"`:
  `Tracks.cpp`, `Tracks.h`, `CCSManager.cpp`, `MultiTrackMeterBridge.cpp`,
  `SendReceiveMeterBridge.cpp`, `SendReceiveModeBase.cpp`, `MeterBridge.cpp`,
  `Display.cpp`, `PanMode.cpp`, `CommandMode.h`, `DisplayHandler.cpp`,
  `VPOT_LED.cpp`, `UndoEnd.cpp`, `csurf_mcu.cpp`, `PlugMap.h`,
  `ActionsDisplay.cpp`, `MultiTrackMode.cpp`.
- Two ambiguous **lowercase** `#include "assert.h"` corrected:
  - `Tracks.h:14` → `#include <cassert>` (this file uses standard `assert()`
    and already includes our header separately at line 18 for `ASSERT()`).
  - `MultiTrackMode.cpp:11` → `#include "McuAssert.h"` (this file uses only
    our `ASSERT_M` macro).

---

## 2. Boost 1.39.0 → 1.91.0 upgrade

The same rationale as the earlier JUCE 1.52 → 8 upgrade: **Boost 1.39 (2009)
does not compile under a modern libc++ / C++17 toolchain without an
ever-growing pile of patches.** Rather than keep patching, Boost was upgraded
to the latest stable (1.91.0), which compiles cleanly on all platforms. Only
headers are used (`boost/signals2.hpp`, smart pointers).

### 2.1 `fetch_deps.sh`
- Version `1.39.0` → `1.91.0`; download dir `boost_1_39_0` → `boost_1_91_0`;
  URL `https://archives.boost.io/release/1.91.0/source/boost_1_91_0.tar.bz2`.
- **Removed** the two idempotent Boost 1.39 patches that are no longer needed:
  - `shared_ptr.hpp` move-ctor bug patch
    (`BOOST_SHARED_PTR_ENABLE_MOVE_CTORS`).
  - `signals2/last_value.hpp` + `optional_last_value.hpp` patch (include
    `slot_base.hpp` so `expired_slot` is complete — GCC tolerated catching an
    incomplete type, libc++ does not).
- **Removed** the now-unused `sed_inplace()` helper (it existed only for
  those patches).

### 2.2 `CMakeLists.txt`
- `BOOST_DIR` → `boost_1_91_0`; dependency-guard `foreach` updated.
- **Removed** the `_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR` and
  `_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION` compile definitions
  from the APPLE branch (these re-enabled `std::auto_ptr` /
  `std::unary_function` / `std::binary_function` that modern Boost no longer
  needs).

### 2.3 `AGENTS.md`
- Tech-stack table, prerequisites (§2) and revival context updated to Boost
  1.91.0 (with a note that 1.39 did not compile under modern libc++/C++17).

---

## 3. Build-system changes (`CMakeLists.txt`, APPLE branch)

### 3.1 Force Apple Clang on macOS (compiler guard before `project()`)
A Homebrew GCC is commonly set as the default compiler via the `CC` env var
(e.g. `CC=/opt/homebrew/bin/gcc-15`). CMake then used GCC for C but Apple
Clang for C++, and JUCE's `juceaide` helper (built at configure time) failed
because GCC rejects Clang-only warning flags (`-Wshadow-all`,
`-Wshorten-64-to-32`, …). Added a guard before `project()` that detects
`uname -s == Darwin` and forces `clang` / `clang++` for both languages
unless the user explicitly passes `-DCMAKE_C_COMPILER=`.

### 3.2 Scope `-std=c++17` to C++ only
JUCE registers C sources onto our target (notably
`juce_graphics_Sheenbidi.c`). The APPLE branch's `-std=c++17` was applied to
those `.c` files too, which is a hard error in Clang (`-std=c++17 not allowed
with 'C'`). GCC/Linux silently ignored it (latent bug masked on Linux).
Fixed with the generator expression
`$<$<COMPILE_LANGUAGE:CXX>:-std=c++17>`.

### 3.3 SWELL stub: `swell-modstub-generic.cpp` → `swell-modstub.mm` (2026-07-04)

The original macOS branch used `swell-modstub-generic.cpp` (the Linux
libSwell.so model), which binds SWELL function pointers via the `getfunc`
parameter passed to `SWELL_dllMain`. On macOS REAPER calls `SWELL_dllMain` as
a load probe with `getfunc=NULL` — it never passes the real SWELL function
getter through this path. Result: every SWELL function pointer
(`DialogBox`, `CreateWindow`, …) remained NULL → `configFunc` crashed
with `SIGSEGV` at address 0x0 when REAPER tried to open the surface
configuration dialog (`CSurf_ShowConfig` → our `configFunc` → NULL deref).

**Fix: compile the macOS-specific stub `swell-modstub.mm` instead.** This
stub contains a global `SwellAPPInitializer` instance whose constructor runs
at dylib load time and binds all SWELL function pointers by asking REAPER's
`NSApp` delegate for its SWELL API getter:

```objc
id del = [NSApp delegate];
if (del && [del respondsToSelector:@selector(swellGetAPPAPIFunc)])
    SWELLAPI_GetFunc = send_msg(del, @selector(swellGetAPPAPIFunc));
```

This is the **canonical macOS SWELL plugin mechanism** — REAPER, being a
SWELL application, exposes its SWELL implementation through the Cocoa app
delegate. The stub still exports `SWELL_dllMain` (returning 1) so REAPER's
load probe succeeds, but the real binding is done by the constructor.

**CMake details:**
- Switched `target_sources` from `${SWELL_DIR}/swell-modstub-generic.cpp` to
  `${SWELL_DIR}/swell-modstub.mm`.
- Applied `set_source_files_properties(LANGUAGE CXX)` on the `.mm` file so
  `clang++` compiles it as Objective-C++ without needing the `OBJCXX` project
  language enabled (we only `project(… C CXX)`).
- **Removed** the `-include;cstddef` hack that was local to the generic stub
  (the `.mm` does not wrap `swell.h` in `extern "C"`, so the `std::byte` /
  `extern "C"` conflict does not arise).
- The generic stub remains in use on Linux (where `SWELL_LOAD_SWELL_DYLIB`
  dlopens `libSwell.so`).

### 3.4 macOS specifics (wired earlier, unchanged or lightly touched)
For reference, the macOS branch differs from Linux as:
- SWELL: the `.mm` stub binds via NSApp delegate (see §3.3);
  no `SWELL_LOAD_SWELL_DYLIB`, no `dlopen`, no `SWELL_curmodule_cursorresource_head`.
- Default SWELL autogen dialog flags (`FLIPPED|NOAUTOSIZE`) — no
  style/scale overrides (Linux needs them).
- No explicit system libraries — macOS frameworks (Cocoa, IOKit, Metal,
  QuartzCore, …) are linked automatically by `juce::juce_gui_basics`.
- Output: `reaper_csurf_mcu_klinke.dylib`.

---

## 4. Clang-strictness source fixes (GCC only warned)

These are all places where GCC treated the construct as a warning but Clang
treats it as an error.

### 4.1 Redundant class-scope qualification (`-Wextra-qualification`)
Removed the redundant `ClassName::` qualifier on members defined inside their
own class body.
- `csurf_mcu.h`: `CSurf_MCU::GetTypeString`, `GetDescString`, `GetConfigString`
  → unqualified.
- `Transport.h`: `Transport::Transport` (ctor), `setClient`, `startReel`,
  `endReel` → unqualified.

### 4.2 Brace-init narrowing (`int` → `unsigned char`)
- `Actions.cpp:120-121`: `MIDI_event_t` initializer used
  `pAction->getButtonId()` (returns `int`) in a `unsigned char` field.
  Added `static_cast<unsigned char>(...)`.

### 4.3 Pointer-to-`int` truncation (loses information on 64-bit arm64)
`GetSetMediaTrackInfo()` returns `void*`. A direct `(int)` cast is an error
on 64-bit. Cast through `intptr_t` (the track number fits in `int`):
- `MultiTrackMode.cpp:180`
- `Tracks.cpp:339`, `Tracks.cpp:362`

### 4.4 Address of a temporary (`-Waddress-of-temporary`)
- `PlugMode.cpp:1170-1172`: `&PlugAccess::ElementDesc(bank, page, …)` took the
  address of a temporary. Replaced with named locals
  (`faderDesc`, `vpotDesc`) whose address is passed.

### 4.5 Ambiguous `String + char[]` overload
- `PlugModeParamComponent.cpp:190`: `String(...) + paramName` (`char[80]`) was
  ambiguous against JUCE String overloads under Clang. Made explicit with
  `+ String(paramName)`.

---

## 5. JUCE event pump — Linux only

`CSurf_MCU::Run()` called `juce::detail::dispatchNextMessageOnSystemQueue(true)`
in a loop to drain JUCE's X11 event queue (so on-screen editor windows paint
and respond). This is the Linux/X11 manual pump (Rothchild fix #4).

In **JUCE 8**, `dispatchNextMessageOnSystemQueue` is defined **only** in
`juce_Messaging_linux.cpp` and `juce_Messaging_windows.cpp` — there is **no
macOS implementation**. Linking therefore failed with an undefined symbol on
macOS.

On macOS, JUCE dispatches messages through the native `NSRunLoop`, which is
driven by REAPER's main thread, so no manual pump is needed. Guarded the call
with `#if JUCE_LINUX` (the forward declaration near the top of
`csurf_mcu.cpp` is left in place — an unreferenced declaration causes no link
error).

---

## 6. `res.rc_mac_menu`

`csurf_main.cpp` includes `res.rc_mac_menu` (the SWELL menu resource) on
macOS. The file was missing. Recreated as the documented empty placeholder
(`\n//EOF\n`). It is gitignored (generated SWELL artefact), so it is created
in-tree rather than committed.

---

## 7. `fetch_deps.sh` — portable "next steps" output

The final "next steps" block used to print Linux (`apt install …`) instructions
unconditionally. It now detects the OS via `uname -s` and prints the matching
instructions: macOS (`xcode-select --install`, `brew install cmake`,
`sysctl -n hw.ncpu`, deploy to `~/Library/Application Support/REAPER/...`)
vs Linux.

---

## 8. Files changed

| File | Reason |
|------|--------|
| `CMakeLists.txt` | Boost dir; compiler guard; `-std=c++17` C++-only; SWELL `.mm` stub switch; removed `-include cstddef` + libc++ macros |
| `fetch_deps.sh` | Boost 1.91.0; removed patches + `sed_inplace`; OS-aware next steps |
| `AGENTS.md` | Boost 1.91.0 references |
| `VERSION` → `VERSION.txt` | APFS `<version>` collision (rename) |
| `Assert.h` → `McuAssert.h` | APFS `<assert.h>` collision (rename) |
| `Actions.cpp` | `static_cast<unsigned char>` narrowing fix |
| `MultiTrackMode.cpp` | `McuAssert.h` include; `(int)(intptr_t)` cast |
| `Tracks.cpp` | `McuAssert.h` include; `(int)(intptr_t)` casts (×2) |
| `Tracks.h` | `<cassert>` + `McuAssert.h` includes |
| `PlugMode.cpp` | address-of-temporary → named locals |
| `PlugModeParamComponent.cpp` | `+ String(paramName)` ambiguity fix |
| `csurf_mcu.h` | removed extra qualification (×3) |
| `csurf_mcu.cpp` | `McuAssert.h` include; `dispatchNextMessageOnSystemQueue` `#if JUCE_LINUX` |
| `Transport.h` | removed extra qualification (×4) |
| `res.rc_mac_menu` | recreated (empty SWELL placeholder) |
| `reaper-sdk/WDL/swell/swell-modstub-generic.cpp` | removed from macOS build (replaced by swell-modstub.mm) |
| `CCSManager.cpp`, `MultiTrackMeterBridge.cpp`, `SendReceiveMeterBridge.cpp`, `SendReceiveModeBase.cpp`, `MeterBridge.cpp`, `Display.cpp`, `PanMode.cpp`, `CommandMode.h`, `DisplayHandler.cpp`, `VPOT_LED.cpp`, `UndoEnd.cpp`, `PlugMap.h`, `ActionsDisplay.cpp` | `#include "Assert.h"` → `#include "McuAssert.h"` |

### Net effect
- **1 dependency upgraded** (Boost 1.39 → 1.91.0), **2 dependency patches
  removed** (shared_ptr, signals2).
- **2 files renamed** for APFS case-collision safety.
- **2 build-system additions** (compiler guard, SWELL `-include cstddef`),
  **1 build-system fix** (`-std=c++17` C++-only), **2 build-system removals**
  (libc++ macros).
- Result: first working macOS build (arm64, `NOUNDEFS`).

---

## 9. Known / remaining items

1. **Surface configuration dialog works (2026-07-04)** — the `SIGSEGV` in
   `configFunc` (swell-modstub-generic.cpp's NULL SWELL pointers) was fixed
   by switching to `swell-modstub.mm` (§3.3). The dialog opens correctly.
2. **Full hardware integration** — not yet tested with a physical MCU device
   on macOS. The CSurf_MCU constructor should be fine (CoreMIDI, no
   PipeWire/JACK timing issues). The JUCE on-screen editor windows rely on
   the native `NSRunLoop` (no manual pump); if they do not paint/respond
   correctly inside REAPER, the event dispatch may need revisiting.
3. **`readme.txt` / `Linux-Port-Changes.md`** still mention Boost 1.39 as the
   historical record; they were left untouched (history).
4. **Windows CMake branch** — still not compiled on a Windows host (separate
   effort; the Boost upgrade benefits it too).
5. **Auto-incrementing build counter** still bumps on every `cmake` configure
   (per the standing TODO to remove it once all platforms are tested).
6. **`MCU_DEBUG_LOG`** defaults ON (should default OFF for Release).
