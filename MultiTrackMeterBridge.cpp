/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "MultiTrackMeterBridge.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "Tracks.h"
#include "boost\foreach.hpp"


MultiTrackMeterBridge::MultiTrackMeterBridge()
    : MeterBridge() {
}


void MultiTrackMeterBridge::updateMeterBridge(DWORD now, CSurf_MCU * pMCU) {
  //if (m_pDisplayHandler->getMCU()->IsFlagSet(CONFIG_FLAG_NO_LEVEL_METER))
  //  return;
  // 0xD0 = level meter, hi nibble = channel index, low = level (F=clip, E=top)
  int x;

  double decay = 0.0;
  if (m_mcu_meter_lastrun) {
    decay =
        VU_BOTTOM * (double)(now - m_mcu_meter_lastrun) /
        (1.4 * 1000.0); // they claim 1.8s for falloff but we'll underestimate
  }
  m_mcu_meter_lastrun = now;
  for (x = 1; x < 9; x++) {
    MediaTrack *t;

    if (t = Tracks::instance()->getMediaTrackForChannel(x)) {
			auto ts = Tracks::instance()->getTrackStateForMediaTrack(t);
      // check mute/solo state of track(s), maybe the signal is muted
      bool isPlaying = ts->getVUactive();

      int v = 0x0;
      if (isPlaying) {
        v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
        // get peak
        double pp =
            VAL2DB((Track_GetPeakInfo(t, 0) + Track_GetPeakInfo(t, 1)) * 0.5);

        if (m_mcu_meterpos[x - 1] > -VU_BOTTOM * 2)
          m_mcu_meterpos[x - 1] -= decay;

        if (pp < m_mcu_meterpos[x - 1])
          continue;
        m_mcu_meterpos[x - 1] = pp;
        if (pMCU->IsFlagSet(CONFIG_FLAG_PROX)) {
					if (pp < 0.0) {
						if (pp <= -VU_BOTTOM)
							v = 0x0;
						else if (pp <= -VU_BOTTOM_QCON)
							v = 0x1;
						else
							v = (int)((pp + VU_BOTTOM_QCON) * 13.0 / VU_BOTTOM_QCON);
					}
				} else {
					if (pp < 0.0) {
						if (pp <= -VU_BOTTOM)
							v = 0x0;
						else
							v = (int)((pp + VU_BOTTOM) * 13.0 / VU_BOTTOM);
					}
				}
      }

			sendToHardware(pMCU, x - 1, v);
    }
  }
	MeterBridge::updateMasterLEDs(now, pMCU, decay);
}

