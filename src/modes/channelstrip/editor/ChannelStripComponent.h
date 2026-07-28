/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * On-screen editor for Channel Strip Mode. Opened via ALT+TRACK.
 *
 * Purely global: manages the 16 global channel strips, independent of any
 * track or unit. One table, one row per strip.
 *
 *   +------------------------------------------------------+
 *   | # | Plugin | Abbrev | InsPos | Parameter              | (16 rows = 16 strips)
 *   +------------------------------------------------------+
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripBindingTable.h"

class ChannelStripMode;

class ChannelStripComponent : public Component {
public:
  ChannelStripComponent(ChannelStripMode *pMode);
  ~ChannelStripComponent();

  void resized() override;
  void updateEverything();

private:
  ChannelStripMode *m_pMode;
  ChannelStripBindingTable *m_table;
};
