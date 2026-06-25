cmake_minimum_required(VERSION 3.10)

# ─────────────────────────────────────────────────────────────────────────────
#  CSurf_Klinke_MCU — cross-platform CMake build
#
#  Status:
#    • Linux   — baseline build working (GCC, X11/JUCE 1.52, SWELL).
#    • Windows — NOT yet wired into CMake (the .vcxproj remains the source of
#                truth until the WIN32 branch below is implemented; next
#                milestone after the Linux baseline is confirmed).
#    • macOS   — NOT yet wired into CMake (planned after Windows).
#
#  Dependencies (gitignored; fetch with ./fetch_deps.sh, see AGENTS.md §2):
#    juce_8/      reaper-sdk/sdk/  reaper-sdk/WDL/  boost_1_39_0/
#
#  The same dependency versions as the original VS project are kept
#  (Boost 1.39). JUCE is upgraded from 1.52 to 8 on this EXPERIMENTAL
#  juce-upgrade branch (module build via add_subdirectory). Resulting binary
#  is AGPLv3 (JUCE 8 made all modules AGPLv3/JUCE dual). Branch may be abandoned.
# ─────────────────────────────────────────────────────────────────────────────
project(reaper_csurf_mcu_klinke C CXX)

# --- Paths --------------------------------------------------------------------
set(SRC_DIR   ${CMAKE_CURRENT_SOURCE_DIR})
set(JUCE_DIR  ${SRC_DIR}/juce_8)
set(SDK_DIR   ${SRC_DIR}/reaper-sdk/sdk)
set(WDL_DIR   ${SRC_DIR}/reaper-sdk/WDL)
set(SWELL_DIR ${WDL_DIR}/swell)
set(BOOST_DIR ${SRC_DIR}/boost_1_39_0)

# --- Guard: bail early with a clear message if deps are missing --------------
foreach(d juce_8 reaper-sdk boost_1_39_0)
    if(NOT EXISTS "${SRC_DIR}/${d}")
        message(FATAL_ERROR
            "\n  Dependency '${d}/' not found.\n"
            "  Run ./fetch_deps.sh first (see AGENTS.md, §2 Prerequisites).\n")
    endif()
endforeach()

# --- Options ------------------------------------------------------------------
option(MCU_DEBUG_LOG "Write button/window debug log to <REAPER_RESOURCE>/mcu_klinke_debug.log" ON)

# =============================================================================
#  Versioning — VERSION file at the repo root holds "<version> <build-count>".
#
#  • version       (e.g. 0.9.1.3): bumped MANUALLY for a release; when bumped,
#                  reset build-count to 1.
#  • build-count : auto-incremented on every `cmake` configure and compiled
#                  into the binary as "... build N". The number stored in the
#                  file is the build number of the LAST build produced.
#
#  The file is rewritten (count++) on each configure — commit it as-is. The
#  generated header ${CMAKE_BINARY_DIR}/Version.h is included by csurf_mcu.h.
#  Increment happens at CONFIGURE time (the build flow always runs `cmake ..`
#  before `cmake --build`, so the count tracks one build cycle at a time).
# =============================================================================
set(MCU_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
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
# increment-first model: count stored in the file = last build produced
math(EXPR MCU_BUILD_COUNT "${_vc} + 1")
set(MCU_VERSION_DISPLAY "v${MCU_VERSION} build ${MCU_BUILD_COUNT}")
file(WRITE "${MCU_VERSION_FILE}"
    "# Format: <version> <build-count>\n"
    "#\n"
    "# version       bump MANUALLY for a release (e.g. 0.9.1.3 -> 0.9.2.0).\n"
    "#               When you bump the version, reset build-count to 1.\n"
    "# build-count   auto-incremented on every `cmake` configure and compiled into\n"
    "#               the binary as \"... build N\". The number stored here is the\n"
    "#               build number of the LAST build produced.\n"
    "#\n"
    "# This file is rewritten (count++) on each configure - this is expected; commit\n"
    "# it as-is. Do NOT run `cmake ..` without building afterwards (it bumps the\n"
    "# counter without producing a binary). Displayed in Reaper as: v<version> build <count>.\n"
    "${MCU_VERSION} ${MCU_BUILD_COUNT}\n")
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

target_include_directories(reaper_csurf_mcu_klinke PRIVATE
    ${CMAKE_BINARY_DIR}   # generated Version.h (MCU_VERSION_STRING)
    ${SRC_DIR}      # repo root first: csurf.h, reaper_plugin_functions.h, etc.
    ${JUCE_DIR}
    ${SDK_DIR}
    ${WDL_DIR}      # ptrlist.h, ...
    ${SWELL_DIR}    # swell.h, ...
    ${BOOST_DIR})   # boost/signals2.hpp, ...

# posix_shims/windows.h → swell.h (only on !WIN32; on Windows the real
# Windows SDK windows.h is used instead — no shadowing).
if(NOT WIN32)
    target_include_directories(reaper_csurf_mcu_klinke PRIVATE ${SRC_DIR}/posix_shims)
endif()

target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE
    TIXML_USE_TICPP
    NDEBUG
    SWELL_PROVIDED_BY_APP
    SWELL_LOAD_SWELL_DYLIB       # bind SWELL from REAPER's libSwell.so at load
    _FILE_OFFSET_BITS=64
    NOMINMAX                     # stop SWELL's min/max macros breaking <limits>
    WDL_NO_DEFINE_MINMAX
    # res.rc_mac_dlg auto-generates FLIPPED|NOAUTOSIZE dialog flags that yield a
    # 0x0 dialog on Linux and a flipped Y-axis on macOS. Override to a plain
    # WS_CHILD style (=1) at scale 1.0. (see res.rc_mac_dlg / res_linux.cpp)
    "SET_IDD_SURFACEEDIT_MCUMAIN_STYLE=1"
    "SET_IDD_SURFACEEDIT_MCUEX_STYLE=1"
    "SET_IDD_SURFACEEDIT_MCUMAIN_SCALE=1.0"
    "SET_IDD_SURFACEEDIT_MCUEX_SCALE=1.0"
    "SWELL_DLG_WS_DEFAULT_SCALING=0")  # macOS-only macro; 0 so _BEGIN compiles on Linux

if(MCU_DEBUG_LOG)
    target_compile_definitions(reaper_csurf_mcu_klinke PRIVATE MCU_DEBUG_LOG)
endif()

# --- Source files (full set from the .vcxproj; verified present) -------------
target_sources(reaper_csurf_mcu_klinke PRIVATE
    Actions.cpp
    ActionsDialogComponent.cpp
    ActionsDisplay.cpp
    ButtonManager.cpp
    CCSManager.cpp
    CCSMode.cpp
    CCSModesEditor.cpp
    CommandMode.cpp
    CommandModeMainComponent.cpp
    CommandModePageComponent.cpp
    CommandModeVPOTComponent.cpp
    csurf_main.cpp
    csurf_mcu.cpp
    Display.cpp
    DisplayHandler.cpp
    MeterBridge.cpp
    MultiTrackMeterBridge.cpp
    MultiTrackMode.cpp
    MultiTrackOptions.cpp
    MultiTrackOptions2.cpp
    MultiTrackSelector.cpp
    Options.cpp
    PanMode.cpp
    PerformanceMode.cpp
    PlugAccess.cpp
    PluginWatcher.cpp
    PlugMap.cpp
    PlugMapManager.cpp
    PlugMapSaveDialog.cpp
    PlugMode.cpp
    PlugMode2ndOptions.cpp
    PlugModeBankComponent.cpp
    PlugModeBankReferenceComponent.cpp
    PlugModeChannelComponent.cpp
    PlugModeComponent.cpp
    PlugModeFaderComponent.cpp
    PlugModeMapInfoComponent.cpp
    PlugModeMeterBridge.cpp
    PlugModeOptions.cpp
    PlugModePageComponent.cpp
    PlugModePageReferenceComponent.cpp
    PlugModeParamComponent.cpp
    PlugModeSelectors.cpp
    PlugModeSingleBankComponent.cpp
    PlugModeSingleChannelComponent.cpp
    PlugModeSinglePageComponent.cpp
    PlugModeVPOTComponent.cpp
    PlugModeVPOTTableComponent.cpp
    PlugMoveWatcher.cpp
    PlugPresetManager.cpp
    PlugWindowManager.cpp
    ProjectConfig.cpp
    ReceiveMode.cpp
    Region.cpp
    Selector.cpp
    SendMode.cpp
    SendReceiveMeterBridge.cpp
    SendReceiveModeBase.cpp
    TabbedComponentWithCallback.cpp
    Tracks.cpp
    TrackStatesEditorComponent.cpp
    TrackStatesTableComponent.cpp
    Transport.cpp
    UndoEnd.cpp
    VPOT_LED.cpp
    ${SWELL_DIR}/swell-modstub-generic.cpp   # binds SWELL funcs via REAPER
    res_linux.cpp)                           # SWELL dialog resources

# JUCE 8 module (pulls core/events/graphics/data_structures transitively).
target_link_libraries(reaper_csurf_mcu_klinke PRIVATE
    juce::juce_gui_basics)

# =============================================================================
#  Platform-specific flags / system libs / output name
# =============================================================================
if(UNIX AND NOT APPLE)
    # --- Linux (baseline) ----------------------------------------------------
    # C++14: the ported source uses C++11+ features (auto, nullptr, lambdas) and
    # builds under strict ISO C++14 (gnu++14 was previously used; c++14 is the
    # portability target). Boost 1.39's shared_ptr has a move-ctor bug under
    # C++11+ (declares move ctors but no copy ctor → implicit copy ctor
    # deleted → boost::signals2 breaks); we patch that in fetch_deps.sh.
    # Same flag on both targets to avoid cross-TU ABI risk.
    # -std=c++14 (strict ISO, no GNU extensions) is the portability target.
    # Verified 2026-06-23: builds clean on Linux/GCC with zero errors — the
    # code relies on no GNU extensions and no _GNU_SOURCE-only POSIX funcs
    # (no realpath()/strdup() etc. went missing). Same warning set as gnu++14.
    # JUCE 8 requires C++17 minimum; our source moves c++14 -> c++17 here.
    # -fpermissive kept: the Jucer 1.11-generated *Component files are loose
    # about conversions; we will tighten this as the upgrade progresses.
    target_compile_options(reaper_csurf_mcu_klinke PRIVATE
        -std=c++17 -fPIC -fvisibility=hidden -fpermissive
        -Wno-deprecated-declarations -Wno-unused-result
        -Wno-sign-compare -Wno-reorder -Wno-multichar)
    target_link_libraries(reaper_csurf_mcu_klinke PRIVATE X11 Xext pthread dl)
    set_target_properties(reaper_csurf_mcu_klinke PROPERTIES
        PREFIX "" OUTPUT_NAME "reaper_csurf_mcu_klinke" SUFFIX ".so")

elseif(WIN32)
    # --- Windows -------------------------------------------------------------
    # TODO (next milestone): native MSVC build. Until then the .vcxproj remains
    # the source of truth for Windows. Planned: MSVC toolset v142, link
    # reaper_plugin_functions etc., output .dll (no lib prefix), compile res.rc
    # via the resource compiler instead of SWELL/res_linux.cpp.
    message(FATAL_ERROR
        "\n  CMake build for Windows is not implemented yet.\n"
        "  Build with reaper_csurf.vcxproj (Visual Studio) for now.\n"
        "  This is the next milestone once the Linux baseline is confirmed.\n")

elseif(APPLE)
    # --- macOS ---------------------------------------------------------------
    # TODO (after Windows): Clang/Xcode, AppKit (not X11), JUCE Carbon/Cocoa
    # backend of 1.52, output .dylib.
    message(FATAL_ERROR
        "\n  CMake build for macOS is not implemented yet.\n"
        "  Planned after the Windows build.\n")

else()
    message(FATAL_ERROR "Unsupported platform for the CMake build.")
endif()
