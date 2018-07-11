/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifndef MCU_DISPLAYTRACKMETER
#define MCU_DISPLAYTRACKMETER

#include "Display.h"

class DisplayTrackMeter: public Display { 
 private:
  double m_mcu_meterpos[8];
  DWORD m_mcu_meter_lastrun;

 public:
  DisplayTrackMeter(DisplayHandler* pDisplayHandler, int numRows);
  bool onlyOnMainUnit() {return false; }
  bool hasMeter() { return true; }
  void updateTrackMeter(DWORD now);
  void changeText(int row, int pos, const char *text, int pad);
  void changeField(int row, int field, const char* text);
};

#endif
