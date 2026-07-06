/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
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
