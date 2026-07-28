/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * A Channel Strip Map — exactly 16 flat parameter bindings operated by the
 * 8 VPOTs of one unit (slots 1..8 normal, 9..16 with Shift).
 *
 * One map exists per (trackGUID, unitIndex); the per-track/per-unit storage is
 * handled by ChannelStripTrackState (Step E). A map can also be saved to /
 * loaded from a file (the editor's save/load, Step C9).
 *
 * XML:
 *   <CHANNELSTRIPMAP creator="..." info="...">
 *     <SLOT nr="1" fxident="VST3:ReaEQ (Cockos)" fxguid="{..}" param="3" name="EQG1" inspos="last"/>
 *     ...up to 16...
 *   </CHANNELSTRIPMAP>
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripBinding.h"
#include <vector>

class ChannelStripMap {
public:
  static const int kNumSlots = 16; // 8 normal + 8 Shift

  ChannelStripMap();
  ~ChannelStripMap();

  void initEmpty();

  // slot index 0..15
  ChannelStripBinding *getSlot(int i) { return &m_slots[i]; }
  const ChannelStripBinding *getSlot(int i) const { return &m_slots[i]; }

  int numAssigned() const;

  const String &getCreator() const { return m_creator; }
  void setCreator(const String &creator) { m_creator = creator; }
  const String &getInfo() const { return m_info; }
  void setInfo(const String &info) { m_info = info; }

  // Writes into (or reads from) a parent element by adding/finding <SLOT>
  // children. writeToXml adds a child per assigned slot.
  void writeToXml(XmlElement *pParent) const;
  bool readFromXml(const XmlElement *pParent);

private:
  std::vector<ChannelStripBinding> m_slots; // size kNumSlots
  String m_creator;
  String m_info;
};
