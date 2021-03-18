/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifndef MCU_METERBRIDGE
#define MCU_METERBRIDGE

#include "reaper_plugin.h" // DWORD

#define VU_BOTTOM 70
#define VU_BOTTOM_QCON 22

class CSurf_MCU;

class MeterBridge {
public:
  MeterBridge();
  virtual bool alsoOnDisplay() { return false; }
  virtual void updateMeterBridge(DWORD now, CSurf_MCU *pMCU) = 0;

protected:
	virtual void updateMeter(int iChannel, MediaTrack *pMT, CSurf_MCU *pMCU,
													 double decay, int pin);
	virtual void updateMasterLEDs(CSurf_MCU *pMCU, double decay);
	void sendToHardware(CSurf_MCU *pMCU, int pos, short meter);

  double m_mcu_meterpos[10]; // 9 and 10 are for the master
  DWORD m_mcu_meter_lastrun;
};

#endif

