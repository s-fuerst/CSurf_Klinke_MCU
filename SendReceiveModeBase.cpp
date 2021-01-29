/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "SendReceiveModeBase.h"
#include "Display.h"
#include <boost/foreach.hpp>
#include "Assert.h"

SendReceiveModeBase::SendReceiveModeBase(CCSManager* pManager) : CCSMode(pManager),
m_flip(false),
m_pLastSelectedTrack(NULL),
m_startWithSend(0)
{
  m_pDisplay = new Display(pManager->getDisplayHandler(), 2);
}

SendReceiveModeBase::~SendReceiveModeBase(void)
{
  safe_delete(m_pDisplay);
}

void SendReceiveModeBase::activate() {
  m_pDisplay->clear();

  CCSMode::activate();

  if (selectedTrack() == NULL || selectedTrack() != m_pLastSelectedTrack) {
    m_startWithSend = 0;
    m_pLastSelectedTrack = selectedTrack();
  }

  m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
}

void SendReceiveModeBase::updateSoloLEDs() {
  getSendInfos(&m_sendInfos, MONO);
  for (unsigned int iInfo = 0; iInfo < 8; iInfo++) {
    if (m_startWithSend + iInfo < m_sendInfos.size())
      m_pCCSManager->setSoloLED(this, iInfo + 1, *((bool*) m_sendInfos[m_startWithSend + iInfo]) ? LED_ON : LED_OFF);
    else
      m_pCCSManager->setSoloLED(this, iInfo + 1, LED_OFF);
  }
}

void SendReceiveModeBase::updateMuteLEDs() {
  getSendInfos(&m_sendInfos, MUTE);
  for (unsigned int iInfo = 0; iInfo < 8; iInfo++) {
    if (m_startWithSend + iInfo < m_sendInfos.size())
      m_pCCSManager->setMuteLED(this, iInfo + 1, *((bool*) m_sendInfos[m_startWithSend + iInfo]) ? LED_ON : LED_OFF);
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

  for (unsigned int iInfo = 1; iInfo < 9; iInfo++) {
    if (m_startWithSend + iInfo <= m_sendInfos.size()) {
	  int sendIdx = calcSendIdx(m_startWithSend + iInfo - 1);
      if (m_flip) {
		GetTrackSendUIVolPan(selectedTrack(), sendIdx, &vol, &pan);
		m_pCCSManager->setFader(this, iInfo, panToInt14(pan));
	  } else {
		GetTrackSendUIVolPan(selectedTrack(), sendIdx, &vol, &pan);
		m_pCCSManager->setFader(this, iInfo, volToInt14(vol));
	  }
	}
	else
	  m_pCCSManager->setFader(this, iInfo, 0);


	// set master fader to selectedTrack value
	if (selectedTrack() != NULL) {
	  m_pCCSManager->setFader(this, 0, volToInt14(m_pCCSManager->getMCU()->GetSurfaceVolume(selectedTrack())));
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

  for (unsigned int iInfo = 0; iInfo < 8; iInfo++) {
    VPOT_LED* pVPOT = m_pCCSManager->getVPOT(iInfo+1);
    if (m_startWithSend + iInfo < m_sendInfos.size()) {
      if (m_flip)
        pVPOT->setValueFromChar(volToChar(*((double*) m_sendInfos[m_startWithSend + iInfo])));
      else
        pVPOT->setValueFromChar(panToChar(*((double*) m_sendInfos[m_startWithSend + iInfo])));
      
      pVPOT->setMode(VPOT_LED::SINGLE);
    }
    else {
      pVPOT->setValue(0);
      pVPOT->setMode(VPOT_LED::FROM_LEFT);
    }
  }
}

void SendReceiveModeBase::trackName(MediaTrack* trackid, const char* pName) {
  if (trackid == selectedTrack()) {
    writeTrackName(strlen(m_pSendOrReceiveText));
  }
}

void SendReceiveModeBase::writeTrackName(int startPos) {
  m_pDisplay->changeText(0, startPos, m_pCCSManager->getMCU()->GetTrackName(selectedTrack()), 44 - startPos);
}

void SendReceiveModeBase::updateDisplay() {
  if (selectedTrack() == NULL) {
    m_pDisplay->changeTextFullLine(0, "You must select a single track.", true);
    m_pDisplay->clearLine(1);
  } else {
    m_pDisplay->changeText(0, 0, m_pSendOrReceiveText, strlen(m_pSendOrReceiveText));
    writeTrackName(strlen(m_pSendOrReceiveText));
    m_pDisplay->changeText(0, 46, "solo=mono", 19);

    getSendInfos(&m_sendInfos, TRACK);
    unsigned int iInfo;
    for (iInfo = 0; (m_startWithSend + iInfo) < m_sendInfos.size() && iInfo < 8; iInfo++) {
      if (m_pCCSManager->getFaderTouched(iInfo + 1) && !m_flip) {
        double* volume  = (double*) getSendInfo(VOL, iInfo + m_startWithSend);

        char text[7];
        double asDB = VAL2DB(*volume);
        if (asDB > -100)
          sprintf(text, "%6.1f", asDB);
        else 
          sprintf(text, "  -inf");

        m_pDisplay->changeField(1, iInfo + 1, text);
      }
      else {
        m_pDisplay->changeField(1, iInfo + 1, m_pCCSManager->getMCU()->GetTrackName((MediaTrack*) m_sendInfos[m_startWithSend + iInfo]));
      }
    }
    while (iInfo < 8) {
      m_pDisplay->changeField(1, iInfo + 1, "");
      iInfo++;
    }
  }
}

const char* SendReceiveModeBase::stringForESendInfo(ESendInfo sendInfo) {
  switch (sendInfo) {
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
  }
  return NULL;
}

bool SendReceiveModeBase::buttonFaderBanks(int button, bool pressed) {
  if (pressed == false) 
    return true;

  switch (button) {
    case B_BANK_UP:
      if ((m_startWithSend + 9) > (int) m_sendInfos.size()) 
        return true;
      m_startWithSend += 8;
      break;
    case B_BANK_DOWN:
      m_startWithSend -= 8;
      break;
    case B_CHANNEL_UP:
      m_startWithSend++;
      break;
    case B_CHANNEL_DOWN:
      m_startWithSend--;
      break;
  }
  if (m_startWithSend < 0)
    m_startWithSend = 0;
  else if (m_startWithSend + 1 > (int) m_sendInfos.size())
    m_startWithSend = (int) m_sendInfos.size() - 1;

  updateEverything();

  return true;
}

bool SendReceiveModeBase::buttonMute(int channel, bool pressed) {
  if (pressed) {
    int sendNr = m_startWithSend + channel - 1;
    bool* pOldState = (bool*) getSendInfo(MUTE, sendNr);
    if (pOldState) {
      bool newState = !*pOldState;
      setSendInfo(MUTE, sendNr, (void*) &newState);

      m_pCCSManager->setMuteLED(this, channel, newState ? LED_ON : LED_OFF);
    }
  }

  return true;
}

bool SendReceiveModeBase::buttonSolo(int channel, bool pressed) {
  if (pressed) {
    int sendNr = m_startWithSend + channel - 1;
    bool* pOldState = (bool*) getSendInfo(MONO, sendNr);
    if (pOldState) {
      bool newState = !*pOldState;
      setSendInfo(MONO, sendNr, (void*) &newState);

      m_pCCSManager->setSoloLED(this, channel, newState ? LED_ON : LED_OFF);
    }
  } 

  return true;
}

bool SendReceiveModeBase::fader(int channel, int value) {
  if (channel == 0 && selectedTrack()) {
    CSurf_SetSurfaceVolume(selectedTrack(),CSurf_OnVolumeChange(selectedTrack(),int14ToVol(value),false),NULL);
  } else {
	int sendIdx = calcSendIdx(m_startWithSend + channel - 1);
    if (m_flip) {
      double newVal = int14ToPan(value);
	  SetTrackSendUIPan(selectedTrack(), sendIdx, newVal, 0);
    } else {
      double newVal = int14ToVol(value);
	  DBOUT("send fader " << sendIdx << " with value " << newVal << "\n");
	  SetTrackSendUIVol(selectedTrack(), sendIdx, newVal, 0);
    }
  }
  m_pCCSManager->setFader(this, channel, value);

  return true;
}

bool SendReceiveModeBase::faderTouched(int channel, bool touched) {
  if (touched == false) {
	int sendIdx = calcSendIdx(m_startWithSend + channel - 1);
	if (m_flip) {
	  double newVal = int14ToPan(m_pCCSManager->getFaderPos(channel));
	  SetTrackSendUIPan(selectedTrack(), sendIdx, newVal, 1);		
	} else {
	  double newVal = int14ToVol(m_pCCSManager->getFaderPos(channel));
	  DBOUT("release fader " << sendIdx << " with value " << newVal << "\n");
	  SetTrackSendUIVol(selectedTrack(), sendIdx, newVal, 1);		
	}
  }

  return true;
}

bool SendReceiveModeBase::vpotMoved(int channel, int numSteps) {
  int sendNr = m_startWithSend + channel - 1;
  double* pOldState;
  if (m_flip) 
    pOldState = (double*) getSendInfo(VOL, sendNr);
  else 
    pOldState = (double*) getSendInfo(PAN, sendNr);

  if (pOldState) {
    if (m_pCCSManager->getVPOT(channel)->isPressed()) {
      numSteps *= 5;
    }
    double newState = *pOldState + numSteps / 40.f;
    newState = min(newState, 1.);
    newState = max(newState, -1.);
    
    if (m_flip) { 
      setSendInfo(VOL, sendNr, (void*) &newState, WAIT_FOR_MORE_MOVEMENT);
      m_pCCSManager->getVPOT(channel)->setValueFromChar(volToChar(newState));
    }
    else {
      setSendInfo(PAN, sendNr, (void*) &newState, WAIT_FOR_MORE_MOVEMENT);
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
  updateSoloLEDs();
  updateMuteLEDs();
  updateDisplay();
}


bool SendReceiveModeBase::somethingTouched(bool touched) {
  if (!touched) {
    SendMessage(g_hwnd,WM_COMMAND,ID_TOGGLE_SHOW_SEND,0);
    SendMessage(g_hwnd,WM_COMMAND,ID_TOGGLE_SHOW_SEND,0);
  }

  return true;
}

int SendReceiveModeBase::getNumSends()
{
  getSendInfos(&m_sendInfos, TRACK);
  return m_sendInfos.size();
}
