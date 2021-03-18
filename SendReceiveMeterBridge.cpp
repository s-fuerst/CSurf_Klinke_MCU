/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "SendReceiveMeterBridge.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "Tracks.h"
#include "boost\foreach.hpp"
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

	for (int iInfo = 0; (offset + iInfo) < sendInfos.size() && iInfo < 8; iInfo++) {
    MediaTrack *t = (MediaTrack *) sendInfos[offset + iInfo];
		updateMeter(iInfo - offset + 1, t, pMCU, decay, -1);
  }

  MeterBridge::updateMasterLEDs(pMCU, decay);
}

