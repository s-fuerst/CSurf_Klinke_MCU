/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#ifndef MCU_METERBRIDGE
#define MCU_METERBRIDGE

#include "reaper_plugin.h" // DWORD
#include <vector>

#define VU_BOTTOM 44
#define VU_SIGNAL_LED 70

class CSurf_MCU;

class MeterBridge {
public:
  MeterBridge();
  virtual ~MeterBridge() {}
  virtual bool alsoOnDisplay() { return false; }
  virtual void updateMeterBridge(CSurf_MCU *pMCU) = 0;

  // While a channel's display-value is "locked" (e.g. a fader is touched or
  // a VPOT value is briefly shown), the mode asks the bridge to skip the
  // LCD meter bar for that channel so the value text on row 1 stays visible.
  // This only matters on units with meters-on-display; on other units
  // showMeterOnDisplay() returns early anyway. Indexed by global channel.
  void setDisplayMeterSuppressed(int channel, bool suppressed);

protected:
  virtual void updateMeter(int iChannel, MediaTrack *pMT, CSurf_MCU *pMCU,
                           double decay, int pin);
  virtual void updateMasterLEDs(CSurf_MCU *pMCU, double decay);
  void sendToHardware(CSurf_MCU *pMCU, int pos, short meter);
  void showMeterOnDisplay(CSurf_MCU *pMCU, int channel, short meter);

  // strip-meter state dynamically sized (availableChannels() entries).
  // Master-meter state is separate (L/R, always 2 values, never indexed by strip pos).
  std::vector<double> m_stripMeterPos;
  double m_masterMeterPos[2];
  void ensureStripMeterState(int channelCount);

  // Per-global-channel flag set by the mode each frame. When true,
  // showMeterOnDisplay() skips the LCD bar so a value shown on row 1
  // (touched fader or briefly-shown VPOT value) is not overwritten.
  std::vector<bool> m_displayMeterSuppressed;

  DWORD m_mcu_meter_lastrun;
};

#endif
