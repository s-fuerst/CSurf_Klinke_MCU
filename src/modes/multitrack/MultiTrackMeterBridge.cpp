/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "MultiTrackMeterBridge.h"
#include "csurf_mcu.h"
#include "McuAssert.h"
#include "Tracks.h"


MultiTrackMeterBridge::MultiTrackMeterBridge()
    : MeterBridge() {
}


void MultiTrackMeterBridge::updateMeterBridge(CSurf_MCU * pMCU) {
  //if (m_pDisplayHandler->getMCU()->IsFlagSet(CONFIG_FLAG_MACKIE_LEVEL_METER))
  //  return;
  // 0xD0 = level meter, hi nibble = channel index, low = level (F=clip, E=top)
	DWORD now = pMCU->GetActualFrameTime();
	
  int x;

  double decay = 0.0;
  if (m_mcu_meter_lastrun) {
    decay =
        VU_BOTTOM * (double)(now - m_mcu_meter_lastrun) /
        (1.4 * 1000.0); // they claim 1.8s for falloff but we'll underestimate
  }
  m_mcu_meter_lastrun = now;
  ensureStripMeterState(pMCU->availableChannels());
  for (x = 1; x <= pMCU->availableChannels(); x++) {
    MediaTrack *t;

    if ((t = Tracks::instance()->getMediaTrackForChannel(x))) {
			updateMeter(x, t, pMCU, decay, -1);
    }
  }
	MeterBridge::updateMasterLEDs(pMCU, decay);
}

