/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifndef MCU_DISPLAYHANDLER
#define MCU_DISPLAYHANDLER

//#include "csurf.h"
class CSurf_MCU;
class Display;
class MIDI_Message;

class DisplayHandler {
public:
  enum EnumMCUType { MCU = 0, MCU_EX, PROX };

private:
  CSurf_MCU *m_pMCU;
  Display *m_pActualDisplay;
  EnumMCUType m_mcuType;
  bool m_metersEnabled[9];
  bool m_wait;

  Display *m_pHardwareState;

  void addHeader(MIDI_Message *pmm, int row);

public:
  DisplayHandler(CSurf_MCU *pMCU, EnumMCUType mcuType);
  ~DisplayHandler();
  //      void init();

  void switchTo(Display *pDisplay);
  void updateDisplay(Display *pDisplay, int row, int pos, char const *text,
                     int pad, bool forceUpdate = false);
  Display *getDisplay() const { return m_pActualDisplay; }
  void enableMeter(bool enable);
  void enableMeter(int channel, bool enable); // channel is 1 based
  bool getMetersEnabled(int channel) const { return m_metersEnabled[channel]; }
  CSurf_MCU *getMCU() const { return m_pMCU; }

  void waitForMoreChanges(bool block);
  void sendDifferences(Display *pDisplay, int row, const char *text);
  void sendToHardware(int row, int pos, char const *text, int len);
};

#endif
