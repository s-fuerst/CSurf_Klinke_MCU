cmake_minimum_required(VERSION 3.10)

# On macOS, JUCE 8 must be built with Apple Clang. A Homebrew GCC is often set
# as the default via the CC env var (e.g. CC=/opt/homebrew/bin/gcc-15), which
# breaks the juceaide helper build with Clang-only warning flags
# (-Wshadow-all, -Wshorten-64-to-32, ...). Force Clang on macOS unless the
# user explicitly passed -DCMAKE_C_COMPILER / -DCMAKE_CXX_COMPILER.
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
  execute_process(
    COMMAND uname -s
    OUTPUT_VARIABLE _mcu_host_os
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_mcu_host_os STREQUAL "Darwin")
    set(CMAKE_C_COMPILER   clang)
    set(CMAKE_CXX_COMPILER clang++)
    message(STATUS "macOS detected: forcing Apple Clang for C/C++ (override with -DCMAKE_C_COMPILER=...)")
  endif()
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  CSurf_Klinke_MCU — cross-platform CMake build
#
#  Status:
#    • Linux   — build working (GCC, X11, JUCE 8, SWELL).
#    • macOS   — build wired (Clang, Cocoa/Metal, JUCE 8, SWELL).
#    • Windows — build wired (MSVC, native Win32, res.rc, JUCE 8).
#
#  Dependencies (gitignored; fetch with ./scripts/fetch_deps.sh, see AGENTS.md §2):
#    juce_8/      reaper-sdk/sdk/  reaper-sdk/WDL/  boost_1_91_0/
#
#  JUCE 8 uses the module build (add_subdirectory). Boost 1.91 is header-only.
#  Resulting binary is AGPLv3 (JUCE 8 made all modules AGPLv3/JUCE dual).
# ─────────────────────────────────────────────────────────────────────────────

# --- macOS deployment target -------------------------------------------------
# MUST be set BEFORE project(): CMake 4.x no longer applies a CACHE set() that
# runs after project() — the cache entry silently stays empty and the linker
# falls back to the HOST macOS version (e.g. minos 26.0 on macOS 26), which
# would make the plugin unloadable on all older macOS.
# FORCE is required: with a pre-existing EMPTY cache entry (build dirs created
# before this fix, or by CMake 4.x's empty default), a plain CACHE set() is a
# silent no-op. The guard below still lets an explicit
# -DCMAKE_OSX_DEPLOYMENT_TARGET=... always win (non-empty value -> skipped).
# 10.15 (Catalina) is the project's chosen minimum (JUCE 8 itself supports
# deployment back to 10.11, see juce_8/README.md "Deployment Targets").
if(CMAKE_HOST_APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING
            "Minimum macOS version the plugin supports (10.15 = Catalina)" FORCE)
    endif()
endif()

project(reaper_csurf_mcu_klinke C CXX)

# --- Paths --------------------------------------------------------------------
set(SRC_DIR   ${CMAKE_CURRENT_SOURCE_DIR})
set(JUCE_DIR  ${SRC_DIR}/juce_8)
set(SDK_DIR   ${SRC_DIR}/reaper-sdk/sdk)
set(WDL_DIR   ${SRC_DIR}/reaper-sdk/WDL)
set(SWELL_DIR ${WDL_DIR}/swell)
set(BOOST_DIR ${SRC_DIR}/boost_1_91_0)

# --- Guard: bail early with a clear message if deps are missing --------------
foreach(d juce_8 reaper-sdk boost_1_91_0)
    if(NOT EXISTS "${SRC_DIR}/${d}")
        message(FATAL_ERROR
            "\n  Dependency '${d}/' not found.\n"
            "  Run ./scripts/fetch_deps.sh first (see AGENTS.md, §2 Prerequisites).\n")
    endif()
endforeach()

# --- Options ------------------------------------------------------------------
option(MCU_DEBUG_LOG "Write button/window debug log in Debug builds to <REAPER_RESOURCE>/mcu_klinke_debug.log" ON)
option(MCU_KLINKE_BUILD "Enable Klinke's personal developer features (KLINKE preprocessor flag)" OFF)

# =============================================================================
#  Versioning — VERSION.txt at the repo root holds "<version> <build-count>".
#
#  • version       (e.g. 0.9.1.3): bumped MANUALLY for a release; when bumped,
#                  reset build-count to 1.
#  • build-count : edited MANUALLY only. The number stored in the file is used
#                  verbatim and compiled into the binary as "... build N".
#
#  CMake never rewrites VERSION.txt — it only reads it and generates the header
#  ${CMAKE_BINARY_DIR}/Version.h (included by csurf_mcu.h). To change the
#  version or build number, edit VERSION.txt by hand before configuring.
#
#  NOTE: the file is named VERSION.txt (not VERSION) to avoid colliding with
#  libc++'s <version> header on macOS's case-insensitive APFS filesystem,
#  where the repo root is on the include search path.
# =============================================================================
set(MCU_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION.txt")
if(NOT EXISTS "${MCU_VERSION_FILE}")
    file(WRITE "${MCU_VERSION_FILE}" "0.9.1.3 0\n")
endif()
file(READ "${MCU_VERSION_FILE}" _vraw)
string(REGEX REPLACE "\r" "" _vraw "${_vraw}")
# Find "<version> <count>" directly via regex — robust against comment lines,
# semicolons inside comments, and stray whitespace. Ignores everything else.
string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)[ \t]+([0-9]+)" _m "${_vraw}")
set(MCU_VERSION "${CMAKE_MATCH_1}")
set(_vc "${CMAKE_MATCH_2}")
if(MCU_VERSION STREQUAL "")
    set(MCU_VERSION "0.9.1.3")
    set(_vc "0")
endif()
# manual-only model: the count stored in the file is used verbatim; CMake never
# writes it back.
set(MCU_BUILD_COUNT "${_vc}")
set(MCU_VERSION_DISPLAY "v${MCU_VERSION} build ${MCU_BUILD_COUNT}")
message(STATUS "MCU version: ${MCU_VERSION_DISPLAY}")
# generate the header consumed by csurf_mcu.h
configure_file("${CMAKE_SOURCE_DIR}/Version.h.in" "${CMAKE_BINARY_DIR}/Version.h" @ONLY)

# =============================================================================
#  JUCE 8 (modern, module build) — added as a subdirectory and linked as
#  juce::juce_gui_basics. That module pulls in juce_core / juce_events /
#  juce_graphics / juce_data_structures transitively. EXCLUDE_FROM_ALL keeps
#  the many JUCE targets we do not link from being built by default.
#
#  Module feature options are set as cache variables BEFORE add_subdirectory so
#  JUCE picks them up. We turn off only what the plugin does not use or what is
#  heavyweight on the build host; defaults are otherwise accepted.
# =============================================================================
set(JUCE_WEB_BROWSER        OFF CACHE BOOL "" FORCE)
set(JUCE_USE_CDBURNER       OFF CACHE BOOL "" FORCE)
set(JUCE_USE_CDREADER       OFF CACHE BOOL "" FORCE)
set(JUCE_QUICKTIME          OFF CACHE BOOL "" FORCE)  # gone on modern macOS anyway
set(JUCE_PLUGINHOST_VST     OFF CACHE BOOL "" FORCE)
set(JUCE_PLUGINHOST_VST3    OFF CACHE BOOL "" FORCE)
set(JUCE_PLUGINHOST_AU      OFF CACHE BOOL "" FORCE)
set(JUCE_PLUGINHOST_LADSPA  OFF CACHE BOOL "" FORCE)
set(JUCE_PLUGINHOST_LV2     OFF CACHE BOOL "" FORCE)
set(JUCE_USE_FLAC           OFF CACHE BOOL "" FORCE)
set(JUCE_USE_OGGVORBIS      OFF CACHE BOOL "" FORCE)
set(JUCE_USE_CAMERA         OFF CACHE BOOL "" FORCE)
add_subdirectory(${JUCE_DIR} juce_8_build EXCLUDE_FROM_ALL)

# =============================================================================
#  Plugin target
# =============================================================================
add_library(reaper_csurf_mcu_klinke SHARED)

# Include-path strategy: add every src/ subdirectory explicitly so flat-name
# #includes (e.g. "csurf_mcu.h", "PlugMode.h") keep resolving unchanged.
# vendor/ and resources/ are added explicitly (csurf.h #includes resource.h;
# res_linux.cpp #includes res.rc_mac_dlg).
target_include_directories(reaper_csurf_mcu_klinke PRIVATE
    ${CMAKE_BINARY_DIR}   # generated Version.h (MCU_VERSION_STRING)
    ${SRC_DIR}/src
    ${SRC_DIR}/src/core
    ${SRC_DIR}/src/ui
    ${SRC_DIR}/src/hardware
    ${SRC_DIR}/src/hardware/display
    ${SRC_DIR}/src/action
    ${SRC_DIR}/src/action/editor
    ${SRC_DIR}/src/modes/commands
    ${SRC_DIR}/src/modes/commands/editor
    ${SRC_DIR}/src/modes/multitrack
    ${SRC_DIR}/src/modes/multitrack/editor
    ${SRC_DIR}/src/modes/sends
    ${SRC_DIR}/src/modes/plugin
    ${SRC_DIR}/src/modes/plugin/editor
    ${SRC_DIR}/src/modes/channelstrip
    ${SRC_DIR}/src/modes/channelstrip/editor
    ${SRC_DIR}/vendor     # checked-in vendored headers (csurf.h, reaper_plugin_functions.h @v5.92)
    ${SRC_DIR}/resources  # dialog resources (resource.h, res.rc_mac_dlg)
    ${JUCE_DIR}
    ${SDK_DIR}
    ${WDL_DIR}      # ptrlist.h, ...
    ${SWELL_DIR}    # swell.h, ...
    ${BOOST_DIR})   # boost/signals2.hpp, ...

target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE
    TIXML_USE_TICPP
    NDEBUG
    BOOST_BIND_GLOBAL_PLACEHOLDERS  # suppress Boost 1.91 deprecation notes for global _1,_2
    JUCE_MODAL_LOOPS_PERMITTED=1   # enable synchronous AlertWindow/DialogWindow APIs
    NOMINMAX                     # stop SWELL's min/max macros breaking <limits>
    WDL_NO_DEFINE_MINMAX)
# SWELL_PROVIDED_BY_APP only on !WIN32 (REAPER provides SWELL on Linux/macOS; Windows uses native Win32)
if(NOT WIN32)
    target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE SWELL_PROVIDED_BY_APP)
endif()

if(MCU_DEBUG_LOG)
    target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE
        $<$<CONFIG:Debug>:MCU_DEBUG_LOG>)
endif()

# Optional: build the Release DLL with debug symbols (a .pdb next to the DLL)
# so the Visual Studio CPU profiler can resolve surface function names. OFF
# by default — a normal Release build stays lean (~4 MB, no PDB). The
# build-windows-from-wsl.sh --profiler switch toggles this and forces a
# reconfigure automatically; rebuild without the flag to go lean again.
# (See ai-docs/add-track-perf-investigation.md.)
option(MCU_PROFILER "Build Release with debug symbols (PDB) for CPU profiling" OFF)
if(WIN32 AND MCU_PROFILER)
    target_compile_options(reaper_csurf_mcu_klinke PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(reaper_csurf_mcu_klinke PRIVATE $<$<CONFIG:Release>:/DEBUG>)
endif()

if(MCU_KLINKE_BUILD)
    target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE KLINKE)
endif()

# Mark debug builds (CMAKE_BUILD_TYPE == Debug) so the surface-edit dialog can
# append a " d" to the version label. NDEBUG is defined unconditionally above,
# so the build type cannot be detected from the standard _DEBUG/NDEBUG macros —
# it must come from CMake. (Single-config generators only: Make/Ninja, the
# primary path on Linux, macOS, and WSL.)
string(TOUPPER "${CMAKE_BUILD_TYPE}" _mcu_build_type_upper)
if(_mcu_build_type_upper STREQUAL "DEBUG")
    target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE MCU_DEBUG_BUILD)
endif()

# --- Source files — GLOB_RECURSE all .cpp under src/ with CONFIGURE_DEPENDS.
# res_linux.cpp is excluded from the glob (added conditionally per platform).
# Platform-specific sources are appended below per-branch.
file(GLOB_RECURSE MCU_SOURCES CONFIGURE_DEPENDS ${SRC_DIR}/src/*.cpp)
list(FILTER MCU_SOURCES EXCLUDE REGEX "res_linux\\.cpp$")
target_sources(reaper_csurf_mcu_klinke PRIVATE ${MCU_SOURCES})

# JUCE 8 module (pulls core/events/graphics/data_structures transitively).
target_link_libraries(reaper_csurf_mcu_klinke PRIVATE
    juce::juce_gui_basics)

# =============================================================================
#  Platform-specific flags / system libs / output name
# =============================================================================
if(UNIX AND NOT APPLE)
    # --- Linux (baseline) ----------------------------------------------------
    target_sources(reaper_csurf_mcu_klinke PRIVATE
        ${SWELL_DIR}/swell-modstub-generic.cpp   # binds SWELL funcs via REAPER
        src/res_linux.cpp)                       # SWELL dialog resources
    # C++17 is the minimum for JUCE 8.
    # -fpermissive kept: the Jucer 1.11-generated *Component files are loose
    # about conversions; we will tighten this as the upgrade progresses.
    target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE
        SWELL_LOAD_SWELL_DYLIB   # bind SWELL from REAPER's libSwell.so at load
        # res.rc_mac_dlg auto-generates FLIPPED|NOAUTOSIZE dialog flags that yield a
        # 0x0 dialog on Linux. Override them with a plain WS_CHILD style (=1).
        # Leave the scale unspecified so SWELL's generic Linux backend applies its
        # standard 1.9 dialog scale; at 1.0 the native controls are too cramped.
        "SET_IDD_SURFACEEDIT_MCUMAIN_STYLE=1"
        "SET_IDD_SURFACEEDIT_MCUEX_STYLE=1"
        )
    target_compile_options(reaper_csurf_mcu_klinke PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -fPIC -fvisibility=hidden
        $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations> $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-result>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-sign-compare> $<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder> $<$<COMPILE_LANGUAGE:CXX>:-Wno-multichar>)
    # REAPER SDK's MIDI_event_t::midi_message[4] is documented to accept more
    # than 4 bytes (SysEx). Silence the false-positive -Wstringop-overflow
    # that GCC emits when writing past index 3 of the 4-element array.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(
            ${SRC_DIR}/src/core/csurf_mcu.cpp
            ${SRC_DIR}/src/hardware/display/DisplayHandler.cpp
            ${SRC_DIR}/src/hardware/HardwareUnit.cpp
            PROPERTIES COMPILE_FLAGS "-Wno-stringop-overflow")
    endif()

    target_link_libraries(reaper_csurf_mcu_klinke PRIVATE X11 Xext pthread dl curl)
    set_target_properties(reaper_csurf_mcu_klinke PROPERTIES
        PREFIX "" OUTPUT_NAME "reaper_csurf_mcu_klinke" SUFFIX ".so")

elseif(WIN32)
    # --- Windows (MSVC, x64) ------------------------------------------------
    # Builds reaper_csurf_mcu_klinke_x64.dll with MSVC toolset.
    # The native res.rc is compiled by the MSVC resource compiler; the SWELL
    # dialog shims (res_linux.cpp / swell-modstub-generic.cpp) are not needed.
    target_sources(reaper_csurf_mcu_klinke PRIVATE resources/res.rc)
    target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE
        _CRT_SECURE_NO_DEPRECATE)  # suppress strcpy/... deprecation (C4996)
    target_compile_options(reaper_csurf_mcu_klinke PRIVATE
        /std:c++17 /MP /EHsc /W3
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2>)
    # winmm.lib is needed for timeGetTime() in csurf_mcu.cpp, Transport.cpp, ButtonManager.cpp.
    # JUCE 8 modules pull user32/gdi32/comctl32 etc. transitively via juce::juce_gui_basics.
    target_link_libraries(reaper_csurf_mcu_klinke PRIVATE winmm)
    set_target_properties(reaper_csurf_mcu_klinke PROPERTIES
        PREFIX "" OUTPUT_NAME "reaper_csurf_mcu_klinke_x64" SUFFIX ".dll")

elseif(APPLE)
    # --- macOS ----------------------------------------------------------------
    target_sources(reaper_csurf_mcu_klinke PRIVATE
        ${SWELL_DIR}/swell-modstub.mm   # macOS SWELL binding (see note below)
        src/res_linux.cpp)              # SWELL dialog resources
    # Force the .mm through the CXX compiler: project() only enables C/CXX, so
    # CMake has no OBJCXX language to map a .mm onto. clang++ compiles .mm as
    # Objective-C++ by extension, so LANGUAGE CXX routes it there correctly
    # without us having to enable OBJCXX project-wide.
    set_source_files_properties(${SWELL_DIR}/swell-modstub.mm PROPERTIES LANGUAGE CXX)

    # Clang / Apple SDK. JUCE 8 provides native Cocoa + Metal backends.
    # macOS framework dependencies (Cocoa, IOKit, Metal, QuartzCore, etc.) are
    # linked automatically by juce::juce_gui_basics via JUCE module metadata —
    # nothing to list here.

    # SWELL on macOS: REAPER is itself a SWELL application. swell-modstub.mm
    # contains a global SwellAPPInitializer whose constructor runs at dylib
    # load time and binds every SWELL API function pointer by asking REAPER's
    # NSApp delegate for its SWELLAPI_GetFunc ([del swellGetAPPAPIFunc]).
    # This is why SWELL works even though REAPER calls our SWELL_dllMain with a
    # NULL getfunc (it only calls SWELL_dllMain as a load probe; the real
    # binding happens through the NSApp delegate in that constructor).
    #
    # NOTE: do NOT use swell-modstub-generic.cpp here. That stub is the Linux
    # libSwell.so model — it binds SWELL via the getfunc passed to
    # SWELL_dllMain, which REAPER leaves NULL on macOS, leaving every SWELL
    # function pointer NULL and crashing on the first dialog (configFunc).
    #
    # No dialog style/scaling overrides needed on macOS: the default
    # FLIPPED|NOAUTOSIZE autogen flags are correct (unlike Linux).
    target_compile_options(reaper_csurf_mcu_klinke PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -fPIC -fvisibility=hidden
        -Wno-deprecated-declarations -Wno-unused-result
        -Wno-sign-compare -Wno-reorder -Wno-multichar)
    set_target_properties(reaper_csurf_mcu_klinke PROPERTIES
        PREFIX "" OUTPUT_NAME "reaper_csurf_mcu_klinke" SUFFIX ".dylib")

else()
    message(FATAL_ERROR "Unsupported platform for the CMake build.")
endif()
