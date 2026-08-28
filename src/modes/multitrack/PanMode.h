/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "MultiTrackMode.h"
#include "PlugModeOptions.h"
#include "MultiTrackSelector.h"
#include <vector>

class PanMode : public MultiTrackMode {
public:
  PanMode(CCSManager *pManager);

public:
  virtual ~PanMode(void);

  void activate();

  bool vpotMoved(int channel,
                 int numSteps); // numSteps are negativ for left rotation

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

  void ensureVpotValueState(int channelCount);
  bool showingVpotValue(int channel, DWORD now) {
    return channel > 0 && channel < (int)m_vpotValueShownTill.size() &&
           m_vpotValueShownTill[channel] > now;
  }
};
