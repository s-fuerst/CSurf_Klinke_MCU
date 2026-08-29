/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Channel Strip Mode
 *
 * Exposes the most-used FX parameters of the SELECTED track's FX chain on the
 * 8 VPOTs (16 with Shift). See ai-docs/channelstrip-mode-plan.md.
 *
 * Inherits from MultiTrackMode so Sel/Mute/Solo/Rec buttons, faders, bank
 * navigation, and LED updates work identically to Pan/Action/Send modes.
 * ONLY the VPOTs and row 1 of the display behave differently: they address
 * the SELECTED track (not the per-channel bank track), and each unit drives
 * the 16 parameters of the strip assigned to that unit for the selected
 * track.
 *
 * Architecture:
 *   • 16 GLOBAL Channel Strips (project-wide). Each strip = one plugin +
 *     its parameter→VPOT mapping (8 VPOTs + 8 with Shift).
 *   • Per-(trackGUID, unitIndex): an assignment to one of the 16 strips
 *     (–1 = none). Persisted in the project (Step E).
 *   • A unit's 8 VPOTs (16 with Shift) drive the parameters of the strip
 *     assigned to that unit for the selected track.
 *
 * The on-screen editor (ALT+TRACK) manages the 16 strips globally — it is
 * independent of any track or unit.
 *
 * Activated by the TRACK assign button (B_VPOT_TRACK). Stays active until
 * another assign button is pressed (same behaviour as Plug/Pan/Action).
 *
 * Per-unit behaviour is fully DERIVED (not switched) from three facts:
 *   (selected track present?) × (unit has strip AND its plugin is present?)
 *   × (selection mode?)
 * Selection mode is true while B_VPOT_TRACK is held down (re-pick). A unit
 * with no assigned strip — or with an assigned strip whose plugin is MISSING
 * on the selected track (e.g. the user removed the FX) — is always in pick
 * mode regardless of selection mode.
 *
 * States (see plan §4):
 *   no selected track      -> VPOTs OFF, row 1 = "You must select a single track."
 *   unit has no strip      -> row 1 shows strip names (leading "+" = plugin
 *   OR its plugin is       -> missing on the track); VPOT press picks a strip
 *   missing on the track   -> (and auto-adds the plugin if missing)
 *   unit has strip, plugin present, !sel
 *                          -> row 1 shows param name (idle) / value (1 s after turn);
 *                             VPOT turn nudges, VPOT press toggles 0/1
 *   unit has strip, sel    -> row 1 shows strip names; VPOT press re-picks
 */
#pragma once

#include "MultiTrackMode.h"
#include "ChannelStripMap.h"
#include "SurfaceConfig.h" // MAX_SURFACE_UNITS
#include "ProjectConfig.h"
#include <map>

class ChannelStripAccess;
class ChannelStripComponent;

class ChannelStripMode : public MultiTrackMode {
public:
  static const int kNumStrips = 16;
  static const int kVPOTsPerUnit = 16; // 8 normal + 8 Shift

  ChannelStripMode(CCSManager *pManager);
  ~ChannelStripMode() override;

  void activate() override;

  // hardware events
  bool vpotMoved(int channel, int numSteps) override;
  bool vpotPressed(int channel, bool pressed) override;

  void updateDisplay() override;
  void updateVPOTs() override;
  void updateAssignmentDisplay() override;

  void frameUpdate() override;

  // editor lifecycle
  Component **createEditorComponent() override;
  void deleteEditorComponent() override;
  void removeEditor();

  // --- accessors ---
  ChannelStripAccess *getAccess() { return m_pAccess; }

  // 16 global strips (index 0..15), shared across all projects/tracks
  ChannelStripMap *getStrip(int index); // never NULL

  // per-(track, unit) assignment: which strip index (–1 = none)
  int getAssignedStripIndex(MediaTrack *tr, int unit);
  void setAssignedStripIndex(MediaTrack *tr, int unit, int stripIndex,
                             bool notifyHardware = true);

  // runtime: the strip active for this channel's unit on the selected track
  // (NULL if no selected track / unit unassigned)
  ChannelStripMap *getStripForChannel(int globalChannel);
  MediaTrack *getSelectedTrack();

  // selection mode: true while B_VPOT_TRACK is held (re-pick). Set by
  // CCSManager on B_VPOT_TRACK press/release.
  void setSelectionMode(bool on) { m_selectionMode = on; }
  bool isSelectionMode() const { return m_selectionMode; }

  // called by the editor after a strip is mutated
  void bindingChanged();

  // --- CTRL commands (Step G) ---
  // CONTROL+VPOT-1: toggle the FLOATING FX window of the unit's strip FX on
  // the selected track (close it if already open; PlugMode window settings
  // ignored).
  // CONTROL+VPOT-2: toggle the FX CHAIN (close it if already open).
  // CONTROL+VPOT-3 ("PlMode"): switch to PlugMode and select the unit's
  // strip FX there (active unit = the pressed unit's).
  // CONTROL+VPOT-5: remove the assigned FX instance from the selected track.
  void openFxWindow(MediaTrack *tr, int fxSlot, bool floating);
  // CONTROL+VPOT-7/8: move the strip FX one slot up (-1) / down (+1).
  // Empty target slots use the REAPER 7.75+ slot_hint path; occupied
  // neighbouring slots use the original dense TrackFX_CopyToTrack move so
  // the two FX exchange positions. Returns false at chain edges.
  bool moveFx(MediaTrack *tr, int fxSlot, int dir);

  // --- persistence ---
  // All 16 strips (header + VPOT mapping) live in ONE file,
  // ~/.config/REAPER/MCU/ChannelStripMaps/channelstrips.xml. Loaded once at
  // startup; saved when either the main editor or the mapping editor closes.
  void loadStripsFromFile();
  void saveStripsToFile();
  // per-(track, unit) assignments: stored in the Reaper project via
  // ProjectConfig (WRITE/READ/FREE).
  void projectChanged(XmlElement *pRootNode, ProjectConfig::EAction action);

  Options *getOptions() override { return MultiTrackMode::getOptions(); }
  Options *get2ndOptions() override { return MultiTrackMode::get2ndOptions(); }

private:
  // VPOT position 0..15 for a local channel (0..7) + Shift
  int slotFor(int localCh);
  void updateChannel(int globalChannel);
  // persistence helpers
  static juce::File getStripsDir();
  static juce::File getGlobalFile();

  ChannelStripAccess *m_pAccess;
  ChannelStripComponent *m_pEditor;

  // 16 global channel strips
  ChannelStripMap m_strips[kNumStrips];

  // per-channel timestamp of last VPOT change, for the 1-second value display
  Time m_lastVPOTChangeTime[MAX_SURFACE_UNITS * 8];

  // per-track per-unit: which strip index (–1 = none)
  struct PerTrackAssignments {
    int stripIndexForUnit[MAX_SURFACE_UNITS];
    PerTrackAssignments();
  };
  std::map<String, PerTrackAssignments> m_assignments;

  bool m_selectionMode;
  bool m_lastShiftState;
  bool m_lastCtrlState; // last CONTROL state, for the command legend refresh
  int m_projectChangedConnectionId;
};
