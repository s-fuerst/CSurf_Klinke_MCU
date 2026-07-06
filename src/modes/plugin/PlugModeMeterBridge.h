/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#ifndef MCU_PLUGMODEMETERBRIDGE
#define MCU_PLUGMODEMETERBRIDGE

#include "csurf.h"

#include "MeterBridge.h"

class PlugMode;

class PlugModeMeterBridge : public MeterBridge {
public:
  PlugModeMeterBridge(PlugMode *pPlugMode);
	bool alsoOnDisplay() { return true; }
  void updateMeterBridge(CSurf_MCU *pMCU);

private:
	PlugMode *m_pPlugMode;
};

#endif
