/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#ifndef MCU_SENDRECEIVEMETERBRIDGE
#define MCU_SENDRECEIVEMETERBRIDGE

#include "csurf.h"

#include "MeterBridge.h"

class SendReceiveModeBase;

class SendReceiveMeterBridge : public MeterBridge {
public:
  SendReceiveMeterBridge(SendReceiveModeBase *pSendMode);
	// The LCD bars are disabled in Send/Receive mode: the track names are
	// more important to read on row 1 than the emulated level bars. The
	// hardware meter LEDs (strip meters via 0xD0 and ProX master meters)
	// still work — they are driven regardless of this flag.
	bool alsoOnDisplay() { return false; }
  void updateMeterBridge(CSurf_MCU *pMCU);
	void updateMasterLEDs(CSurf_MCU *pMCU, double decay);
private:
	SendReceiveModeBase *m_pSendMode = NULL;
};

#endif
