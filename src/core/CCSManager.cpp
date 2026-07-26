/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#ifdef _WIN32
#include <windows.h>
#endif
#include "CCSManager.h"
#include "MultiTrackMode.h"
#include "PanMode.h"
#include "PerformanceMode.h"
#include "CommandMode.h"
#include "Display.h"
#include "MultiDisplay.h"
#include "HardwareUnit.h"
#include "SendMode.h"
#include "ReceiveMode.h"
#include "PlugMode.h"
#include "csurf_mcu.h"
#include "McuAssert.h"
#include "CCSModesEditor.h"
#include "Options.h"
#include "JuceHeader.h"

#define TOUCHED_MS 2000;

CCSManager::CCSManager(CSurf_MCU *pMCU) {
  m_pMCU = pMCU;
  m_pCommandMode = new CommandMode(this);
  m_pPanMode = new PanMode(this);
  m_pPerformanceMode = new PerformanceMode(this);
  m_pSendMode = new SendMode(this);
  m_pReceiveMode = new ReceiveMode(this);
  m_pPlugMode = new PlugMode(this);

  m_pEditor = new CCSModesEditor(this);

  m_pActualMode = m_pPanMode;

  m_pSwitchBackMode = NULL;
  m_switchBack = false;
  m_selectorActive = false;
  m_optionActive = NULL;

  // size arrays to availableChannels()+1 (0=master, 1..N*8 strips)
  int nCh = pMCU->availableChannels() + 1;
  m_pVPOTS = new VPOT_LED[nCh];
  m_faderTouched = new bool[nCh];
  m_vpotTouched = new bool[nCh];
  m_faderTouchedTill = new DWORD[nCh];
  m_vpotTouchedTill = new DWORD[nCh];
  for (int i = 0; i < nCh; i++) {
    // VPOT 0 (unused master slot) has no owning unit.
    // VPOTs 1..N*8 get per-unit ProX flag from their owning unit.
    bool isProX = (i > 0) ? pMCU->unitForChannel(i)->isProX() : false;
    m_pVPOTS[i].init(getMCU(), i, isProX);
    m_faderTouched[i] = false;
    m_vpotTouchedTill[i] = 0;
    m_faderTouchedTill[i] = 0;
    m_vpotTouched[i] = false;
  }
  m_stateFlip = LED_UNKNOWN;
  m_stateGlobalView = LED_UNKNOWN;
  memset(m_stateAssignmentDisplay, 1, 2);

  m_iNumSoloButtonsPressed = 0;
  m_iNumMuteButtonsPressed = 0;
  m_iNumSelectButtonsPressed = 0;

  m_lastTime = 0;
}

CCSManager::~CCSManager(void) {
	m_pEditor->deleteWindow();
	
  safe_delete(m_pEditor); // editor must be delete before modes
  safe_delete(m_pCommandMode);
  safe_delete(m_pPanMode);
  safe_delete(m_pPerformanceMode);
  safe_delete(m_pSendMode);
  safe_delete(m_pReceiveMode);
  safe_delete(m_pPlugMode);
  safe_delete_array(m_pVPOTS);
  safe_delete_array(m_faderTouched);
  safe_delete_array(m_vpotTouched);
  safe_delete_array(m_faderTouchedTill);
  safe_delete_array(m_vpotTouchedTill);
  m_pActualMode = NULL;
}

void CCSManager::init() {
#if EASY_DEBUG
  if (!m_pMCU->IsExtender()) {
    m_pEditor->setMainComponent(m_pPlugMode->createEditorComponent(), true);
    changeMode(m_pPlugMode);
  }
#endif

  m_pActualMode->activate();
}

void CCSManager::setVPOTMode(VPOT_LED::MODE mode) {
  for (int iChannel = 0; iChannel <= m_pMCU->availableChannels(); iChannel++)
    m_pVPOTS[iChannel].setMode(mode);
}

bool CCSManager::buttonVPOTassign(int button, bool pressed) {
  // open editor (in the case that one exist) when the assign button is pressed
  // in combination with alt
  if (m_pMCU->IsModifierPressed(VK_ALT)) {
    if (pressed) {
      switch (button) {
      case B_VPOT_EQ:
        m_pEditor->setMainComponent(m_pCommandMode, true);
        break;
      case B_VPOT_PLUG:
        m_pEditor->setMainComponent(m_pPlugMode, true);
        break;
      case B_VPOT_PAN:
        m_pEditor->setMainComponent(m_pPanMode, true);
        break;
      }
    }
    return true;
  }

  CCSMode *pNewMode = m_pActualMode;
  MediaTrack *pSingleTrack = Tracks::instance()->getSelectedSingleTrack();

  if (pressed) {
    m_pSwitchBackMode = m_pActualMode;
    m_switchBack = false;

    switch (button) {
    case B_VPOT_PAN:
      pNewMode = m_pPanMode;
      break;
    case B_VPOT_EQ:
      pNewMode = m_pCommandMode;
      break;
      //      case B_VPOT_INSTRUMENT:
      //        pNewMode = m_pPerformanceMode;
      //        break;
    case B_VPOT_SEND:
      if (pSingleTrack && m_pActualMode == m_pSendMode &&
          m_pReceiveMode->getNumSends() > 0)
        pNewMode = m_pReceiveMode;
      else {
        if (pSingleTrack && m_pSendMode->getNumSends() > 0) {
          pNewMode = m_pSendMode;
        } else if (pSingleTrack && m_pReceiveMode->getNumSends() > 0) {
          pNewMode = m_pReceiveMode;
        }
      }
      break;
    case B_VPOT_PLUG:
			pNewMode = m_pPlugMode;
      break;
    }
  } else if (m_selectorActive) {
    m_selectorActive = false;
    m_pActualMode->activate();
  } else if (m_switchBack) {
    pNewMode = m_pSwitchBackMode;
  }

  if (pressed && pNewMode == m_pSwitchBackMode &&
      pNewMode->getSelector() != NULL) {
    pNewMode->getSelector()->activateSelector();
    m_selectorActive = true;
  } else {
    changeMode(pNewMode);
  }

  return true;
}

void CCSManager::switchToDisplay(CCSMode *pMode, Display *pDisplay) {
  if (pMode == m_pActualMode && !m_selectorActive && !m_optionActive &&
      !m_pMCU->IsButtonPressed(B_NAME_VALUE)) {
    // MultiDisplay needs switchToAll to activate each child on its own handler
    MultiDisplay *md = dynamic_cast<MultiDisplay *>(pDisplay);
    if (md)
      md->switchToAll();
    else
      m_pMCU->getDisplayHandler()->switchTo(pDisplay);
  }
}

bool CCSManager::canSwitchDisplay(CCSMode *pMode) {
  // Mirrors the guard inside switchToDisplay(). Exposed so callers that need a
  // side effect beside the switch (e.g. PlugMode's clearNonAnchorChildren on
  // global-message displays) can respect the same selector/option/NameValue
  // locks without duplicating the condition.
  return pMode == m_pActualMode && !m_selectorActive && !m_optionActive &&
         !m_pMCU->IsButtonPressed(B_NAME_VALUE);
}

void CCSManager::updateVPOTLeds() {
  MediaTrack *m_pSelectedTrack = Tracks::instance()->getSelectedSingleTrack();

  m_pMCU->SetLED(B_VPOT_EQ,
                 m_pActualMode == m_pCommandMode ? LED_BLINK : LED_ON);
  m_pMCU->SetLED(B_VPOT_PAN, m_pActualMode == m_pPanMode ? LED_BLINK : LED_ON);

  // B_VPOT_SEND
  if (m_pActualMode == m_pReceiveMode || m_pActualMode == m_pSendMode) {
    m_pMCU->SetLED(B_VPOT_SEND, LED_BLINK);
  } else if (m_pSelectedTrack && m_pReceiveMode->getNumSends() > 0 ||
             m_pSendMode->getNumSends() > 0) {
    m_pMCU->SetLED(B_VPOT_SEND, LED_ON);
  } else {
    m_pMCU->SetLED(B_VPOT_SEND, LED_OFF);
  }

  // B_VPOT_PLUG
  if (m_pActualMode == m_pPlugMode) {
    m_pMCU->SetLED(B_VPOT_PLUG, LED_BLINK);
  } else if (m_pPlugMode->getNumPlugsInSelectedTrack()) {
    m_pMCU->SetLED(B_VPOT_PLUG, LED_ON);
  } else {
    m_pMCU->SetLED(B_VPOT_PLUG, LED_OFF);
  }
}

bool CCSManager::buttonFaderBanks(int button, bool pressed) {
  ASSERT(button >= B_BANK_DOWN && button <= B_CHANNEL_UP);
  m_switchBack = true;
  return m_pActualMode->buttonFaderBanks(button, pressed);
}

bool CCSManager::buttonFlip(bool pressed) {
  m_switchBack = true;
  return m_pActualMode->buttonFlip(pressed);
}

bool CCSManager::buttonGView(bool pressed) {
  m_switchBack = true;
  return m_pActualMode->buttonGView(pressed);
}

bool CCSManager::buttonNameValue(bool pressed) {
  m_switchBack = true;
  return m_pActualMode->buttonNameValue(pressed);
}

bool CCSManager::buttonRec(int channel, bool pressed) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  return m_pActualMode->buttonRec(channel, pressed);
}

bool CCSManager::buttonRecDC(int channel, bool pressed) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  return m_pActualMode->buttonRecDC(channel, pressed);
}

bool CCSManager::buttonSelect(int channel, bool pressed) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  pressed ? m_iNumSelectButtonsPressed++ : m_iNumSelectButtonsPressed--;
  return m_pActualMode->buttonSelect(channel, pressed);
}

bool CCSManager::buttonSelectDC(int channel) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  return m_pActualMode->buttonSelectDC(channel);
}

bool CCSManager::buttonSelectLong(int channel) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  return m_pActualMode->buttonSelectLong(channel);
}

bool CCSManager::buttonMute(int channel, bool pressed) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  pressed ? m_iNumMuteButtonsPressed++ : m_iNumMuteButtonsPressed--;
  return m_pActualMode->buttonMute(channel, pressed);
}

bool CCSManager::buttonSolo(int channel, bool pressed) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  pressed ? m_iNumSoloButtonsPressed++ : m_iNumSoloButtonsPressed--;
  return m_pActualMode->buttonSolo(channel, pressed);
}

bool CCSManager::buttonSoloDC(int channel) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;
  return m_pActualMode->buttonSoloDC(channel);
}

bool CCSManager::fader(int channel, int value) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 0 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;

  // per-unit option (the unit owning this channel)
  if (m_pMCU->fakeFaderTouch(channel)) {
    m_faderTouchedTill[channel] = m_lastTime + TOUCHED_MS;
    elementTouched(FADER, channel, true);
    m_pActualMode->faderTouched(channel, true);
  }

  return m_pActualMode->fader(channel, value);
}

bool CCSManager::faderTouched(int channel, bool touched) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 0 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;

  // per-unit option: units with fake touch ignore real touch messages
  if (m_pMCU->fakeFaderTouch(channel)) {
    return true;
  }

  elementTouched(FADER, channel, touched);
  return m_pActualMode->faderTouched(channel, touched);
}

bool CCSManager::vpotMoved(int channel, int numSteps) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;

  m_vpotTouchedTill[channel] = m_lastTime + TOUCHED_MS;

  elementTouched(VPOT, channel, true);

  if (m_optionActive) {
    m_optionActive->move(channel, numSteps);
    return true;
  }

  if (!m_selectorActive) {
    return m_pActualMode->vpotMoved(channel, numSteps);
  }

  return false;
}

bool CCSManager::vpotPressed(int channel, bool pressed) {
  if (channel > 8 && !m_pActualMode->supportsExtendedChannels()) return false;
  ASSERT(channel >= 1 && channel <= m_pMCU->availableChannels());
  m_switchBack = true;

  // always send touched, but at a end time if VPOT was released
  elementTouched(VPOT, channel, true);
  if (pressed) {
    m_vpotTouchedTill[channel] = 0;
  } else {
    m_vpotTouchedTill[channel] = m_lastTime + TOUCHED_MS;
  }

  m_pVPOTS[channel].setPressed(pressed);

  if (m_selectorActive) {
    if (!pressed) {
      if (!m_pActualMode->getSelector()->select(channel - 1)) {
        m_pActualMode->activate();
        m_selectorActive = false;
      }
    }
    return true;
  }

  if (m_optionActive && pressed) {
    m_optionActive->select(channel - 1);
    return true;
  }

  return m_pActualMode->vpotPressed(channel, pressed);
}

void CCSManager::elementTouched(EElement element, int channel, bool touched) {
  bool *touchedArray = (element == FADER) ? m_faderTouched : m_vpotTouched;

  ASSERT(channel >= 0 && channel <= m_pMCU->availableChannels());
  if (touched) {
    if (getElementsTouched() == 0) {
      touchedArray[channel] = touched;
      m_pActualMode->somethingTouched(true);
    }
  } else {
    if (getElementsTouched() == 1) {
      touchedArray[channel] = touched;
      m_pActualMode->somethingTouched(false);
    }
  }

  touchedArray[channel] = touched;

  if (element == FADER) {
    if (getNumFadersTouched() == 1) {
      for (int iChannel = 0; iChannel <= m_pMCU->availableChannels(); iChannel++)
        if (touchedArray[iChannel])
          m_pActualMode->singleFaderTouched(iChannel);
    } else {
      m_pActualMode->singleFaderTouched(0);
    }
  } else {
    if (getNumVPotTouched() == 1) {
      for (int iChannel = 0; iChannel <= m_pMCU->availableChannels(); iChannel++)
        if (touchedArray[iChannel])
          m_pActualMode->singleVPotTouched(iChannel);
    } else {
      m_pActualMode->singleVPotTouched(0);
    }
  }

  //  if (getElementsTouched() == 1) {
  //    for (int iChannel = 1; iChannel < 9; iChannel++)
  //      if (touchedArray[iChannel])
  //        (element == FADER) ? m_pActualMode->singleFaderTouched(iChannel) :
  //        m_pActualMode->singleVPotTouched(iChannel);
  //  } else {
  //    (element == FADER) ? m_pActualMode->singleFaderTouched(0) :
  //    m_pActualMode->singleVPotTouched(0);
  //  }
}

void CCSManager::updateEverything() {
  m_pActualMode->updateFaders();
  updateAllLEDs();
  m_pActualMode->updateVPOTs();
  m_pActualMode->updateAssignmentDisplay();
  m_pActualMode->updateDisplay();
}

void CCSManager::updateFader() { m_pActualMode->updateFaders(); }

void CCSManager::updateVPOTs() { m_pActualMode->updateVPOTs(); }

void CCSManager::updateAllLEDs() {
  m_pActualMode->updateFlipLED();
  m_pActualMode->updateGlobalViewLED();
  m_pActualMode->updateMuteLEDs();
  m_pActualMode->updateRecLEDs();
  m_pActualMode->updateSelectLEDs();
  m_pActualMode->updateSoloLEDs();
}

void CCSManager::updateFlipLED() { m_pActualMode->updateFlipLED(); }

void CCSManager::updateGlobalViewLED() { m_pActualMode->updateGlobalViewLED(); }

void CCSManager::updateMuteLEDs() { m_pActualMode->updateMuteLEDs(); }

void CCSManager::updateRecLEDs() { m_pActualMode->updateRecLEDs(); }

void CCSManager::updateSelectLEDs() { m_pActualMode->updateSelectLEDs(); }

void CCSManager::updateSoloLEDs() { m_pActualMode->updateSoloLEDs(); }

void CCSManager::updateAssignmentDisplay() {
  m_pActualMode->updateAssignmentDisplay();
}

void CCSManager::setFader(CCSMode* pCaller, int channel, int value) {
  CHECKMODEANDCHANNEL

  // Route through CSurf_MCU to the owning HardwareUnit.
  m_pMCU->sendStripFader(channel, value);
}

void CCSManager::setRecLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL
  // strip LED → owning unit via global channel.
  // rec notes 0x00..0x07; local = (channel-1) % 8
  int local = (channel - 1) % 8;
  m_pMCU->setStripLED(channel, local, state);
}

void CCSManager::setSoloLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL
  // solo notes 0x08..0x0f
  int local = (channel - 1) % 8;
  m_pMCU->setStripLED(channel, 0x08 + local, state);
}

void CCSManager::setMuteLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL
  // mute notes 0x10..0x17
  int local = (channel - 1) % 8;
  m_pMCU->setStripLED(channel, 0x10 + local, state);
}

void CCSManager::setSelectLED(CCSMode *pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL
  // select notes 0x18..0x1f
  int local = (channel - 1) % 8;
  m_pMCU->setStripLED(channel, 0x18 + local, state);
}

void CCSManager::setFlipLED(CCSMode *pCaller, int state) {
  CHECKMODE

  if (m_stateFlip != state) {
    m_pMCU->SetLED(B_FLIP, state ? LED_ON : LED_OFF);
    m_stateFlip = state;
  }
}

void CCSManager::setGlobalViewLED(CCSMode *pCaller, int state) {
  CHECKMODE

  if (m_stateGlobalView != state) {
    m_pMCU->SetLED(B_GLOBAL_VIEW, state ? LED_ON : LED_OFF);
    m_stateGlobalView = state;
  }
}

void CCSManager::setAssignmentDisplay(CCSMode *pCaller, const char text[2]) {
  CHECKMODE

  // Normalise to 2 chars (caller may pass shorter strings like "")
  char normalised[2] = {' ', ' '};
  if (text[0]) normalised[0] = text[0];
  if (text[0] && text[1]) normalised[1] = text[1];

  if (memcmp(normalised, m_stateAssignmentDisplay, 2) != 0) {
    // route assignment digits to every transport-capable unit
    // (Mackie main units only; ProX units suppress assignment display).
    // This replaces the old global ProX routing gate.
    for (int i = 0; i < m_pMCU->numUnits(); i++) {
      HardwareUnit *u = m_pMCU->unitForChannel(i * 8 + 1);
      if (!u || !m_pMCU->isTransportUnit(u)) continue;
      if (u->isProX()) continue;
      u->sendMidi(0xB0, 0x40 + 11, normalised[0], -1);
      u->sendMidi(0xB0, 0x40 + 10, normalised[1], -1);
    }
  }
}

void CCSManager::trackListChange() { m_pActualMode->trackListChange(); }

bool CCSManager::isSelectedTrack(MediaTrack *tr) {
  SelectedTrack *i = m_pMCU->GetSelectedTracks();
  while (i) {
    MediaTrack *track = i->track();
    if (track == tr)
      return true;
    i = i->next;
  }
  return false;
}

void CCSManager::trackSelected(MediaTrack *trackid, bool selected) {
  if (selected)
    selectTrack(trackid);
  else
    deselectTrack(trackid);

  // UpdateAutoModes() is now called once per frame in CSurf_MCU::Run().
  // REAPER broadcasts SetSurfaceSelected to all tracks on bank operations, so
  // calling it here fired it once per track and flooded the MCU's MIDI receive
  // buffer (the midiEvents spikes during banking).
}

void CCSManager::selectTrack(MediaTrack *trackid) {
  const GUID *guid = GetTrackGUID(trackid);

  // Empty list, start new list
  SelectedTrack *pTracks = m_pMCU->GetSelectedTracks();
  if (pTracks == NULL) {
    pTracks = new SelectedTrack(trackid);
    m_pMCU->SetSelectedTracks(pTracks);
    return;
  }

  // This track is head of list
  if (guid && !memcmp(&pTracks->guid, guid, sizeof(GUID)))
    return;

  // Scan for track already selected
  SelectedTrack *i = pTracks;
  while (i->next) {
    i = i->next;
    if (guid && !memcmp(&i->guid, guid, sizeof(GUID)))
      return;
  }

  // Append at end of list if not already selected
  i->next = new SelectedTrack(trackid);
}

void CCSManager::deselectTrack(MediaTrack *trackid) {
  const GUID *guid = GetTrackGUID(trackid);

  // Empty list?
  SelectedTrack *pTracks = m_pMCU->GetSelectedTracks();
  if (pTracks) {
    // This track is head of list?
    if (guid && !memcmp(&pTracks->guid, guid, sizeof(GUID))) {
      SelectedTrack *tmp = pTracks;
      pTracks = pTracks->next;
      m_pMCU->SetSelectedTracks(pTracks);
      delete tmp;
    }

    // Search for this track
    else {
      SelectedTrack *i = pTracks;
      while (i->next) {
        if (guid && !memcmp(&i->next->guid, guid, sizeof(GUID))) {
          SelectedTrack *tmp = i->next;
          i->next = i->next->next;
          delete tmp;
          break;
        }
        i = i->next;
      }
    }
  }
}

void CCSManager::trackName(MediaTrack *trackid, const char *pName) {
  m_pActualMode->trackName(trackid, pName);
}

DisplayHandler *CCSManager::getDisplayHandler() {
  return m_pMCU->getDisplayHandler();
}

int CCSManager::getFaderPos(int channel) {
  return m_pMCU->getFaderPos(channel);
}

Display *CCSManager::createDisplay(int numRows) {
  // N=1: plain Display. N>1: returns a MultiDisplay with one child
  // per unit, each backed by its own DisplayHandler.
  if (m_pMCU->numUnits() > 1) {
    MultiDisplay *md = new MultiDisplay(getDisplayHandler(), numRows);
    for (int i = 0; i < m_pMCU->numUnits(); i++) {
      HardwareUnit *u = m_pMCU->unitForChannel(i * 8 + 1);
      if (u) {
        Display *child = new Display(u->displayHandler(), numRows);
        md->addChild(child);
      }
    }
    return md;
  }
  return new Display(getDisplayHandler(), numRows);
}

int CCSManager::getElementsTouched() {
  return getNumFadersTouched() + getNumVPotTouched();
}

int CCSManager::getNumFadersTouched() {
  return getNumTrueArrayEntries(m_faderTouched, m_pMCU->availableChannels() + 1);
}

int CCSManager::getNumVPotTouched() {
  return getNumTrueArrayEntries(m_vpotTouched, m_pMCU->availableChannels() + 1);
}

void CCSManager::resetAllFaderTouch() {
  for (int i = 0; i <= m_pMCU->availableChannels(); i++) {
    m_faderTouched[i] = false;
    m_vpotTouched[i] = false;
  }
}

int CCSManager::getNumTrueArrayEntries(bool *pArray, int size) {
  int numTrue = 0;
  for (int i = 0; i < size; i++) {
    if (pArray[i]) {
      numTrue++;
    }
  }

  return numTrue;
}

void CCSManager::frameUpdate(DWORD time) {
  m_lastTime = time;

  for (int i = 1; i <= m_pMCU->availableChannels(); i++) {
    if (m_vpotTouchedTill[i] > 0 && time > m_vpotTouchedTill[i]) {
      elementTouched(VPOT, i, false);
      m_vpotTouchedTill[i] = 0;
    }
  }

  if (m_pMCU->anyUnitFakeFaderTouch()) {
    for (int i = 0; i <= m_pMCU->availableChannels(); i++) {
      if (m_faderTouchedTill[i] > 0 && time > m_faderTouchedTill[i]) {
        elementTouched(FADER, i, false);
        m_faderTouchedTill[i] = 0;
        m_pActualMode->faderTouched(i, false);
      }
    }
  }

  checkOption();

  updateVPOTLeds();

  m_pActualMode->frameUpdate();
}

void CCSManager::checkOption() {
  if (m_optionActive != NULL && m_pMCU->IsModifierPressed(VK_OPTION) ||
      m_pMCU->IsButtonPressed(B_NAME_VALUE))
    return;

  if (m_pMCU->IsModifierPressed(VK_OPTION)) {
    if (m_pMCU->IsModifierPressed(VK_SHIFT)) {
      if (m_pActualMode->get2ndOptions() != NULL) {
        m_optionActive = m_pActualMode->get2ndOptions();
        m_optionActive->activateSelector();
      }
    } else {
      if (m_pActualMode->getOptions() != NULL) {
        m_optionActive = m_pActualMode->getOptions();
        m_optionActive->activateSelector();
      }
    }
  } else if (m_optionActive != NULL) {
    m_pActualMode->activate();
    m_optionActive = NULL;
  }
}

void CCSManager::setMode(EMode mode) {
  switch (mode) {
  case SEND:
    changeMode(m_pSendMode);
    break;
  case RECEIVE:
    changeMode(m_pReceiveMode);
    break;
  default:
    ASSERT_M(false, "direct mode change must be added for this mode");
    break;
  }
}

void CCSManager::changeMode(CCSMode *pNewMode) {
  if (pNewMode && pNewMode != m_pActualMode) {
    m_pActualMode->deactivate();
    m_pActualMode = pNewMode;
    m_pActualMode->activate();
  }
}

void CCSManager::closeEditorIfOpen(Component *pComponent) {
  m_pEditor->closeWindowAndRemoveComponent(pComponent);
}
