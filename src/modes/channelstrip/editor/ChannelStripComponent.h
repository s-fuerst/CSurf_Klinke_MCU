/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * On-screen editor for Channel Strip Mode. Opened via ALT+TRACK.
 *
 *   +--------------------------------------------------+
 *   | Unit: [combo 1..N]   Channel Strip editor        |
 *   +--------------------------------------------------+
 *   | # | Plugin | Abbrev | InsPos | Parameter          |  (16 rows)
 *   | ...                                              |
 *   +--------------------------------------------------+
 *
 * Edits flow straight into the active unit's ChannelStripMap and notify the
 * mode so the hardware reflects them immediately.
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripBindingTable.h"

class ChannelStripMode;

class ChannelStripComponent : public Component, public ComboBox::Listener {
public:
  ChannelStripComponent(ChannelStripMode *pMode);
  ~ChannelStripComponent();

  void resized() override;
  void comboBoxChanged(ComboBox *combo) override;

  void updateEverything();

private:
  ChannelStripMode *m_pMode;
  Label *m_unitLabel;
  ComboBox *m_unitCombo;
  ChannelStripBindingTable *m_table;
};
