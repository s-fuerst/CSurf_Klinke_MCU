/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once

class CSurf_MCU;

class VPOT_LED {
public:
  enum MODE { SINGLE = 0, FROM_LEFT = 2, FROM_MIDDLE_POINT = 1, OFF = 3 };

  VPOT_LED();
  virtual ~VPOT_LED(void);

  void init(CSurf_MCU *pMCU, int globalTrack, bool isProX = false);

  //      void setMCU(CSurf_MCU pMCU) {m_pMCU = pMCU;}
  //      void setTrack(int track) {m_track = track;}
  void setMode(const MODE mode);
  void setValue(
      const int value); // between 1 - 11, or 0 if no led should be turned on
  void setValueFromChar(const char value) { setValue(1 + ((value * 11) >> 7)); }
  void setBottom(const bool bot);
  void setAll(const MODE mode, const int value, const bool bot);

  void setPressed(const bool pressed) { m_pressed = pressed; }
  bool isPressed() { return m_pressed; }

private:
  void updateLEDs();
  template <class T> void changeState(T *pDest, const T source);

  // the position of the VPOT leds (0 for the first channel)
  int m_track;
  // 0-15?
  int m_value;
  // see enum-definition above
  MODE m_mode;
  // the single LED at the bottom of the VPOT
  bool m_bottom;

  // per-VPOT ProX capability.
  bool m_isProX;

  // the ccsmanager set m_pressed to true, as long as the VPOT is pressed
  bool m_pressed;

  // something has changed, so the leds must be updated in next updateLEDs
  bool m_mustUpdate;

  CSurf_MCU *m_pMCU;
};
