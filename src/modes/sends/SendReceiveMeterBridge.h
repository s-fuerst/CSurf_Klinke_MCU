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
	bool alsoOnDisplay() { return true; }
  void updateMeterBridge(CSurf_MCU *pMCU);
	void updateMasterLEDs(CSurf_MCU *pMCU, double decay);
private:
	SendReceiveModeBase *m_pSendMode = NULL;
};

#endif
