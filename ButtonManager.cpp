/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "ButtonManager.h"
#include "boost/bind.hpp"

ButtonManager::ButtonManager(CSurf_MCU *pMCU) : m_pMCU(pMCU) {
  m_signalFrameConnection =
      pMCU->connect2FrameSignal(boost::bind(&ButtonManager::frame, this, _1));
}

ButtonManager::~ButtonManager(void) { m_signalFrameConnection.disconnect(); }

void ButtonManager::reset() {
  memset(&m_button_pressed, 0, 128);
  memset(&m_button_doublepressed, 0, 128);
  memset(&m_button_hold_used, 0, 128);
  memset(&m_button_pressed_time, 0, 128 * sizeof(DWORD));
}

typedef bool (CSurf_MCU::*MidiHandlerFunc)(MIDI_event_t *);

struct ButtonHandler {
  unsigned int evt_min;
  unsigned int evt_max; // inclusive
  MidiHandlerFunc func;
  MidiHandlerFunc func_dc;
};

bool ButtonManager::dispatchMidiEvent(MIDI_event_t *evt) {
  if ((evt->midi_message[0] & 0xf0) != 0x90)
    return false;

  static const int nPressOnlyHandlers = 18;
  static const ButtonHandler pressOnlyHandlers[nPressOnlyHandlers] = {
      // Press down only events
      {0x08, 0x0f, NULL, &CSurf_MCU::OnSoloDC},
      {0x18, 0x1f, NULL, &CSurf_MCU::OnChannelSelectDC},
      {0x35, 0x35, &CSurf_MCU::OnSMPTEBeats, NULL},
      {0x36, 0x3d, &CSurf_MCU::OnFunctionKey, NULL},
      {0x3e, 0x45, &CSurf_MCU::OnGlobalViewKeys, NULL},
      {0x4a, 0x4e, &CSurf_MCU::OnAutoMode, NULL},
      {0x50, 0x50, &CSurf_MCU::OnSave, NULL},
      {0x51, 0x51, &CSurf_MCU::OnUndo, NULL},
      {0x52, 0x52, &CSurf_MCU::OnCancel, NULL},
      {0x56, 0x56, &CSurf_MCU::OnCycle, NULL},
      {0x57, 0x57, &CSurf_MCU::OnDropButton, NULL},
      {0x59, 0x59, &CSurf_MCU::OnClick, NULL},
      {0x5a, 0x5a, &CSurf_MCU::OnGlobalSoloButton, NULL},
      {0X5b, 0x5f, NULL, &CSurf_MCU::OnTransportDC},
      {0x64, 0x64, &CSurf_MCU::OnZoom, NULL},
      {0x65, 0x65, &CSurf_MCU::OnScrub, NULL},
      {0x71, 0x71, &CSurf_MCU::ResetAllFaderTouch, NULL},
  };

  //  static const int nReleaseOnlyHandlers = 1;
  //  static const ButtonHandler releaseOnlyHandlers[nReleaseOnlyHandlers] = {
  //    { 0x33, 0x33, &CSurf_MCU::OnGlobal,             NULL },
  //      // Release Only Handler
  //  };

  // release will not be send, if also a pressAndHoldHandler exist and an
  // holdEvent is send
  static const int nPressAndReleaseHandlers = 16;
  static const ButtonHandler pressAndReleaseHandlers[nPressAndReleaseHandlers] =
      {
          // Press and release events
          {0x00, 0x07, &CSurf_MCU::OnRecArm, &CSurf_MCU::OnRecArmDC},
          {0x08, 0x0f, &CSurf_MCU::OnSolo},
          {0x10, 0x17, &CSurf_MCU::OnMute},
          {0x18, 0x1f, &CSurf_MCU::OnChannelSelect},
          {0x20, 0x27, &CSurf_MCU::OnRotaryEncoderPush},
          {0x28, 0x2d, &CSurf_MCU::OnVPOTAssign},
          {0x2e, 0x31, &CSurf_MCU::OnBankChannel},
          {0x32, 0x32, &CSurf_MCU::OnFlip},
          {0x33, 0x33, &CSurf_MCU::OnGlobal},
          {0x34, 0x34, &CSurf_MCU::OnNameValue, &CSurf_MCU::OnNameValueDC},
          {0x46, 0x49, &CSurf_MCU::OnKeyModifier},
          {0x54, 0x54, &CSurf_MCU::OnMarker},
          {0x55, 0x55, &CSurf_MCU::OnNudge},
          {0x5b, 0x5f, &CSurf_MCU::OnTransport},
          {0x60, 0x63, &CSurf_MCU::OnScroll},
          {0x68, 0x70, &CSurf_MCU::OnTouch},
      };

  unsigned int evt_code = evt->midi_message[1]; // get_midi_evt_code( evt );

#if 0
  char buf[512];
  sprintf( buf, "   0x%08x %02x %02x %02x %02x 0x%08x 0x%08x %s", evt_code,
					 evt->midi_message[0], evt->midi_message[1], evt->midi_message[2], evt->midi_message[3],
					 handlers[0].evt_min, handlers[0].evt_max, 
					 handlers[0].evt_min <= evt_code && evt_code <= handlers[0].evt_max ? "yes" : "no" );
  UpdateMackieDisplay( 0, buf, 56 );
#endif

  DWORD now = timeGetTime();
  // button down is handled at the end of the function
  bool pressed = (evt->midi_message[2] >= 0x40);
  m_button_pressed[evt_code] = pressed;
  m_button_pressed_time[evt_code] = m_button_pressed[evt_code] ? now : 0;
  // For these events we only want to track button press
  if (pressed) {
    m_button_hold_used[evt_code] = false;
    // Check for double click
    bool double_click = (int)evt_code == m_button_last &&
                        now - m_button_last_time < DOUBLE_CLICK_INTERVAL;
    m_button_last = evt_code;
    m_button_last_time = now;
    if (double_click)
      m_button_doublepressed[evt_code] = true;

    // Find event handler
    for (int i = 0; i < nPressOnlyHandlers; i++) {
      ButtonHandler bh = pressOnlyHandlers[i];
      if (bh.evt_min <= evt_code && evt_code <= bh.evt_max) {
        // Try double click first
        if (double_click && bh.func_dc != NULL)
          if ((m_pMCU->*bh.func_dc)(evt))
            return true;

        // Single click (and unhandled double clicks)
        if (bh.func != NULL)
          if ((m_pMCU->*bh.func)(evt))
            return true;
      }
    }
  }

  // For these events we want press and release
  for (int i = 0; i < nPressAndReleaseHandlers; i++)
    if (pressAndReleaseHandlers[i].evt_min <= evt_code &&
        evt_code <= pressAndReleaseHandlers[i].evt_max)
      // release will not be send, if also a pressAndHoldHandler exist and an
      // holdEvent is send
      if (!m_button_hold_used[evt_code] || pressed) {
        if (m_button_doublepressed[evt_code]) {
          m_button_doublepressed[evt_code] = pressed;
          if (pressAndReleaseHandlers[i].func_dc != NULL &&
              (m_pMCU->*pressAndReleaseHandlers[i].func_dc)(evt))
            return true;
        } else {
          if (pressAndReleaseHandlers[i].func != NULL &&
              (m_pMCU->*pressAndReleaseHandlers[i].func)(evt))
            return true;
        }
      }

  return false;
}

typedef bool (CSurf_MCU::*ButtonFunc)(int);

struct ButtonHandlerLong {
  unsigned int evt_min;
  unsigned int evt_max; // inclusive
  ButtonFunc func;
};

void ButtonManager::frame(DWORD time) {
  static const int nPressAndHoldHandlers = 1;
  static const ButtonHandlerLong pressAndHoldHandlers[nPressAndHoldHandlers] = {
      // Press and hold events
      {0x18, 0x1f, &CSurf_MCU::OnChannelSelectLong},
  };

  for (unsigned int button = 0; button < NUM_BUTTONS; button++) {
    if (m_button_pressed[button] && !m_button_hold_used[button] &&
        m_button_pressed_time[button] > 0 &&
        m_button_pressed_time[button] < (time - 333)) {
      for (int i = 0; i < nPressAndHoldHandlers; i++) {
        if (pressAndHoldHandlers[i].evt_min <= button &&
            button <= pressAndHoldHandlers[i].evt_max) {
          if ((m_pMCU->*pressAndHoldHandlers[i].func)(
                  button - pressAndHoldHandlers[i].evt_min + 1)) {
            m_button_hold_used[button] = true;
          }
        }
      }
    }
  }
}
