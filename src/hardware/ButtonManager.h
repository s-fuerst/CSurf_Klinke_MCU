/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

#include "csurf_mcu.h"
#include <vector>

#pragma once

class CSurf_MCU;

#define NUM_BUTTONS 128

struct PerUnitButtonState {
  bool pressed[NUM_BUTTONS];
  bool doublepressed[NUM_BUTTONS];
  bool hold_used[NUM_BUTTONS];
  DWORD pressed_time[NUM_BUTTONS];
};

class ButtonManager {
public:
  ButtonManager(CSurf_MCU *pMCU);
  ~ButtonManager(void);

  void reset();

  // unitIndex is the sender unit index (0 = primary, 1+ = extender).
  // Used to isolate per-unit double-click and long-press state.
  bool dispatchMidiEvent(MIDI_event_t *evt, int unitIndex = 0);

  bool isButtonPressed(int i) { return m_button_pressed[i]; }
  int isLastButton(int button) { return button == m_button_last; }

  void frame(DWORD time);

private:
  // lazily size per-unit state to match m_pMCU->numUnits().
  void ensureUnitState();

  int m_button_last;
  DWORD m_button_last_time;

  // shared fallback for backward compat (isButtonPressed)
  bool m_button_pressed[NUM_BUTTONS];
  DWORD m_button_pressed_time[NUM_BUTTONS];

  // Per-unit double-click and long-press state
  std::vector<PerUnitButtonState> m_perUnitState;

  connection m_signalFrameConnection;

  CSurf_MCU *m_pMCU;
};
