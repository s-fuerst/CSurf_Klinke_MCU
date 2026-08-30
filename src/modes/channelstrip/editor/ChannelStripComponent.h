/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * On-screen editor for Channel Strip Mode. Opened via ALT+TRACK.
 *
 * Purely global: manages the 16 global channel strips, independent of any
 * track or unit. One table, one row per strip, plus a toolbar to save/load
 * the COMPLETE set (all 16 slots) to/from a user-chosen XML file.
 *
 *   +---------------------------------------------------------------------+
 *   | # | Plugin | Abbrev | InsPos | Parameter… | Save | Load | Clear     | (16 rows)
 *   +---------------------------------------------------------------------+
 *   | [ Save all 16… ]   [ Load all 16… ]                                  |
 *   +---------------------------------------------------------------------+
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripBindingTable.h"

class ChannelStripMode;

class ChannelStripComponent : public Component, public Button::Listener {
public:
  ChannelStripComponent(ChannelStripMode *pMode);
  ~ChannelStripComponent();

  void resized() override;
  void updateEverything();
  void buttonClicked(Button *button) override;

private:
  ChannelStripMode *m_pMode;
  ChannelStripBindingTable *m_table;
  TextButton *m_saveAllButton;
  TextButton *m_loadAllButton;
};
