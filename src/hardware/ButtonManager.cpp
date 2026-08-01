/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ButtonManager.h"
#include "boost/bind.hpp"

ButtonManager::ButtonManager(CSurf_MCU *pMCU) : m_pMCU(pMCU) {
  m_signalFrameConnection =
    pMCU->connect2FrameSignal(boost::bind(&ButtonManager::frame, this, _1));
  reset();
}

ButtonManager::~ButtonManager(void) { m_signalFrameConnection.disconnect(); }

void ButtonManager::ensureUnitState() {
  int n = m_pMCU->numUnits();
  if (n > 0 && (int)m_perUnitState.size() < n) {
    int old = (int)m_perUnitState.size();
    m_perUnitState.resize(n);
    for (int u = old; u < n; u++) {
      memset(&m_perUnitState[u].pressed, 0, NUM_BUTTONS);
      memset(&m_perUnitState[u].doublepressed, 0, NUM_BUTTONS);
      memset(&m_perUnitState[u].hold_used, 0, NUM_BUTTONS);
      memset(&m_perUnitState[u].pressed_time, 0, NUM_BUTTONS * sizeof(DWORD));
      m_perUnitState[u].last_code = -1;
      m_perUnitState[u].last_time = 0;
    }
  }
}

void ButtonManager::reset() {
#ifdef KLINKE
  m_comboChUpHeld = false;
  m_comboBankHeld = false;
  m_comboSession = false;
#endif
  memset(&m_button_pressed, 0, 128);
  memset(&m_button_pressed_time, 0, 128 * sizeof(DWORD));
  m_button_last = -1; // init for isLastButton()
  m_button_last_time = 0;
  for (int u = 0; u < (int)m_perUnitState.size(); u++) {
    memset(&m_perUnitState[u].pressed, 0, NUM_BUTTONS);
    memset(&m_perUnitState[u].doublepressed, 0, NUM_BUTTONS);
    memset(&m_perUnitState[u].hold_used, 0, NUM_BUTTONS);
    memset(&m_perUnitState[u].pressed_time, 0, NUM_BUTTONS * sizeof(DWORD));
    m_perUnitState[u].last_code = -1;
    m_perUnitState[u].last_time = 0;
  }
}

typedef bool (CSurf_MCU::*MidiHandlerFunc)(MIDI_event_t *);

struct ButtonHandler {
  unsigned int evt_min;
  unsigned int evt_max; // inclusive
  MidiHandlerFunc func;
  MidiHandlerFunc func_dc;
};

bool ButtonManager::dispatchMidiEvent(MIDI_event_t *evt, int unitIndex) {
  if (!evt || evt->size < 3)
    return false;

  unsigned char status = evt->midi_message[0] & 0xf0;

  // MCU button release is Note-On velocity 0 (0x90). On Linux the
  // MIDI stack sometimes rewrites that to Note-Off (0x80); normalise back.
  bool is_note_off = (status == 0x80);
  if (is_note_off) {
    status = 0x90;
    evt->midi_message[0] = (evt->midi_message[0] & 0x0f) | 0x90;
    evt->midi_message[2] = 0;  // release: velocity 0
  }

  if (status != 0x90)
    return false;

  // lazily size per-unit arrays, clamp unitIndex
  ensureUnitState();
  if (unitIndex < 0 || unitIndex >= (int)m_perUnitState.size())
    unitIndex = 0;

  PerUnitButtonState &st = m_perUnitState[unitIndex];

  static const int nPressOnlyHandlers = 19;
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
    {0x72, 0x79, &CSurf_MCU::OpenFXFavorite, NULL},
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
  if (evt_code >= NUM_BUTTONS)
    return false;

#if 0
  char buf[512];
  sprintf( buf, "   0x%08x %02x %02x %02x %02x 0x%08x 0x%08x %s", evt_code,
	   evt->midi_message[0], evt->midi_message[1], evt->midi_message[2], evt->midi_message[3],
	   handlers[0].evt_min, handlers[0].evt_max, 
	   handlers[0].evt_min <= evt_code && evt_code <= handlers[0].evt_max ? "yes" : "no" );
  UpdateMackieDisplay( 0, buf, 56 );
#endif

  DWORD now = timeGetTime();
  bool pressed = (evt->midi_message[2] >= 0x40);

  // per-unit pressed state for double-click/long-press isolation
  st.pressed[evt_code] = pressed;
  st.pressed_time[evt_code] = pressed ? now : 0;

  // Shared fallback for isButtonPressed() backward compat
  m_button_pressed[evt_code] = pressed;
  m_button_pressed_time[evt_code] = pressed ? now : 0;

#ifdef KLINKE
  // Hand-off combo (only the Platform M+ = KLINKE_COMBO_UNIT_INDEX has the
  // bank/channel buttons): hold Channel up (0x31), press Bank down (0x2e) to
  // hand control TO this extension, Bank up (0x2f) to hand it to Schaltmix.
  if (unitIndex == KLINKE_COMBO_UNIT_INDEX) {
    if (evt_code == 0x31) {
      m_comboChUpHeld = pressed;
      if (!pressed && !m_comboBankHeld)
        m_comboSession = false;
    } else if (evt_code == 0x2e || evt_code == 0x2f) {
      const bool chUp = m_comboChUpHeld || m_comboSession ||
	(st.pressed_time[0x31] > 0 &&
	 now - st.pressed_time[0x31] < 300);
      if (chUp) {
        if (pressed) {
          m_comboSession = true;
          m_comboBankHeld = true;
          m_pMCU->setSurfaceEnabled(evt_code == 0x2e); // Bank down -> enabled
        } else {
          m_comboBankHeld = false;
          if (!m_comboChUpHeld)
            m_comboSession = false;
        }
        return true; // swallow: no banking while doing the combo
      }
    }
  }
  // inactive: Run() only forwards the combo notes from the M+; consume them
  // here when they are not a valid combo (unit 1 is processed normally)
  if (unitIndex == KLINKE_COMBO_UNIT_INDEX && !m_pMCU->isSurfaceEnabled())
    return true;
#endif

  // For these events we only want to track button press
  if (pressed) {
    st.hold_used[evt_code] = false;
    // Double-click detection is per-unit: the same local note code on a
    // DIFFERENT unit is a different button and must not count as a
    // double-click. Otherwise the second press is swallowed by the
    // double-click path (select has no func_dc) and the action is lost —
    // e.g. selecting a plugin on unit 2 right after pressing unit 1's
    // select (both send evt_code 0x18..0x1f) within DOUBLE_CLICK_INTERVAL.
    bool double_click = (int)evt_code == st.last_code &&
      now - st.last_time < DOUBLE_CLICK_INTERVAL;
    st.last_code = evt_code;
    st.last_time = now;
    // global fallback kept for isLastButton() backward compat
    m_button_last = evt_code;
    m_button_last_time = now;
    if (double_click)
      st.doublepressed[evt_code] = true;

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
      if (!st.hold_used[evt_code] || pressed) {
        if (st.doublepressed[evt_code]) {
          st.doublepressed[evt_code] = pressed;
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

  // iterate per-unit button state for long-press detection
  ensureUnitState();
  for (int unit = 0; unit < (int)m_perUnitState.size(); unit++) {
    PerUnitButtonState &st = m_perUnitState[unit];
    for (unsigned int button = 0; button < NUM_BUTTONS; button++) {
      if (st.pressed[button] && !st.hold_used[button] &&
          st.pressed_time[button] > 0 &&
          st.pressed_time[button] < (time - 333)) {
        for (int i = 0; i < nPressAndHoldHandlers; i++) {
          if (pressAndHoldHandlers[i].evt_min <= button &&
              button <= pressAndHoldHandlers[i].evt_max) {
            int localChannel = button - pressAndHoldHandlers[i].evt_min + 1;
            // translate local → global channel for extender units
            int globalChannel = localChannel + unit * 8;
            if ((m_pMCU->*pressAndHoldHandlers[i].func)(globalChannel)) {
              st.hold_used[button] = true;
            }
          }
        }
      }
    }
  }
}
