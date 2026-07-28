/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Channel Strip Mode
 *
 * Exposes the most-used FX parameters of the selected track's FX chain as a
 * flat surface of 8 VPOTs (16 with Shift). See ai-docs/channelstrip-mode-plan.md.
 *
 * One ChannelStripMap (16 flat parameter bindings) per (trackGUID, unitIndex):
 *   slot 1..8  -> VPOTs 1..8 of the unit (normal)
 *   slot 9..16 -> VPOTs 1..8 of the unit (Shift held)
 *
 * Activated by the TRACK assign button (B_VPOT_TRACK). Stays active until
 * another assign button is pressed (same behaviour as Plug/Pan/Action).
 */
#pragma once

#include "CCSMode.h"
#include "ChannelStripMap.h"
#include <map>
#include <vector>

class Display;
class ChannelStripAccess;
class ChannelStripComponent;

class ChannelStripMode : public CCSMode {
public:
  ChannelStripMode(CCSManager *pManager);
  ~ChannelStripMode() override;

  // opt into extender (channel > 8) events — one strip per unit
  bool supportsExtendedChannels() const override { return true; }

  void activate() override;
  void deactivate() override;

  // from MCU via CCSManager
  bool vpotMoved(int channel, int numSteps) override;
  bool vpotPressed(int channel, bool pressed) override;

  void updateDisplay() override;
  void updateVPOTs() override;
  void updateAssignmentDisplay() override;

  void trackName(MediaTrack *trackid, const char *pName) override {
    updateDisplay();
  }
  void frameUpdate() override;

  Component **createEditorComponent() override;
  void deleteEditorComponent() override;
  void removeEditor();

  // the number of parameter slots a single strip exposes (8 + 8 Shift)
  static const int kSlotsPerUnit = 16;

  ChannelStripAccess *getAccess() { return m_pAccess; }
  MediaTrack *getSelectedTrack(); // public wrapper of CCSMode::selectedTrack()

  // The per-(trackGUID, unit) maps. In-memory for now (Step E adds project
  // persistence). Returns NULL if no track is selected.
  ChannelStripMap *getMapForUnit(int unit);
  int numUnits() const;

  // Called by the editor after editing a binding so the hardware reflects it.
  void bindingChanged();

private:
  // resolve channel(1-based global) -> (unit, localCh 0..7)
  void resolveChannel(int channel, int &unit, int &localCh);
  // slot index 0..15 for a given unit+localCh, honouring Shift
  int slotFor(int localCh);
  // (re)bind to the selected track: loads/creates its per-unit maps
  void trackChanged(MediaTrack *pTrack);
  // update one channel's VPOT ring + display fields
  void updateChannel(int globalChannel);

  Display *m_pDisplay; // per-unit composite (MultiDisplay if N>1)

  ChannelStripAccess *m_pAccess;
  ChannelStripComponent *m_pEditor;

  // current track's maps (point into m_mapsByTrack), NULL if no selection
  std::vector<ChannelStripMap> *m_pCurrentMaps;
  std::map<String, std::vector<ChannelStripMap>> m_mapsByTrack;
  String m_currentTrackGUID;
  bool m_lastShiftState;
};
