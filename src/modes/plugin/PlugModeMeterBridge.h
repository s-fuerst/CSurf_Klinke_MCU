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
  // Plug Mode shows parameter names/values on the LCD; the software-meter
  // bars must never be drawn over that text. Strip/master meters are still
  // sent to the hardware (0xD0/0xD1), only the on-display bars are off.
  bool alsoOnDisplay() { return false; }
  void updateMeterBridge(CSurf_MCU *pMCU);

private:
	PlugMode *m_pPlugMode;
};

#endif
