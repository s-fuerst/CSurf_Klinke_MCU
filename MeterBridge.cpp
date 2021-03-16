/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "MeterBridge.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "Tracks.h"
#include "boost\foreach.hpp"

MeterBridge::MeterBridge() {
  int x;
  for (x = 0; x < sizeof(m_mcu_meterpos) / sizeof(m_mcu_meterpos[0]); x++) {
    m_mcu_meterpos[x] = -100000.0;
		m_last_sent_meterpos[x] = -1;
	}
  m_mcu_meter_lastrun = 0;
}

void MeterBridge::updateMasterLEDs(DWORD now, CSurf_MCU *pMCU, double decay) {
	if (pMCU->IsFlagSet(CONFIG_FLAG_PROX)) {
		int x;
		int v = 0x0;
		for (x = 0; x < 2; x++) {
			v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
			double pp =
				VAL2DB(Track_GetPeakInfo(GetMasterTrack(NULL), x));

			if (m_mcu_meterpos[8 + x] > -VU_BOTTOM * 2)
				m_mcu_meterpos[8 + x] -= decay;
			
			if (pp > m_mcu_meterpos[8 + x]) {
				m_mcu_meterpos[8 + x] = pp;
			}
			else {
				pp = m_mcu_meterpos[8 + x];
			}
			
			if (pp < 0.0) {
				if (pp <= -VU_BOTTOM)
					v = 0x0;
				else
					v = (int)((pp + VU_BOTTOM_QCON) * 13.0 / VU_BOTTOM_QCON);
			}
			sendToHardware(pMCU, 8 + x, v);
		}
	}
}

void MeterBridge::sendToHardware(CSurf_MCU *pMCU, int pos, short meter) {
	ASSERT(pos >=0 && pos < 10);

	if (meter > 12)
		meter = 12;
	if (meter < 0)
		meter = 0;
	
	if (m_last_sent_meterpos[pos] == meter)
		return;

	m_last_sent_meterpos[pos] = meter;

	if (pos < 8)
		pMCU->SendMidi(0xD0, (pos << 4) | meter, 0, -1);
	else
		pMCU->SendMidi(0xD1, ((pos - 8) << 4) | meter, 0, -1);
}

