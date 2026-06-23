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
#    juce_1_52/   reaper-sdk/sdk/  reaper-sdk/WDL/  boost_1_39_0/
#
#  The same dependency versions as the original VS project are kept
#  (JUCE 1.52, Boost 1.39). Do not "modernise" them without a decision.
# ─────────────────────────────────────────────────────────────────────────────
project(reaper_csurf_mcu_klinke CXX)

# --- Paths --------------------------------------------------------------------
set(SRC_DIR   ${CMAKE_CURRENT_SOURCE_DIR})
set(JUCE_DIR  ${SRC_DIR}/juce_1_52)
set(SDK_DIR   ${SRC_DIR}/reaper-sdk/sdk)
set(WDL_DIR   ${SRC_DIR}/reaper-sdk/WDL)
set(SWELL_DIR ${WDL_DIR}/swell)
set(BOOST_DIR ${SRC_DIR}/boost_1_39_0)

# --- Guard: bail early with a clear message if deps are missing --------------
foreach(d juce_1_52 reaper-sdk boost_1_39_0)
    if(NOT EXISTS "${SRC_DIR}/${d}")
        message(FATAL_ERROR
            "\n  Dependency '${d}/' not found.\n"
            "  Run ./fetch_deps.sh first (see AGENTS.md, §2 Prerequisites).\n")
    endif()
endforeach()

# --- Options ------------------------------------------------------------------
option(MCU_DEBUG_LOG "Write button/window debug log to <REAPER_RESOURCE>/mcu_klinke_debug.log" ON)

# =============================================================================
#  JUCE 1.52 amalgamated — compiled as a separate OBJECT library so the old
#  C++03-era source gets its own (very permissive) flags and -w silencing.
# =============================================================================
find_package(Freetype REQUIRED)   # only the Linux X11 backend uses FreeType
add_library(juce_amalgamated OBJECT ${JUCE_DIR}/juce_amalgamated.cpp)
target_include_directories(juce_amalgamated PRIVATE
    ${JUCE_DIR}
    ${WDL_DIR}
    ${FREETYPE_INCLUDE_DIRS})
target_compile_definitions(juce_amalgamated PRIVATE
    NDEBUG
    JUCE_OPENGL=0
    JUCE_USE_XINERAMA=0
    JUCE_USE_XSHM=0
    JUCE_USE_XCURSOR=0)

# =============================================================================
#  Plugin target
# =============================================================================
add_library(reaper_csurf_mcu_klinke SHARED)

target_include_directories(reaper_csurf_mcu_klinke PRIVATE
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

# JUCE object is always linked; system libs differ per platform (below).
target_link_libraries(reaper_csurf_mcu_klinke PRIVATE
    $<TARGET_OBJECTS:juce_amalgamated>)

# =============================================================================
#  Platform-specific flags / system libs / output name
# =============================================================================
if(UNIX AND NOT APPLE)
    # --- Linux (baseline) ----------------------------------------------------
    # C++14: the ported source uses C++11+ features (auto, nullptr, lambdas) and
    # Rothchild's Linux port builds cleanly under gnu++14. Boost 1.39's
    # shared_ptr has a move-ctor bug under C++11+ (declares move ctors but no
    # copy ctor → implicit copy ctor deleted → boost::signals2 breaks); we
    # patch that in fetch_deps.sh. Same flag on both targets to avoid cross-TU
    # ABI risk.
    target_compile_options(juce_amalgamated PRIVATE
        -std=gnu++14 -fPIC -w -fpermissive -Wno-deprecated-declarations)
    target_compile_options(reaper_csurf_mcu_klinke PRIVATE
        -std=gnu++14 -fPIC -fvisibility=hidden -fpermissive
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
