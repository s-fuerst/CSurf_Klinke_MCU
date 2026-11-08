/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "MultiTrackMode.h"
#include "PlugModeOptions.h"
#include "MultiTrackSelector.h"
#include "ModifierCommands.h" // CTRL+VPOT command table (Layer 2)
#include <vector>

class PanMode : public MultiTrackMode {
public:
  PanMode(CCSManager *pManager);

public:
  virtual ~PanMode(void);

  void activate();

  bool vpotMoved(int channel,
                 int numSteps); // numSteps are negativ for left rotation

  bool vpotPressed(int channel, bool pressed) override;

  void updateDisplay();

  Selector *getSelector() { return m_pSelector; }

private:
  // How long (ms) a turned VPOT's value (Pan, or Volume when flipped) is
  // shown on row 1 before reverting to the fader-controlled value.
  static const DWORD VPOT_VALUE_SHOW_MS = 1000;
  // Per-global-channel deadline (based on CCSManager::getLastTime()) until
  // which the VPOT-controlled value is shown on row 1 instead of the
  // fader-controlled value. 0 means "not showing".
  std::vector<DWORD> m_vpotValueShownTill;
  // Full-line hints also occupy field separator columns, so clear them when
  // the hint disappears before redrawing individual fields.
  bool m_ctrlSelectionHintVisible = false;

  // --- CTRL commands (modifier command scheme, see
  // ai-docs/modifier-command-scheme.md). All commands require exactly one
  // selected normal track. The row-1 legend follows the live selection.
  MediaTrack *selectedCommandTrack() const;
  bool ctrlInsertTrack(int channel);
  bool ctrlDuplicateTrack(int channel);
  bool ctrlClearTrack(int channel);
  bool ctrlRemoveTrack(int channel);
  void updateCtrlLegend();

  // Remove track envelope sections from the RPPXML state used by Clear.
  bool stripEnvelopeSections(const String &chunk, String &out);

  // CTRL+VPOT command table (Layer 2).
  ModifierCommands m_ctrlCommands;

  void ensureVpotValueState(int channelCount);
  bool showingVpotValue(int channel, DWORD now) {
    return channel > 0 && channel < (int)m_vpotValueShownTill.size() &&
           m_vpotValueShownTill[channel] > now;
  }
};
