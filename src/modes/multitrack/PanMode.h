/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "MultiTrackMode.h"
#include "PlugModeOptions.h"
#include "MultiTrackSelector.h"

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
};
