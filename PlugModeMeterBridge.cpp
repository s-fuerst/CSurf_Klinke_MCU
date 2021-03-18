/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "PlugModeMeterBridge.h"
#include "csurf_mcu.h"
#include "PlugMode.h"
#include "PlugAccess.h"


PlugModeMeterBridge::PlugModeMeterBridge(PlugMode *pPlugMode)
    : MeterBridge() {
	m_pPlugMode = pPlugMode;
}


void PlugModeMeterBridge::updateMeterBridge(CSurf_MCU * pMCU) {
  //if (m_pDisplayHandler->getMCU()->IsFlagSet(CONFIG_FLAG_NO_LEVEL_METER))
  //  return;
  // 0xD0 = level meter, hi nibble = channel index, low = level (F=clip, E=top)
	DWORD now = pMCU->GetActualFrameTime();

  double decay = 0.0;
  if (m_mcu_meter_lastrun) {
    decay =
        VU_BOTTOM * (double)(now - m_mcu_meter_lastrun) /
        (1.4 * 1000.0); // they claim 1.8s for falloff but we'll underestimate
  }
  m_mcu_meter_lastrun = now;
	MediaTrack *pPlugTrack = m_pPlugMode->getPlugAccess()->getPlugTrack();
	for (int x = 1; x < 9; x++) {
		updateMeter(x, pPlugTrack, pMCU, decay, x-1);
	}
	MeterBridge::updateMasterLEDs(pMCU, decay);
}

