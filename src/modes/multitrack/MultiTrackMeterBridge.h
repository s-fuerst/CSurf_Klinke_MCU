/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifndef MCU_MULTITRACKMETERBRIDGE
#define MCU_MULTITRACKMETERBRIDGE

#include "csurf.h"

#include "MeterBridge.h"

class MultiTrackMeterBridge : public MeterBridge {
public:
  MultiTrackMeterBridge();
	bool alsoOnDisplay() { return true; }
  void updateMeterBridge(CSurf_MCU *pMCU);
};

#endif
