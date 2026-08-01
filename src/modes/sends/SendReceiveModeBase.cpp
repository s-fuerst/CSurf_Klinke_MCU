/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "SendReceiveModeBase.h"
#include "SendReceiveMeterBridge.h"
#include "Display.h"
#include "MultiDisplay.h"
#include "HardwareUnit.h"
#include <boost/foreach.hpp>
#include "McuAssert.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include "Tracks.h"

SendReceiveModeBase::SendReceiveModeBase(CCSManager *pManager)
    : CCSMode(pManager), m_flip(false), m_pLastSelectedTrack(NULL),
      m_startWithSend(0) {
  m_pDisplay = pManager->createDisplay(4);
	m_pMeterBridge = new SendReceiveMeterBridge(this);
}

SendReceiveModeBase::~SendReceiveModeBase(void) {
	safe_delete(m_pMeterBridge);
	safe_delete(m_pDisplay);
}

void SendReceiveModeBase::activate() {
  m_pDisplay->clear();

  CCSMode::activate();

  if (selectedTrack() == NULL || selectedTrack() != m_pLastSelectedTrack) {
    m_startWithSend = 0;
    m_pLastSelectedTrack = selectedTrack();
  }

  // size rec-button state to the surface channel count.
  m_recButtonPressed.assign(
      Tracks::instance()->getNumberOfChannelStrips(), false);

  // MultiDisplay needs switchToAll for N>1
  MultiDisplay *md = dynamic_cast<MultiDisplay *>(m_pDisplay);
  if (md)
    md->switchToAll();
  else
    m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
	m_pCCSManager->getMCU()->enableMCUMeters(true);
}

void SendReceiveModeBase::updateRecLEDs() {
  getSendInfos(&m_sendInfos, AUTOMODE);
  // widened from 8 to nStrips
  const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  for (int iInfo = 0; iInfo < nStrips; iInfo++) {
    if (m_startWithSend + iInfo < m_sendInfos.size()) {
			int mode = *((int *)m_sendInfos[m_startWithSend + iInfo]);
			if (mode == -1) {
				mode = AUTO_MODE_TRIM;
				setSendInfo(AUTOMODE, m_startWithSend + iInfo, &mode);
			}
				
      m_pCCSManager->setRecLED(
          this, iInfo + 1,
          mode == AUTO_MODE_READ || mode == AUTO_MODE_TRIM ? LED_OFF : LED_ON);
		}
    else
      m_pCCSManager->setRecLED(this, iInfo + 1, LED_OFF);
  }
}

void SendReceiveModeBase::updateSoloLEDs() {
  getSendInfos(&m_sendInfos, MONO);
  // widened from 8 to nStrips
  const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  for (int iInfo = 0; iInfo < nStrips; iInfo++) {
    if (m_startWithSend + iInfo < m_sendInfos.size())
      m_pCCSManager->setSoloLED(
          this, iInfo + 1,
          *((bool *)m_sendInfos[m_startWithSend + iInfo]) ? LED_ON : LED_OFF);
    else
      m_pCCSManager->setSoloLED(this, iInfo + 1, LED_OFF);
  }
}

void SendReceiveModeBase::updateMuteLEDs() {
  getSendInfos(&m_sendInfos, MUTE);
  // widened from 8 to nStrips
  const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  for (int iInfo = 0; iInfo < nStrips; iInfo++) {
    if (m_startWithSend + iInfo < m_sendInfos.size())
      m_pCCSManager->setMuteLED(
          this, iInfo + 1,
          *((bool *)m_sendInfos[m_startWithSend + iInfo]) ? LED_ON : LED_OFF);
    else
      m_pCCSManager->setMuteLED(this, iInfo + 1, LED_OFF);
  }
}

void SendReceiveModeBase::updateFaders() {
  // if (m_flip)
  //   getSendInfos(&m_sendInfos, PAN);
  // else
  //   getSendInfos(&m_sendInfos, VOL);
  double vol;
  double pan;

  // widened from 8 to nStrips
  const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  for (int iInfo = 1; iInfo <= nStrips; iInfo++) {
    if (m_startWithSend + iInfo <= m_sendInfos.size()) {
      int sendIdx = calcSendIdxGet(m_startWithSend + iInfo - 1);
      if (m_flip) {
        getTrackUIVol(selectedTrack(), sendIdx, &vol, &pan);
        m_pCCSManager->setFader(this, iInfo, panToInt14(pan));
      } else {
        getTrackUIVol(selectedTrack(), sendIdx, &vol, &pan);
        m_pCCSManager->setFader(this, iInfo, volToInt14(vol));
      }
    } else
      m_pCCSManager->setFader(this, iInfo, 0);

    // set master fader to selectedTrack value
    if (selectedTrack() != NULL) {
      m_pCCSManager->setFader(
          this, 0,
          volToInt14(
              m_pCCSManager->getMCU()->GetSurfaceVolume(selectedTrack())));
    } else {
      m_pCCSManager->setFader(this, 0, 0);
    }
  }
}

void SendReceiveModeBase::updateVPOTs() {
  if (m_flip)
    getSendInfos(&m_sendInfos, VOL);
  else
    getSendInfos(&m_sendInfos, PAN);

  // widened from 8 to nStrips
  const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  for (int iInfo = 0; iInfo < nStrips; iInfo++) {

    VPOT_LED *pVPOT = m_pCCSManager->getVPOT(iInfo + 1);
    if (m_startWithSend + iInfo < m_sendInfos.size()) {
			if (m_flip)
				m_pCCSManager->getVPOT(iInfo+1)->setMode(VPOT_LED::FROM_LEFT);
			else
				m_pCCSManager->getVPOT(iInfo+1)->setMode(VPOT_LED::FROM_MIDDLE_POINT);
			
      if (m_flip)
        pVPOT->setValueFromChar(
            volToChar(*((double *)m_sendInfos[m_startWithSend + iInfo])));
      else
        pVPOT->setValueFromChar(
            panToChar(*((double *)m_sendInfos[m_startWithSend + iInfo])));

    } else {
			m_pCCSManager->getVPOT(iInfo+1)->setMode(VPOT_LED::OFF);
      pVPOT->setValue(0);
    }
  }
}

void SendReceiveModeBase::trackName(MediaTrack *trackid, const char *pName) {
  if (trackid == selectedTrack()) {
    writeTrackName(strlen(m_pSendOrReceiveText));
  }
}

void SendReceiveModeBase::writeTrackName(int startPos) {
  CSurf_MCU *pMCU = m_pCCSManager->getMCU();
  MultiDisplay *md = dynamic_cast<MultiDisplay *>(m_pDisplay);
  int nUnits = pMCU->numUnits();
  for (int unit = 0; unit < nUnits; unit++) {
    Display *display = m_pDisplay;
    if (md) {
      if (unit >= (int)md->children().size())
        continue;
      display = md->children()[unit];
    }

    HardwareUnit *hardwareUnit = pMCU->unitForChannel(unit * 8 + 1);
    // On a two-row unit, held REC buttons temporarily use the upper row for
    // automation modes instead of the normal mode/track header.
    if ((!hardwareUnit || !hardwareUnit->isProX()) &&
        (isRecButtonPressedOnUnit(unit) ||
         m_pCCSManager->getNumFadersTouched()))
      continue;
    display->changeText(hardwareUnit && hardwareUnit->isProX() ? 2 : 0,
                        startPos, pMCU->GetTrackName(selectedTrack()),
                        44 - startPos);
  }
}

bool SendReceiveModeBase::isRecButtonPressedOnUnit(int unit) const {
  int firstChannel = unit * 8;
  int lastChannel = std::min(firstChannel + 8,
                             (int)m_recButtonPressed.size());
  for (int channel = firstChannel; channel < lastChannel; channel++)
    if (m_recButtonPressed[channel])
      return true;
  return false;
}

void SendReceiveModeBase::updateDisplay() {
  if (selectedTrack() == NULL) {
    m_pDisplay->changeTextFullLine(0, "You must select a single track.", true);
    m_pDisplay->clearLine(1);
    m_pDisplay->clearLine(2);
    m_pDisplay->clearLine(3);
  } else {
    m_pDisplay->clearLine(0);
    m_pDisplay->clearLine(1);
    m_pDisplay->clearLine(2);
    m_pDisplay->clearLine(3);

    CSurf_MCU *pMCU = m_pCCSManager->getMCU();
    MultiDisplay *md = dynamic_cast<MultiDisplay *>(m_pDisplay);
    const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
    getSendInfos(&m_sendInfos, AUTOMODE);
    std::vector<void *> autoModes = m_sendInfos;
    getSendInfos(&m_sendInfos, TRACK);
    std::vector<void *> tracks = m_sendInfos;

    const int headerLength = strlen(m_pSendOrReceiveText);
    const int nUnits = pMCU->numUnits();
    for (int unit = 0; unit < nUnits; unit++) {
      Display *display = m_pDisplay;
      if (md) {
        if (unit >= (int)md->children().size())
          continue;
        display = md->children()[unit];
      }
      HardwareUnit *hardwareUnit = pMCU->unitForChannel(unit * 8 + 1);
      bool isProX = hardwareUnit && hardwareUnit->isProX();
      int headerRow = isProX ? 2 : 0;
      if (isProX ||
          (!isRecButtonPressedOnUnit(unit) &&
           !m_pCCSManager->getNumFadersTouched())) {
        display->changeText(headerRow, 0, m_pSendOrReceiveText, headerLength);
        display->changeText(headerRow, isProX ? 34 : 46, "solo=mono", 19);
      }
    }
    writeTrackName(headerLength);

    for (int iInfo = 0; iInfo < nStrips; iInfo++) {
      int channel = iInfo + 1;
      HardwareUnit *hardwareUnit = pMCU->unitForChannel(channel);
      bool isProX = hardwareUnit && hardwareUnit->isProX();
      bool showRecAutoModes =
          !isProX && isRecButtonPressedOnUnit((channel - 1) / 8);
      bool showFaderValues =
          !showRecAutoModes && m_pCCSManager->getNumFadersTouched();
      bool hasSend = m_startWithSend + iInfo < (int)tracks.size();

      if (!hasSend) {
        if (isProX) {
          m_pDisplay->changeField(0, channel, "");
          m_pDisplay->changeField(1, channel, "");
          m_pDisplay->changeField(3, channel, "");
        } else {
          m_pDisplay->changeField(1, channel, "");
        }
        continue;
      }

      int sendIdx = calcSendIdxGet(m_startWithSend + iInfo);
      double vol;
      double pan;
      getTrackUIVol(selectedTrack(), sendIdx, &vol, &pan);

      if (isProX) {
        int mode = *((int *)autoModes[m_startWithSend + iInfo]);
        if (m_pCCSManager->getVPotTouched(channel)) {
          if (m_flip)
            m_pDisplay->showDB(1, channel, vol);
          else
            m_pDisplay->showPan(1, channel, pan);
        } else {
          const char *modeText = "";
          switch (mode) {
          case AUTO_MODE_READ:  modeText = "Read";  break;
          case AUTO_MODE_LATCH: modeText = "Latch"; break;
          case AUTO_MODE_TRIM:  modeText = "Trim";  break;
          case AUTO_MODE_WRITE: modeText = "Write"; break;
          case AUTO_MODE_TOUCH: modeText = "Touch"; break;
          }
          m_pDisplay->changeField(1, channel, modeText);
        }
        if (m_flip)
          m_pDisplay->showPan(3, channel, pan);
        else
          m_pDisplay->showDB(3, channel, vol);
        m_pDisplay->changeField(
            0, channel,
            pMCU->GetTrackName((MediaTrack *)tracks[m_startWithSend + iInfo]));
      } else if (showRecAutoModes) {
        int mode = *((int *)autoModes[m_startWithSend + iInfo]);
        const char *modeText = "";
        switch (mode) {
        case AUTO_MODE_READ:  modeText = "Read";  break;
        case AUTO_MODE_LATCH: modeText = "Latch"; break;
        case AUTO_MODE_TRIM:  modeText = "Trim";  break;
        case AUTO_MODE_WRITE: modeText = "Write"; break;
        case AUTO_MODE_TOUCH: modeText = "Touch"; break;
        }
        // Held REC replaces the normal header with automation modes while
        // retaining the destination track names on the lower row.
        m_pDisplay->changeField(0, channel, modeText);
        m_pDisplay->changeField(
            1, channel,
            pMCU->GetTrackName((MediaTrack *)tracks[m_startWithSend + iInfo]));
      } else if (showFaderValues) {
        // Fader touch uses the upper row for values and retains destination
        // track names on the lower row, matching the REC-held layout.
        if (m_flip)
          m_pDisplay->showPan(0, channel, pan);
        else
          m_pDisplay->showDB(0, channel, vol);
        m_pDisplay->changeField(
            1, channel,
            pMCU->GetTrackName((MediaTrack *)tracks[m_startWithSend + iInfo]));
      } else {
        m_pDisplay->changeField(
            1, channel,
            pMCU->GetTrackName((MediaTrack *)tracks[m_startWithSend + iInfo]));
      }
    }

    // The master fader display is only present on a ProX main unit.  Field 9
    // is local to that unit, so it must bypass MultiDisplay's strip routing.
    HardwareUnit *mainUnit = pMCU->firstTransportUnit();
    if (mainUnit && mainUnit->isProX()) {
      Display *mainDisplay = m_pDisplay;
      if (md)
        mainDisplay = md->mainChild();
      if (mainDisplay) {
        mainDisplay->changeField(2, 9, pMCU->GetTrackName(selectedTrack()));
        mainDisplay->showDB(3, 9, pMCU->GetSurfaceVolume(selectedTrack()));
      }
    }
  }
}

const char *SendReceiveModeBase::stringForESendInfo(ESendInfo sendInfo) {
  switch (sendInfo) {
  case TRACK:
    return "";
  case MUTE:
    return "B_MUTE";
  case PHASE:
    return "B_PHASE";
  case MONO:
    return "B_MONO";
  case VOL:
    return "D_VOL";
  case PAN:
    return "D_PAN";
  case AUTOMODE:
		return "I_AUTOMODE";
  }
  return NULL;
}

bool SendReceiveModeBase::buttonRec(int channel, bool pressed) {
	// bound to availableChannels(); state array is sized in activate().
	ASSERT(channel > 0 && channel <= m_pCCSManager->getMCU()->availableChannels());
	if ((int)m_recButtonPressed.size() < channel)
		m_recButtonPressed.resize(channel, false); // defensive (e.g. future dynamic width)
	m_recButtonPressed[channel - 1] = pressed;
	
  if (pressed) {
    int sendNr = m_startWithSend + channel - 1;
    int *pOldState = (int *)getSendInfo(AUTOMODE, sendNr);
		if (pOldState) {
			int newMode = AUTO_MODE_TRIM;
			if (*pOldState == AUTO_MODE_TRIM || *pOldState == AUTO_MODE_READ) 
				newMode = AUTO_MODE_TOUCH;

			setSendInfo(AUTOMODE, sendNr, (void *)&newMode);

			ThemeLayout_RefreshAll();
		}
  }

  return true;
}

bool SendReceiveModeBase::buttonFaderBanks(int button, bool pressed) {
  if (pressed == false)
    return true;

  // bank window widened from 8 to nStrips (channel scroll stays ±1).
  const int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  switch (button) {
  case B_BANK_UP:
    // No more sends beyond the current window -> ignore.
    if (m_startWithSend + nStrips >= (int)m_sendInfos.size())
      return true;
    m_startWithSend += nStrips;
    break;
  case B_BANK_DOWN:
    m_startWithSend -= nStrips;
    break;
  case B_CHANNEL_UP:
    m_startWithSend++;
    break;
  case B_CHANNEL_DOWN:
    m_startWithSend--;
    break;
  }
  // Clamp against the current send list.
  if (m_sendInfos.empty())
    m_startWithSend = 0;
  else if (m_startWithSend < 0)
    m_startWithSend = 0;
  else if (m_startWithSend + 1 > (int)m_sendInfos.size())
    m_startWithSend = (int)m_sendInfos.size() - 1;

  updateEverything();

  return true;
}

bool SendReceiveModeBase::buttonMute(int channel, bool pressed) {
  if (pressed) {
    int sendNr = m_startWithSend + channel - 1;
    bool *pOldState = (bool *)getSendInfo(MUTE, sendNr);
    if (pOldState) {
      bool newState = !*pOldState;
      setSendInfo(MUTE, sendNr, (void *)&newState);

      m_pCCSManager->setMuteLED(this, channel, newState ? LED_ON : LED_OFF);
    }
  }

  return true;
}

bool SendReceiveModeBase::buttonSolo(int channel, bool pressed) {
  if (pressed) {
    int sendNr = m_startWithSend + channel - 1;
    bool *pOldState = (bool *)getSendInfo(MONO, sendNr);
    if (pOldState) {
      bool newState = !*pOldState;
      setSendInfo(MONO, sendNr, (void *)&newState);

      m_pCCSManager->setSoloLED(this, channel, newState ? LED_ON : LED_OFF);
    }
  }

  return true;
}

bool SendReceiveModeBase::fader(int channel, int value) {
  if (channel == 0 && selectedTrack()) {
		//		m_pCCSManager->elementTouched(FADER, channel, true);
    CSurf_SetSurfaceVolume(
        selectedTrack(),
        CSurf_OnVolumeChange(selectedTrack(), int14ToVol(value), false), NULL);
  } else {
    int sendIdx = calcSendIdxSet(m_startWithSend + channel - 1);
    if (m_flip) {
      double newVal = int14ToPan(value);
      SetTrackSendUIPan(selectedTrack(), sendIdx, newVal, 0);
    } else {
      double newVal = int14ToVol(value);
      SetTrackSendUIVol(selectedTrack(), sendIdx, newVal, 0);
    }
		m_pCCSManager->setFader(this, channel, value);
  }

  return true;
}

bool SendReceiveModeBase::faderTouched(int channel, bool touched) {
  if (touched == false) {
    int sendIdx = calcSendIdxSet(m_startWithSend + channel - 1);
    if (m_flip) {
      double newVal = int14ToPan(m_pCCSManager->getFaderPos(channel));
      SetTrackSendUIPan(selectedTrack(), sendIdx, newVal, 1);
    } else {
      double newVal = int14ToVol(m_pCCSManager->getFaderPos(channel));
      SetTrackSendUIVol(selectedTrack(), sendIdx, newVal, 1);
    }
  }

  return true;
}

bool SendReceiveModeBase::vpotMoved(int channel, int numSteps) {
  int sendNr = m_startWithSend + channel - 1;
  double *pOldState;
  if (m_flip)
    pOldState = (double *)getSendInfo(VOL, sendNr);
  else
    pOldState = (double *)getSendInfo(PAN, sendNr);

  if (pOldState) {
    if (m_pCCSManager->getVPOT(channel)->isPressed()) {
      numSteps *= 5;
    }
    double newState = *pOldState + numSteps / 40.f;
		if (m_flip)
			newState = std::min(newState, 4.);
		else
			newState = std::min(newState, 1.);

    newState = std::max(newState, -1.);

    if (m_flip) {
      setSendInfo(VOL, sendNr, (void *)&newState, WAIT_FOR_MORE_MOVEMENT);
      m_pCCSManager->getVPOT(channel)->setValueFromChar(volToChar(newState));
    } else {
      setSendInfo(PAN, sendNr, (void *)&newState, WAIT_FOR_MORE_MOVEMENT);
      m_pCCSManager->getVPOT(channel)->setValueFromChar(panToChar(newState));
    }
  }

  return true;
}

bool SendReceiveModeBase::buttonFlip(bool pressed) {
  if (pressed) {
    m_flip = !m_flip;
  }

  updateFlipLED();
  updateFaders();
  updateVPOTs();

  return true;
}

void SendReceiveModeBase::updateFlipLED() {
  m_pCCSManager->setFlipLED(this, m_flip);
}

void SendReceiveModeBase::frameUpdate() {
  updateFaders();
  updateVPOTs();
	updateRecLEDs();
  updateSoloLEDs();
  updateMuteLEDs();
  updateDisplay();
  m_pMeterBridge->updateMeterBridge(m_pCCSManager->getMCU());
}

bool SendReceiveModeBase::somethingTouched(bool touched) {
  if (!touched) {
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_SHOW_SEND, 0);
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_SHOW_SEND, 0);
  }

  return true;
}

int SendReceiveModeBase::getNumSends() {
  getSendInfos(&m_sendInfos, TRACK);
  return m_sendInfos.size();
}

bool SendReceiveModeBase::setAutoMode(AutoMode mode) {
	bool ret = false;

  // iterate actual pressed-state (sized to nStrips), not a fixed 1..8.
  for (size_t i = 0; i < m_recButtonPressed.size(); i++) {
		if (m_recButtonPressed[i]) {
			int sendNr = m_startWithSend + (int)i;

			setSendInfo(AUTOMODE, sendNr, (void *)&mode);

			ThemeLayout_RefreshAll();

			ret = true;
 		}
	}

  return ret;
}
