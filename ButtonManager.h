#include "csurf_mcu.h"

#pragma once

class CSurf_MCU;

#define NUM_BUTTONS 128

class ButtonManager {
public:
  ButtonManager(CSurf_MCU *pMCU);
  ~ButtonManager(void);

  void reset();

  bool dispatchMidiEvent(MIDI_event_t *evt);

  bool isButtonPressed(int i) { return m_button_pressed[i]; }
  int isLastButton(int button) { return button == m_button_last; }

  void frame(DWORD time);

private:
  int m_button_last;
  DWORD m_button_last_time;

  bool m_button_pressed[NUM_BUTTONS];
  bool m_button_doublepressed[NUM_BUTTONS];
  bool m_button_hold_used[NUM_BUTTONS]; // when pressing a button this will send
                                        // to false and after sending a hold
                                        // event, this will be set to true
  DWORD m_button_pressed_time[NUM_BUTTONS];

  connection m_signalFrameConnection;

  CSurf_MCU *m_pMCU;
};
