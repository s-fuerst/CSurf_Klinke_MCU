/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Channel Strip Mode
 *
 * Exposes the most-used FX parameters of the selected track's FX chain on the
 * 8 VPOTs (16 with Shift). See ai-docs/channelstrip-mode-plan.md.
 *
 * Architecture:
 *   • 16 GLOBAL Channel Strips (projektübergreifend). Each strip = one plugin +
 *     its parameter→VPOT mapping (8 VPOTs + 8 with Shift).
 *   • Per-(trackGUID, unitIndex): an assignment to one of the 16 strips
 *     (–1 = none). Persisted in the project (Step E).
 *   • A unit's 8 VPOTs drive the parameters of its assigned strip.
 *
 * The on-screen editor (ALT+TRACK) manages the 16 strips globally — it is
 * independent of any track or unit. The per-track/unit assignment is a separate
 * concern (not in that editor).
 *
 * Activated by the TRACK assign button (B_VPOT_TRACK). Stays active until
 * another assign button is pressed (same behaviour as Plug/Pan/Action).
 */
#pragma once

#include "CCSMode.h"
#include "ChannelStripMap.h"
#include "SurfaceConfig.h" // MAX_SURFACE_UNITS
#include <map>

class Display;
class ChannelStripAccess;
class ChannelStripComponent;

class ChannelStripMode : public CCSMode {
public:
  static const int kNumStrips = 16;
  static const int kVPOTsPerUnit = 16; // 8 normal + 8 Shift

  ChannelStripMode(CCSManager *pManager);
  ~ChannelStripMode() override;

  bool supportsExtendedChannels() const override { return true; }

  void activate() override;
  void deactivate() override;

  // hardware events
  bool vpotMoved(int channel, int numSteps) override;
  bool vpotPressed(int channel, bool pressed) override;

  void updateDisplay() override;
  void updateVPOTs() override;
  void updateAssignmentDisplay() override;

  void trackName(MediaTrack *trackid, const char *pName) override {
    updateDisplay();
  }
  void frameUpdate() override;

  // editor lifecycle
  Component **createEditorComponent() override;
  void deleteEditorComponent() override;
  void removeEditor();

  // --- accessors ---
  ChannelStripAccess *getAccess() { return m_pAccess; }
  MediaTrack *getSelectedTrack();
  int numUnits() const;

  // 16 global strips (index 0..15), shared across all projects/tracks
  ChannelStripMap *getStrip(int index); // never NULL

  // per-unit assignment on the CURRENT track (–1 = unassigned)
  int getAssignedStripIndex(int unit);
  void setAssignedStripIndex(int unit, int stripIndex, bool notifyHardware = true);

  // runtime: the strip active for this unit (NULL if unassigned / no track)
  ChannelStripMap *getStripForUnit(int unit);

  // called by the editor after a strip is mutated
  void bindingChanged();

private:
  void resolveChannel(int channel, int &unit, int &localCh);
  int slotFor(int localCh); // VPOT position 0..15 (8 normal + 8 Shift)
  void trackChanged(MediaTrack *pTrack);
  void updateChannel(int globalChannel);

  Display *m_pDisplay;

  ChannelStripAccess *m_pAccess;
  ChannelStripComponent *m_pEditor;

  // 16 global channel strips
  ChannelStripMap m_strips[kNumStrips];

  // per-track per-unit: which strip index (–1 = none)
  struct PerTrackAssignments {
    int stripIndexForUnit[MAX_SURFACE_UNITS];
    PerTrackAssignments();
  };
  std::map<String, PerTrackAssignments> m_assignments;
  String m_currentTrackGUID;

  bool m_lastShiftState;
};
