/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#ifndef MCU_DISPLAYHANDLER
#define MCU_DISPLAYHANDLER

#include <vector>

//#include "csurf.h"
class HardwareUnit;
class Display;
class MIDI_Message;

class DisplayHandler {
public:
  enum EnumMCUType { MCU = 0, MCU_EX, PROX };

private:
  HardwareUnit *m_pUnit;
  Display *m_pActualDisplay;
  EnumMCUType m_mcuType;
  bool m_isProX; // per-unit ProX capability
  // widened from fixed [9] to dynamic vector (9 = 8 strips + master per unit)
  std::vector<bool> m_metersEnabled;
  bool m_wait;

  Display *m_pHardwareState;

  void addHeader(MIDI_Message *pmm, int row);

public:
  DisplayHandler(HardwareUnit *pUnit, EnumMCUType mcuType, bool isProX);
  ~DisplayHandler();
  //      void init();

  void switchTo(Display *pDisplay);
  void updateDisplay(Display *pDisplay, int row, int pos, char const *text,
                     int pad, bool forceUpdate = false);
  Display *getDisplay() const { return m_pActualDisplay; }
  void enableMCUMeter(bool enable);
  void enableMCUMeter(int channel, bool enable); // channel is 1 based
  bool getMetersEnabled(int channel) const { return m_metersEnabled[channel]; }
  HardwareUnit *getUnit() const { return m_pUnit; }

  void waitForMoreChanges(bool block);
  void sendDifferences(Display *pDisplay, int row, const char *text);
  void sendToHardware(int row, int pos, char const *text, int len);
};

#endif
