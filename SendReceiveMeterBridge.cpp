/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "SendReceiveMeterBridge.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "Tracks.h"
#include <boost/foreach.hpp>
#include "SendReceiveModeBase.h"

SendReceiveMeterBridge::SendReceiveMeterBridge(SendReceiveModeBase *pSendMode)
    : MeterBridge() {
	m_pSendMode = pSendMode;
}


void SendReceiveMeterBridge::updateMeterBridge(CSurf_MCU * pMCU) {
	DWORD now = pMCU->GetActualFrameTime();
	
	std::vector<void *> sendInfos;
	m_pSendMode->getSendInfos(&sendInfos, TRACK);

  double decay = 0.0;
  if (m_mcu_meter_lastrun) {
    decay =
        VU_BOTTOM * (double)(now - m_mcu_meter_lastrun) /
        (1.4 * 1000.0); // they claim 1.8s for falloff but we'll underestimate
  }
  m_mcu_meter_lastrun = now;

	unsigned offset = m_pSendMode->getChannelOffset();

	for (int iInfo = 0; iInfo < 8; iInfo++) {
		if ((offset + iInfo) < sendInfos.size()) {
			MediaTrack *t = (MediaTrack *)sendInfos[offset + iInfo];
			updateMeter(iInfo + 1, t, pMCU, decay, -1);
		} else
			updateMeter(iInfo + 1, NULL, pMCU, decay, -1);
  }

  updateMasterLEDs(pMCU, decay);
}

void SendReceiveMeterBridge::updateMasterLEDs(CSurf_MCU *pMCU, double decay) {
	if (pMCU->IsFlagSet(CONFIG_FLAG_PROX)) {
		int x;
		int v = 0x0;
		for (x = 0; x < 2; x++) {
			v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
			double pp = -100;
			if (Tracks::instance()->getSelectedSingleTrack())
				pp = VAL2DB(Track_GetPeakInfo(Tracks::instance()->getSelectedSingleTrack(), x));

			if (m_mcu_meterpos[8 + x] > -VU_BOTTOM * 2)
				m_mcu_meterpos[8 + x] -= decay;
			
			if (pp > m_mcu_meterpos[8 + x]) {
				m_mcu_meterpos[8 + x] = pp;
			}
			else {
				pp = m_mcu_meterpos[8 + x];
			}
			
			if (pp < 0.0) {
				if (pp <= -VU_SIGNAL_LED)
					v = 0x0;
				else if (pp <= -VU_BOTTOM)
					v = 0x1;
				else
					v = (int)((pp + VU_BOTTOM) * 11.0 / VU_BOTTOM) + 1;
			}
			sendToHardware(pMCU, 8 + x, v);
		}
	}
}

