/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "Transport.h"
#include "csurf_mcu.h" 

class CSurf_MCU;

Transport::Transport(CSurf_MCU *p_mcu) {
  m_client = POSITION;
  m_clientButtonPressed = POSITION;
  m_direction = NO_REEL;
  m_pMCU = p_mcu;
  m_ffwdPressTime = 0;
  m_rewindPressTime = 0;
}

void Transport::setClient(Client new_client) {
  m_client = new_client;
  updateLeds();
}

void Transport::handleButton(Client button, bool buttonDown) {
  if (buttonDown)
    m_clientButtonPressed = button;
  else {
    m_clientButtonPressed = POSITION;
    if (m_pMCU->IsLastButton(B_MARKER) || m_pMCU->IsLastButton(B_NUDGE)) {
      if (button != m_client)
        setClient(button);
      else
        setClient(POSITION);
    }
  }
}

void Transport::updateLeds() {
  m_pMCU->SetLED(B_MARKER, m_client == MARKER ? LED_ON : LED_OFF);
  m_pMCU->SetLED(B_NUDGE, m_client == NUDGE ? LED_ON : LED_OFF);
}

static double calcNextRepeatTime(double lastTime) {
  double ret = REPEAT_TIME_REDUCING_FACTOR * lastTime;
  if (ret < 20) {
    ret = 20;
  }
  return ret;
}

void Transport::rewind() {
  if (m_client == POSITION) {
    if (m_pMCU->IsModifierPressed(VK_SHIFT)) {
      if (m_pMCU->IsModifierPressed(VK_ALT)) {
        m_pMCU->GetRegion()->GetFromActualRegion(true);
        m_pMCU->GetRegion()->SetStart(GetCursorPosition());
        m_pMCU->GetRegion()->Set(true);
        return;
      } else if (m_pMCU->IsModifierPressed(VK_OPTION)) {
        m_pMCU->GetRegion()->GetFromActualRegion(false);
        m_pMCU->GetRegion()->SetStart(GetCursorPosition());
        m_pMCU->GetRegion()->Set(false);
        return;
      }
    } else if (m_pMCU->IsModifierPressed(VK_OPTION)) {
      m_pMCU->GetRegion()->GetFromActualRegion(false);
      SetEditCurPos(m_pMCU->GetRegion()->GetStart(), true, false);
      return;
    } else if (m_pMCU->IsModifierPressed(VK_ALT)) {
      m_pMCU->GetRegion()->GetFromActualRegion(true);
      SetEditCurPos(m_pMCU->GetRegion()->GetStart(), true, false);
      return;
    } else if (m_pMCU->IsModifierPressed(VK_CONTROL)) {
      SetEditCurPos(0, true, false);
      return;
    }
  }

  if (m_direction == REWIND) {
    if (m_pMCU->IsModifierPressed(VK_SHIFT))
      SetEditCurPos(m_pMCU->MoveInBeats(GetCursorPosition(), -1), true, false);
    else
      SetEditCurPos(m_pMCU->MoveInBars(GetCursorPosition(), -1), true, false);

    m_pMCU->ScheduleAction(timeGetTime() + (DWORD)m_repeatTime,
                           &CSurf_MCU::CallTransportRewind);
    m_repeatTime = calcNextRepeatTime(m_repeatTime);
  }
}

void Transport::forward() {
  if (m_client == POSITION) {
    if (m_pMCU->IsModifierPressed(VK_SHIFT)) {
      if (m_pMCU->IsModifierPressed(VK_ALT)) {
        m_pMCU->GetRegion()->GetFromActualRegion(true);
        m_pMCU->GetRegion()->SetEnd(GetCursorPosition());
        m_pMCU->GetRegion()->Set(true);
        return;
      } else if (m_pMCU->IsModifierPressed(VK_OPTION)) {
        m_pMCU->GetRegion()->GetFromActualRegion(false);
        m_pMCU->GetRegion()->SetEnd(GetCursorPosition());
        m_pMCU->GetRegion()->Set(false);
        return;
      }
    } else if (m_pMCU->IsModifierPressed(VK_OPTION)) {
      m_pMCU->GetRegion()->GetFromActualRegion(false);
      SetEditCurPos(m_pMCU->GetRegion()->GetEnd(), true, false);
      return;
    } else if (m_pMCU->IsModifierPressed(VK_ALT)) {
      m_pMCU->GetRegion()->GetFromActualRegion(true);
      SetEditCurPos(m_pMCU->GetRegion()->GetEnd(), true, false);
      return;
    } else if (m_pMCU->IsModifierPressed(VK_CONTROL)) {
      SendMessage(g_hwnd, WM_COMMAND, ID_GOTO_PROJECT_END, 0);
      return;
    }
  }

  if (m_direction == FORWARD) {
    if (m_pMCU->IsModifierPressed(VK_SHIFT))
      SetEditCurPos(m_pMCU->MoveInBeats(GetCursorPosition(), 1), true, false);
    else
      SetEditCurPos(m_pMCU->MoveInBars(GetCursorPosition(), 1), true, false);

    m_pMCU->ScheduleAction(timeGetTime() + (DWORD)m_repeatTime,
                           &CSurf_MCU::CallTransportForward);
    m_repeatTime = calcNextRepeatTime(m_repeatTime);
  }
}

void Transport::rewindButton(bool down) {
  if (down)
    m_rewindPressTime = timeGetTime();

  switch (getActualClient()) {
  case POSITION:
    if (down) {
      startReel(REWIND);
      rewind();
    } else {
      endReel();
    }
    break;
  case MARKER:
    if (down) {
      if (m_pMCU->IsModifierPressed(VK_OPTION)) {
        if (m_pMCU->IsModifierPressed(VK_SHIFT))
          m_pMCU->GetRegion()->SwapRegionBackward(false);
        else
          m_pMCU->GetRegion()->PreviousRegion(false);
      } else if (m_pMCU->IsModifierPressed(VK_ALT)) {
        if (m_pMCU->IsModifierPressed(VK_CONTROL))
          m_pMCU->GetRegion()->SwapRegionBackward(true);
        else
          m_pMCU->GetRegion()->PreviousRegion(true);
      } else if (m_pMCU->IsModifierPressed(VK_CONTROL)) {
        SetEditCurPos(0, true, false);
      } else {
        SendMessage(g_hwnd, WM_COMMAND, ID_MARKER_PREV, 0);
      }
    }
    break;
  case NUDGE:
    break;
  }
}

void Transport::forwardButton(bool down) {
  if (down)
    m_ffwdPressTime = timeGetTime();

  switch (getActualClient()) {
  case POSITION:
    if (down) {
      startReel(FORWARD);
      forward();
    } else {
      endReel();
    }
    break;
  case MARKER:
    if (down) {
      if (m_pMCU->IsModifierPressed(VK_OPTION)) {
        if (m_pMCU->IsModifierPressed(VK_SHIFT))
          m_pMCU->GetRegion()->SwapRegionForward(false);
        else
          m_pMCU->GetRegion()->NextRegion(false);
      } else if (m_pMCU->IsModifierPressed(VK_ALT)) {
        if (m_pMCU->IsModifierPressed(VK_CONTROL))
          m_pMCU->GetRegion()->SwapRegionForward(true);
        else
          m_pMCU->GetRegion()->NextRegion(true);
      } else if (m_pMCU->IsModifierPressed(VK_CONTROL)) {
        SendMessage(g_hwnd, WM_COMMAND, ID_GOTO_PROJECT_END, 0);
      } else {
        SendMessage(g_hwnd, WM_COMMAND, ID_MARKER_NEXT, 0);
      }
    }
    break;
  case NUDGE:
    break;
  }
}

void Transport::playButton(bool down) {
  if (down) {
    if (m_pMCU->IsModifierPressed(VK_ALT)) {
      Region *pRegion;
      if (m_pMCU->IsModifierPressed(VK_SHIFT))
        pRegion = new Region(m_pMCU->MoveInBeats(GetCursorPosition(), -.5),
                             m_pMCU->MoveInBeats(GetCursorPosition(), .5));
      else
        pRegion = new Region(m_pMCU->MoveInBars(GetCursorPosition(), -.5),
                             m_pMCU->MoveInBars(GetCursorPosition(), .5));
      pRegion->Set(true);
      if (m_pMCU->GetRepeatState() == false) {
        SendMessage(g_hwnd, WM_COMMAND, IDC_REPEAT, 0);
      }
      CSurf_OnPlay();
    } else {
      CSurf_OnPlay();
    }
  }
}

void Transport::startReel(Direction dir) {
  m_playStateBeforeReel = GetPlayState();
  if (m_playStateBeforeReel == 1) { // is playing at the moment
    SetEditCurPos(GetPlayPosition(), true, false);
    CSurf_OnStop();
  }
  m_direction = dir;
  m_repeatTime = STARTING_REPEAT_TIME;
}

void Transport::endReel() {
  m_direction = NO_REEL;
  if (m_playStateBeforeReel == 1) { // was playing before start to reel
    CSurf_OnPlay();
  }
}

void Transport::recordButton(bool down) {
  if (down)
    CSurf_OnRecord();
}

void Transport::stopButton(bool down) {
  if (down) {
    if (m_pMCU->IsModifierPressed(VK_CONTROL) ||
        m_pMCU->IsModifierPressed(VK_ALT) ||
        m_pMCU->IsModifierPressed(VK_OPTION) ||
        m_pMCU->IsModifierPressed(VK_SHIFT))
      CSurf_OnStop();
    else
      SendMessage(g_hwnd, WM_COMMAND, ID_STOP_AND_SAVE_MEDIA, 0);
  }
}

Transport::Client Transport::getActualClient() {
  if (m_clientButtonPressed != POSITION)
    return m_clientButtonPressed;

  return m_client;
}
