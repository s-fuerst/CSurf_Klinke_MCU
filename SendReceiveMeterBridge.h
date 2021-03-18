/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
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
  void updateMeterBridge(DWORD now, CSurf_MCU *pMCU);
private:
	SendReceiveModeBase *m_pSendMode = NULL;
};

#endif
