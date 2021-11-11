/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include <boost/bind.hpp>
#include <boost\foreach.hpp>
#include "PlugMode.h"
#include "PlugModeComponent.h"
#include "PlugAccess.h"
#include "PlugModeMeterBridge.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include "Display.h"
#include "std_helper.h"
#include <math.h>
#include "PlugModeOptions.h"
#include "PlugMode2ndOptions.h"
#include "PlugWindowManager.h"
#include "PlugMoveWatcher.h"
#include "PlugPresetManager.h"

#define NUM_FAVORITES 16
#define NUM_PRESETS 16
#define TIMETOSWITCHPLUGINMS 2000

PlugMode::PlugMode(CCSManager *pManager)
    : CCSMode(pManager), m_iSingleFaderTouched(0), m_iSingleVPotTouched(0),
      m_pAccess(NULL), m_pPlugEditor(NULL), m_buttonNameValuePressed(false),
      m_followTrack(true), m_lastTimePlugWasSelected(0) {
  m_pAccess = new PlugAccess(this);

  m_pPlugModeOptions = new PlugModeOptions(pManager->getDisplayHandler());
  m_pPlugMode2ndOptions = new PlugMode2ndOptions(pManager->getDisplayHandler());

  m_pParamsDisplay = new Display(pManager->getDisplayHandler(), 4);
  m_pTouchedDisplay = new Display(pManager->getDisplayHandler(), 4);
  m_pValueDisplay = new Display(pManager->getDisplayHandler(), 2);

	m_pMeterBridge = new PlugModeMeterBridge(this);

  m_pSingleTrackMessage = new Display(pManager->getDisplayHandler(), 2);
  m_pSingleTrackMessage->changeTextFullLine(
      0, "You must select a single track.", true);
  m_pSingleTrackMessage->clearLine(1);

  m_pNoPlugMessage = new Display(pManager->getDisplayHandler(), 2);
  m_pNoPlugMessage->clearLine(0);
  m_pNoPlugMessage->changeTextFullLine(1, "No FX exist in selected track.",
                                       true);

  m_pNoPlugSelectedMessage = new Display(pManager->getDisplayHandler(), 2);
  m_pNoPlugSelectedMessage->clearLine(0);
  m_pNoPlugSelectedMessage->changeTextFullLine(1, "You must select a FX.",
                                               true);

  m_pPlugSelector = new PlugSelector(pManager->getDisplayHandler(), this);
  m_pBankPagePlugSelector =
      new BankPagePlugSelector(pManager->getDisplayHandler(), this);

  m_pPresetManager = new PlugPresetManager(pManager->getMCU());

  for (int slot = 0; slot < NUM_FAVORITES; slot++)
    m_favPlugins.push_back(tFav(GUID_NOT_ACTIVE, -1, 0));

  m_projectChangedConnectionId =
      ProjectConfig::instance()->connect2ProjectChangeSignal(
          boost::bind(&PlugMode::projectChanged, this, _1, _2));
  m_plugMovedConnectionId = PlugMoveWatcher::instance()->connectPlugMoveSignal(
      boost::bind(&PlugMode::plugMoved, this, _1, _2, _3, _4));
}

PlugMode::~PlugMode(void) {
  ProjectConfig::instance()->disconnectProjectChangeSignal(
      m_projectChangedConnectionId);
  PlugMoveWatcher::instance()->disconnectPlugMoveSignal(
      m_plugMovedConnectionId);

  safe_delete(m_pPresetManager);
  safe_delete(m_pPlugEditor);
  safe_delete(m_pAccess);
  safe_delete(m_pParamsDisplay);
  safe_delete(m_pTouchedDisplay);
	safe_delete(m_pMeterBridge);
  safe_delete(m_pValueDisplay);
  safe_delete(m_pSingleTrackMessage);
  safe_delete(m_pPlugSelector);
  safe_delete(m_pBankPagePlugSelector);
  safe_delete(m_pNoPlugMessage);
  safe_delete(m_pNoPlugSelectedMessage);
  safe_delete(m_pPlugModeOptions);
  safe_delete(m_pPlugMode2ndOptions);
}

void PlugMode::activate() {
  if (m_followTrack)
    m_pAccess->trackChanged(selectedTrack());

  if ((m_pPlugMode2ndOptions->isOptionSetTo(PMO2_MODE_CHANGE,
                                            PMO2A_OPEN_CLOSE) ||
       m_pPlugMode2ndOptions->isOptionSetTo(PMO2_MODE_CHANGE,
                                            PMO2A_OPEN_CLOSE_MIXER)) &&
      m_pCCSManager->getMCU()->IsButtonPressed(B_VPOT_PLUG)) {
    m_pAccess->openFX();
  }

  switchDisplay();

  CCSMode::activate();
}

void PlugMode::deactivate() {
  if (m_pPlugMode2ndOptions->isOptionSetTo(PMO2_MODE_CHANGE,
                                           PMO2A_OPEN_CLOSE) ||
      m_pPlugMode2ndOptions->isOptionSetTo(PMO2_MODE_CHANGE,
                                           PMO2A_OPEN_CLOSE_MIXER)) {
    m_pAccess->getPlugWindowManager()->closeAll();
  }

  if (m_pPlugMode2ndOptions->isOptionSetTo(PMO2_MODE_CHANGE,
                                           PMO2A_OPEN_CLOSE_MIXER)) {
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_SHOW_MIXER, 0);
  }
}

bool PlugMode::buttonRecDC(int channel, bool pressed) {
  if (!pressed)
    return true;

  channel--; // channel is 0 based for PlugMode releated calls
  if (isModifierPressed(VK_SHIFT))
    channel += 8;

  int randomPresetNr = randomPreset();
  int numFX = TrackFX_GetCount(m_pAccess->getPlugTrack());

  for (int i = 0; i < numFX; i++) {
    handlePresetChange(channel, i, randomPresetNr);
  }

  return true;
}

bool PlugMode::buttonRec(int channel, bool pressed) {
  if (!pressed)
    return true;

  channel--; // channel is 0 based for PlugMode releated calls
  if (isModifierPressed(VK_SHIFT))
    channel += 8;

  handlePresetChange(channel, m_pAccess->getPlugSlot(), randomPreset());

  return true;
}

void PlugMode::handlePresetChange(int presetNr, int slot, int randomPresetNr) {
  String fxGUID =
      GUID2String(TrackFX_GetFXGUID(m_pAccess->getPlugTrack(), slot));

  if (isModifierPressed(VK_CONTROL)) {
    m_pPresetManager->storePreset(m_pAccess->getPlugTrack(), slot, presetNr,
                                  m_pAccess);
    m_lastCalledPreset[fxGUID] = presetNr;
    return;
  }

  if (isModifierPressed(VK_OPTION)) {
    if (isModifierPressed(VK_ALT)) {
      m_pPresetManager->deleteAllPresets(fxGUID);
      m_lastCalledPreset[fxGUID] = -1;
      return;
    }
    m_pPresetManager->deletePreset(fxGUID, presetNr);
    if (presetNr = m_lastCalledPreset[fxGUID])
      m_lastCalledPreset[fxGUID] = -1;
    return;
  }

  if (isModifierPressed(VK_ALT) && randomPresetNr >= 0) {
    m_pPresetManager->recallPreset(m_pAccess->getPlugTrack(), slot,
                                   randomPresetNr);
    m_lastCalledPreset[fxGUID] = randomPresetNr;
    return;
  }

  m_pPresetManager->recallPreset(m_pAccess->getPlugTrack(), slot, presetNr);
  m_lastCalledPreset[fxGUID] = presetNr;
}

// select the preset id for the fx in the given slot
int PlugMode::randomPreset() {
  String fxGUID = GUID2String(
      TrackFX_GetFXGUID(m_pAccess->getPlugTrack(), m_pAccess->getPlugSlot()));

  std::vector<int> idsWithPresets;
  for (int i = 0; i < NUM_PRESETS; i++) {
    if (m_pPresetManager->hasPreset(fxGUID, i)) {
      idsWithPresets.push_back(i);
    }
  }

  return idsWithPresets.empty()
             ? -1
             : idsWithPresets[(int)(idsWithPresets.size() * rand() /
                                    (RAND_MAX + 1.0))];
}

bool PlugMode::buttonSolo(int channel, bool pressed) {
  channel--; // channel is 0 based for PlugMode releated calls

  if (pressed) {
    safe_call(m_pPlugEditor, selectedBankChanged(channel))

        m_pBankPagePlugSelector->select(channel);
  }

  if (pressed && m_pAccess->isBankUsed(channel)) {
    m_pAccess->setSelectedBank(channel);
  }

  if (pressed) {
    m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::BANK);
  } else if (m_pCCSManager->getNumSoloButtonsPressed() == 0) {
    m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::NOTHING);
  }

  return true;
}

bool PlugMode::buttonMute(int channel, bool pressed) {
  channel--; // channel is 0 based for PlugMode releated calls

  if (pressed)
    safe_call(m_pPlugEditor, selectedPageChanged(channel))

        if (pressed && isModifierPressed(VK_SHIFT)) {
      for (int iBank = 0; iBank < 8; iBank++) {
        if (m_pAccess->isPageUsed(iBank, channel)) {
          m_pAccess->setSelectedPage(iBank, channel);
        }
      }
    }
  else if (pressed && m_pAccess->isPageUsedInSelectedBank(channel)) {
    m_pAccess->setSelectedPageInSelectedBank(channel);
  }

  if (pressed) {
    m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::PAGE);
  } else if (m_pCCSManager->getNumMuteButtonsPressed() == 0) {
    m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::NOTHING);
  }

  return true;
}

bool PlugMode::fader(int channel, int value) {
  if (channel > 0)
    safe_call(m_pPlugEditor, selectedChannelChanged(channel - 1, true))

        if (m_followTrack && !selectedTrack()) { // PlugMode works only with a
                                                 // single selectedTrack
      m_pCCSManager->setFader(this, channel, 0);
    }

  if (channel == 0 /* Master fader */) {
    m_pAccess->setParamValueInt(PlugAccess::ElementDesc::DRYWET, 0, value);
  } else {
    m_pAccess->setParamValueInt(PlugAccess::ElementDesc::FADER,
                                channel - 1 /*plugmode stuff is 0 based*/,
                                value);
  }
  m_pCCSManager->setFader(this, channel, value);

  return true;
}

bool PlugMode::singleFaderTouched(int channel) {
  if (channel > 0)
    safe_call(m_pPlugEditor, selectedChannelChanged(channel - 1, true))
        m_iSingleFaderTouched = channel;
  switchDisplay();
  return true;
}

bool PlugMode::singleVPotTouched(int channel) {
  if (channel > 0)
    safe_call(m_pPlugEditor, selectedChannelChanged(channel - 1, false))

        m_iSingleVPotTouched = channel;
  switchDisplay();
  return true;
}

bool PlugMode::vpotMoved(int channel, int numSteps) {
  PMVPot::tSteps *pStepMap = m_pAccess->getParamSteps(channel - 1);
  double val = m_pAccess->getParamValueDouble(PlugAccess::ElementDesc::VPOT,
                                              channel - 1);

  if (pStepMap && !pStepMap->empty()) {
    int index = findIndexFromKeyInMap(val, pStepMap);
    (numSteps < 0) ? index-- : index++;
    if (index >= (signed)pStepMap->size()) {
      index = pStepMap->size() - 1;
    } else if (index < 0) {
      index = 0;
    }
    m_pAccess->setParamValueDouble(PlugAccess::ElementDesc::VPOT, channel - 1,
                                   getNthKeyFromMap(index, pStepMap));
  }
  return true;
}

bool PlugMode::vpotPressed(int channel, bool pressed) {
  if (pressed) {
    switch (m_pBankPagePlugSelector->getWhatToSelect()) {
    case BankPagePlugSelector::BANK:
      buttonSolo(channel, true);
      break;
    case BankPagePlugSelector::PAGE:
      buttonMute(channel, true);
      break;
    case BankPagePlugSelector::PLUG:
      buttonSelect(channel, true);
      break;
    }
  }

  if (pressed) {
    PMVPot::tSteps *pStepMap = m_pAccess->getParamSteps(channel - 1);
    double val = m_pAccess->getParamValueDouble(PlugAccess::ElementDesc::VPOT,
                                                channel - 1);

    if (pStepMap && !pStepMap->empty()) {
      int index = findIndexFromKeyInMap(val, pStepMap);
      isModifierPressed(VK_SHIFT) ? index-- : index++;
      if (index >= (signed)pStepMap->size()) {
        index = 0;
      } else if (index < 0) {
        index = pStepMap->size() - 1;
      }
      m_pAccess->setParamValueDouble(PlugAccess::ElementDesc::VPOT, channel - 1,
                                     getNthKeyFromMap(index, pStepMap));
    }
  }
  return true;
}

void PlugMode::updateSoloLEDs() {
  for (int channel = 0; channel < 8; channel++) {
    if (m_pAccess->plugExist() && m_pAccess->getSelectedBank() == channel &&
        m_pAccess->isBankUsed(channel)) {
      m_pCCSManager->setSoloLED(this, channel + 1, LED_BLINK);
    } else if (m_pAccess->plugExist() && m_pAccess->isBankUsed(channel)) {
      m_pCCSManager->setSoloLED(this, channel + 1, LED_ON);
    } else {
      m_pCCSManager->setSoloLED(this, channel + 1, LED_OFF);
    }
  }
}

void PlugMode::updateMuteLEDs() {
  for (int channel = 0; channel < 8; channel++) {
    if (m_pAccess->plugExist() &&
        m_pAccess->getSelectedPageInSelectedBank() == channel &&
        m_pAccess->isPageUsedInSelectedBank(channel)) {
      m_pCCSManager->setMuteLED(this, channel + 1, LED_BLINK);
    } else if (m_pAccess->plugExist() &&
               m_pAccess->isPageUsedInSelectedBank(channel)) {
      m_pCCSManager->setMuteLED(this, channel + 1, LED_ON);
    } else {
      m_pCCSManager->setMuteLED(this, channel + 1, LED_OFF);
    }
  }
}

void PlugMode::updateFlipLED() {
  if (isModifierPressed(VK_ALT))
		m_pCCSManager->setFlipLED(this,
															m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DELTA) > 0
															? LED_BLINK
															: LED_OFF);
	else if (isModifierPressed(VK_SHIFT))
		m_pCCSManager->setFlipLED(this,
															m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DRYWET) > 0
															? LED_BLINK
															: LED_OFF);
	else
		m_pCCSManager->setFlipLED(this,
															m_pAccess->getParamValueInt(PlugAccess::ElementDesc::BYPASS) > 0
															? LED_OFF
															: LED_BLINK);
}

void PlugMode::updateAssignmentDisplay() {
  if (!m_pAccess->plugExist()) {
    m_pCCSManager->setAssignmentDisplay(this, "P ");
    return;
  }

  if (m_followTrack) {
    int iSlot = m_pAccess->getPlugSlot() + 1;
    if (iSlot < 10) {
      char pDisplay[2];
      pDisplay[0] = 'P';
      pDisplay[1] = '0' + iSlot;
      m_pCCSManager->setAssignmentDisplay(this, pDisplay);
    } else {
      m_pCCSManager->setAssignmentDisplay(this, "P_");
    }
  } else {
    m_pCCSManager->setAssignmentDisplay(this, "PL");
  }
}

void PlugMode::updateDisplay() {
  if (selectedTrack()) {
    if ((isSingleFaderTouched() || isSingleVPotTouched()) &&
        !m_buttonNameValuePressed &&
				m_pPlugMode2ndOptions->isOptionSetTo(PMO2_SHOW_DETAILS, PMO2A_ON)) {
      updateTouchedDisplay();
    } else {
      updateParamsDisplay();
    }
  }
}

void PlugMode::updateFaders() {
  if (isModifierPressed(VK_ALT))
    return; // ALT + REC_BUTTON is used for blind test, so also the faders
            // shouldn't give any hint about the preset

  for (unsigned int iFader = 0; iFader < 8; iFader++) {
    m_pCCSManager->setFader(
        this, iFader + 1,
        m_pAccess->getParamValueInt(PlugAccess::ElementDesc::FADER, iFader));
  }

  // set master fader to the drywet value
  m_pCCSManager->setFader(
      this, 0, m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DRYWET));
}

void PlugMode::updateVPOTs() {
  for (unsigned int iVPot = 0; iVPot < 8; iVPot++) {
    PMVPot::tSteps *pStepMap = m_pAccess->getParamSteps(iVPot);
    VPOT_LED *pVPot = m_pCCSManager->getVPOT(iVPot + 1);
    double val =
        m_pAccess->getParamValueDouble(PlugAccess::ElementDesc::VPOT, iVPot);

    pVPot->setBottom(pStepMap != NULL && pStepMap->size() > 0);
    if (!pStepMap) {
      pVPot->setMode(VPOT_LED::OFF);
    } else {
      int index = findIndexFromKeyInMap(val, pStepMap);
      if (index > -1) {
        if (pStepMap->size() == 1) {
          pVPot->setMode(VPOT_LED::FROM_LEFT);
          pVPot->setValue(11);
        } else if (pStepMap->size() == 2) {
          if (index == 0) {
            pVPot->setMode(VPOT_LED::FROM_MIDDLE_POINT);
            pVPot->setValue(1);
          }
          if (index == 1) {
            pVPot->setMode(VPOT_LED::FROM_MIDDLE_POINT);
            pVPot->setValue(11);
          }
        } else {
          pVPot->setMode(VPOT_LED::FROM_LEFT);
          if ((unsigned)index <
              pStepMap->size() /
                  2) { // the ceil/floor stuff give a more symetric result
            pVPot->setValue((int)ceil(index * 10. / (pStepMap->size() - 1)) +
                            1);
          } else {
            pVPot->setValue((int)floor(index * 10. / (pStepMap->size() - 1)) +
                            1);
          }
        }
      } else { // index == -1, if mode is not set, bottom led stay turned off
        pVPot->setMode(VPOT_LED::FROM_LEFT);
        pVPot->setValue(0);
      }
    }
  }
}

void PlugMode::trackListChange() { updateEverything(); }

void PlugMode::switchDisplay() {
  if (m_followTrack && selectedTrack() == NULL) {
    m_pCCSManager->switchToDisplay(this, m_pSingleTrackMessage);
  } else if (getNumPlugsInSelectedTrack() == 0 && m_followTrack) {
    m_pCCSManager->switchToDisplay(this, m_pNoPlugMessage);
  } else if (m_pAccess->getPlugSlot() == -1) {
    m_pCCSManager->switchToDisplay(this, m_pNoPlugSelectedMessage);
  } else if (m_pBankPagePlugSelector->getWhatToSelect() !=
             BankPagePlugSelector::NOTHING) {
    m_pCCSManager->switchToDisplay(
        this, m_pBankPagePlugSelector->getSelectorDisplay());
  } else if ((isSingleFaderTouched() || isSingleVPotTouched()) &&
             !m_buttonNameValuePressed &&
						 m_pPlugMode2ndOptions->isOptionSetTo(PMO2_SHOW_DETAILS, PMO2A_ON)) {
    m_pCCSManager->switchToDisplay(this, m_pTouchedDisplay);
  } else {
    m_pCCSManager->switchToDisplay(this, m_pParamsDisplay);
  }
}

void PlugMode::updateParamsDisplay() {
	if (m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_PROX)) {
		for (int iChannel = 0; iChannel < 8; iChannel++) {
			m_pParamsDisplay->changeField(1, iChannel + 1,
				m_pAccess->getParamValueShort(PlugAccess::ElementDesc::VPOT, iChannel)
																		.toCString());

      m_pParamsDisplay->changeField(0, iChannel + 1,
          m_pAccess->getParamNameShort(PlugAccess::ElementDesc::VPOT, iChannel)
              .toCString());

      m_pParamsDisplay->changeField(3, iChannel + 1,
          m_pAccess
              ->getParamValueShort(PlugAccess::ElementDesc::FADER, iChannel)
              .toCString());

      m_pParamsDisplay->changeField(2, iChannel + 1,
          m_pAccess->getParamNameShort(PlugAccess::ElementDesc::FADER, iChannel)
              .toCString());

			m_pParamsDisplay->changeField(2, 9, " Wet");

      int wet = m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DRYWET);
			char text[8];
			sprintf(text, " %3.0f%%", (100 * (double) wet / 16368.));
			m_pParamsDisplay->changeField(3, 9, text);

		}
		return;
	}
		
  for (int iChannel = 0; iChannel < 8; iChannel++) {
    if (m_pCCSManager->getVPotTouched(iChannel + 1) || m_buttonNameValuePressed)
      m_pParamsDisplay->changeField(
          0, iChannel + 1,
          m_pAccess->getParamValueShort(PlugAccess::ElementDesc::VPOT, iChannel)
              .toCString());
    else
      m_pParamsDisplay->changeField(
          0, iChannel + 1,
          m_pAccess->getParamNameShort(PlugAccess::ElementDesc::VPOT, iChannel)
              .toCString());

    if (m_pCCSManager->getFaderTouched(iChannel + 1) ||
        m_buttonNameValuePressed)
      m_pParamsDisplay->changeField(
          1, iChannel + 1,
          m_pAccess
              ->getParamValueShort(PlugAccess::ElementDesc::FADER, iChannel)
              .toCString());
    else
      m_pParamsDisplay->changeField(
          1, iChannel + 1,
          m_pAccess->getParamNameShort(PlugAccess::ElementDesc::FADER, iChannel)
              .toCString());
  }
}

void PlugMode::updateTouchedDisplay() {
  if (m_pAccess->plugExist()) {
		if (m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_PROX)) {
			updateTouchedDisplayProX();
			return;
		}
		
    PlugAccess::ElementDesc::eType element =
        (m_iSingleFaderTouched > 0) ? PlugAccess::ElementDesc::FADER
                                    : PlugAccess::ElementDesc::VPOT;
    int iChannel = (m_iSingleFaderTouched > 0) ? m_iSingleFaderTouched
                                               : m_iSingleVPotTouched;

		
    m_pTouchedDisplay->changeText(0, 0,
        m_pAccess->getBankNameLong(m_pAccess->getSelectedBank()).toCString(),
        17, true);
    m_pTouchedDisplay->changeText(0, 19,
        m_pAccess
            ->getPageNameLongInSelectedBank(
                m_pAccess->getSelectedPageInSelectedBank())
            .toCString(),
        17, true);
    m_pTouchedDisplay->changeText(0, 38,
				m_pAccess->getParamNameLong(element, iChannel - 1).toCString(),
        17, true);
    m_pTouchedDisplay->changeText(1, 0,
				m_pCCSManager->getMCU()->GetTrackName(m_pAccess->getPlugTrack()),
        17, true);
    m_pTouchedDisplay->changeText(1, 19,
			  m_pAccess->getPlugNameLong().toCString(), 17, true);
    m_pTouchedDisplay->changeText(1, 38,
				m_pAccess->getParamValueLong(element, iChannel - 1).toCString(),
        17, true);
  }
}

void PlugMode::updateTouchedDisplayProX() {
	//	m_pTouchedDisplay->clear();

	static int last_iSingleVPotTouched = -1;
	if (m_iSingleVPotTouched != last_iSingleVPotTouched) {
		last_iSingleVPotTouched = m_iSingleVPotTouched;
		m_pTouchedDisplay->clearLine(0);
		m_pTouchedDisplay->clearLine(1);
	}

	static int last_iSingleFaderTouched = -1;
	if (m_iSingleFaderTouched != last_iSingleFaderTouched) {
		last_iSingleFaderTouched = m_iSingleFaderTouched;
		m_pTouchedDisplay->clearLine(2);
		m_pTouchedDisplay->clearLine(3);
	}

	
	if (m_iSingleVPotTouched > 0) {
    m_pTouchedDisplay->changeText(0, 0,
        m_pAccess->getBankNameLong(m_pAccess->getSelectedBank()).toCString(),
        17, true);
    m_pTouchedDisplay->changeText(0, 19,
        m_pAccess->getPageNameLongInSelectedBank(
                     m_pAccess->getSelectedPageInSelectedBank()).toCString(),
        17, true);
    m_pTouchedDisplay->changeText(0, 38,
				m_pAccess->getParamNameLong(PlugAccess::ElementDesc::VPOT, m_iSingleVPotTouched - 1).toCString(),
        17, true);
    m_pTouchedDisplay->changeText(1, 0,
				m_pCCSManager->getMCU()->GetTrackName(m_pAccess->getPlugTrack()),
        17, true);
    m_pTouchedDisplay->changeText(1, 19,
			  m_pAccess->getPlugNameLong().toCString(), 17, true);
    m_pTouchedDisplay->changeText(1, 38,
				m_pAccess->getParamValueLong(PlugAccess::ElementDesc::VPOT, m_iSingleVPotTouched - 1).toCString(),
        17, true);
	} else {
		for (int iChannel = 0; iChannel < 8; iChannel++) {
      m_pTouchedDisplay->changeField(1, iChannel + 1,
          m_pAccess
              ->getParamValueShort(PlugAccess::ElementDesc::VPOT, iChannel)
              .toCString());

      m_pTouchedDisplay->changeField(0, iChannel + 1,
          m_pAccess->getParamNameShort(PlugAccess::ElementDesc::VPOT, iChannel)
              .toCString());
		}
	}

	if (m_iSingleFaderTouched > 0) {
    m_pTouchedDisplay->changeText(2, 0,
        m_pAccess->getBankNameLong(m_pAccess->getSelectedBank()).toCString(),
        17, true);
    m_pTouchedDisplay->changeText(2, 19,
        m_pAccess
            ->getPageNameLongInSelectedBank(
                m_pAccess->getSelectedPageInSelectedBank())
            .toCString(),
        17, true);
    m_pTouchedDisplay->changeText(2, 38,
				m_pAccess->getParamNameLong(PlugAccess::ElementDesc::FADER, m_iSingleFaderTouched - 1).toCString(),
        17, true);
    m_pTouchedDisplay->changeText(3, 0,
				m_pCCSManager->getMCU()->GetTrackName(m_pAccess->getPlugTrack()),
        17, true);
    m_pTouchedDisplay->changeText(3, 19,
			  m_pAccess->getPlugNameLong().toCString(), 17, true);
    m_pTouchedDisplay->changeText(3, 38,
				m_pAccess->getParamValueLong(PlugAccess::ElementDesc::FADER, m_iSingleFaderTouched - 1).toCString(),
        17, true);
	} else {
		for (int iChannel = 0; iChannel < 8; iChannel++) {
      m_pTouchedDisplay->changeField(3, iChannel + 1,
          m_pAccess
              ->getParamValueShort(PlugAccess::ElementDesc::FADER, iChannel)
              .toCString());

      m_pTouchedDisplay->changeField(2, iChannel + 1,
          m_pAccess->getParamNameShort(PlugAccess::ElementDesc::FADER, iChannel)
              .toCString());

			m_pParamsDisplay->changeField(2, 9, " Wet");

      int wet = m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DRYWET);
			char text[8];
			sprintf(text, " %3.0f%%", (100 * (double) wet / 16368.));
			m_pParamsDisplay->changeField(3, 9, text);
		}
	}
}


String PlugMode::getPlugNameShort(int iSlot) {
  if (selectedTrack()) {
    char paramName[80];
    bool valid = TrackFX_GetFXName(selectedTrack(), iSlot, paramName, 79);
    if (valid) {
      return shortPlugName(paramName);
    }
  }

  return String::empty;
}

int PlugMode::getNumPlugsInSelectedTrack() {
  if (selectedTrack()) {
    return TrackFX_GetCount(selectedTrack());
  }

  return 0;
}

String PlugMode::shortPlugName(const char *pName) {
  String name = pName;
  if (name.contains(JUCE_T(":")))
    return name.fromFirstOccurrenceOf(JUCE_T(":"), false, false)
        .upToFirstOccurrenceOf(JUCE_T("("), false, false)
        .substring(1, 7);
  else
    return name.substring(0, 6);
}

String PlugMode::longPlugName(const char *pName) {
  String name = pName;
  if (name.contains(JUCE_T(":")))
    return name.fromFirstOccurrenceOf(JUCE_T(":"), false, false)
        .substring(1, 18);
  else
    return name.substring(0, 17);
}

void PlugMode::updateEverything() {
  switchDisplay();
  m_pBankPagePlugSelector->updateDisplay();
  CCSMode::updateEverything();
}

bool PlugMode::buttonGView(bool pressed) {
  if (!pressed)
    return true;

  m_followTrack = !m_followTrack;
  if (m_followTrack) {
    m_pAccess->trackChanged(selectedTrack());
  }

  m_pBankPagePlugSelector->clearDisplay();
  updateEverything();

  return true;
}

bool PlugMode::buttonNameValue(bool pressed) {
  m_buttonNameValuePressed = pressed;
  switchDisplay();
  return true;
}

void PlugMode::trackSelected(MediaTrack *trackid, bool selected) {
  if (selected && m_followTrack)
    m_pAccess->trackChanged(trackid);

  updateEverything();
}

bool PlugMode::buttonFlip(bool pressed) {
  if (!pressed)
    return true;

  PlugAccess::ElementDesc::eType type;

  if (isModifierPressed(VK_SHIFT)) {
    type = PlugAccess::ElementDesc::DRYWET;
  }
  else if (isModifierPressed(VK_ALT)) {
    type = PlugAccess::ElementDesc::DELTA;
  }
  else {
    type = PlugAccess::ElementDesc::BYPASS;
  }

  int actualValue = m_pAccess->getParamValueInt(type);
  m_pAccess->setParamValueInt(type, 0, actualValue ? 0 : (int)MAX_FADER_VALUE);

  updateFlipLED();

  return true;
}

Component **PlugMode::createEditorComponent() {
  if (!m_pPlugEditor)
    m_pPlugEditor = new PlugModeComponent(m_pAccess);

  m_pPlugEditor->updateEverything();
  return reinterpret_cast<Component **>(&m_pPlugEditor);
}

void PlugMode::deleteEditorComponent() { removeEditor(); }

void PlugMode::removeEditor() {
  m_pCCSManager->closeEditorIfOpen(m_pPlugEditor);

  safe_delete(m_pPlugEditor);
}

void PlugMode::frameUpdate() {
  m_pMeterBridge->updateMeterBridge(m_pCCSManager->getMCU());
	
  updateEverything();
  if (m_pCCSManager->getNumSelectButtonsPressed() == 0 &&
      m_lastTimePlugWasSelected + TIMETOSWITCHPLUGINMS <
          m_pCCSManager->getLastTime()) {
    m_pAccess->checkChainChanges();
  }

  m_pAccess->checkFloatWindows();

  m_pAccess->getPlugWindowManager()->moveWnd();

	if (m_pPlugMode2ndOptions->isOptionSetTo(PMO2_FOLLOW_CHANGE, PMO2A_ON))
		followChanges();
}

void PlugMode::updateRecLEDs() {
  String fxGUID = GUID2String(
      TrackFX_GetFXGUID(m_pAccess->getPlugTrack(), m_pAccess->getPlugSlot()));

  int start = isModifierPressed(VK_SHIFT) ? 8 : 0;
  for (int channel = 0; channel < 8; channel++) {
    if (channel + start == m_lastCalledPreset[fxGUID]) {
      if (m_pPresetManager->presetMatchState(m_pAccess->getPlugTrack(),
                                             m_pAccess->getPlugSlot(),
                                             channel + start)) {
        m_pCCSManager->setRecLED(
            this, channel + 1, isModifierPressed(VK_ALT) ? LED_ON : LED_BLINK);
        continue;
      } else {
        m_lastCalledPreset[fxGUID] = -1;
      }
    }
    String fxGUID = GUID2String(
        TrackFX_GetFXGUID(m_pAccess->getPlugTrack(), m_pAccess->getPlugSlot()));
    m_pCCSManager->setRecLED(
        this, channel + 1,
        m_pPresetManager->hasPreset(fxGUID, channel + start) ? LED_ON
                                                             : LED_OFF);
  }
}

bool PlugMode::isSlotBypassed(MediaTrack *pPlugTrack, int iSlot) {
  double min, max;
  int bypassID = TrackFX_GetNumParams(pPlugTrack, iSlot) - 2;
	return TrackFX_GetParam(pPlugTrack, iSlot, bypassID, &min, &max) > 0;
}

void PlugMode::updateSelectLEDs() {
  int start = isModifierPressed(VK_SHIFT) ? 8 : 0;
  if (m_followTrack) {
    for (int channel = 0; channel < 8; channel++) {
      if (channel + start == m_pAccess->getPlugSlot())
        m_pCCSManager->setSelectLED(this, channel + 1, LED_BLINK);
      else if (channel + start < getNumPlugsInSelectedTrack()) {
				if (isSlotBypassed(m_pAccess->getPlugTrack(), channel + start))
					m_pCCSManager->setSelectLED(this, channel + 1, LED_BLINK_BYPASSED);
				else
					m_pCCSManager->setSelectLED(this, channel + 1, LED_ON);
			}
      else
        m_pCCSManager->setSelectLED(this, channel + 1, LED_OFF);
    }
  } else {
    for (int channel = 0; channel < 8; channel++) {
      MediaTrack *pMT = NULL;
      if (m_favPlugins[start + channel].get<0>() != GUID_NOT_ACTIVE) {
        pMT = CSurf_MCU::TrackFromGUID(m_favPlugins[start + channel].get<0>());
        int slot = m_favPlugins[start + channel].get<1>();
        if (slot == m_pAccess->getPlugSlot() &&
            pMT == m_pAccess->getPlugTrack() && pMT != NULL)
          m_pCCSManager->setSelectLED(this, channel + 1, LED_BLINK);
        else if (pMT != NULL) {
					if (isSlotBypassed(pMT, channel + start))
						m_pCCSManager->setSelectLED(this, channel + 1, LED_BLINK_BYPASSED);
					else
						m_pCCSManager->setSelectLED(this, channel + 1, LED_ON);
					
				}
      } else
        m_pCCSManager->setSelectLED(this, channel + 1, LED_OFF);
    }
  }
}

bool PlugMode::buttonSelect(int channel, bool pressed) {
  if (pressed) {
    m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::PLUG);
  } else {
    m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::NOTHING);
  }

  if (!pressed)
    return true;

  int slot = channel + (isModifierPressed(VK_SHIFT) ? 8 : 0) - 1;
  if (m_followTrack) {
    if (slot < getNumPlugsInSelectedTrack()) {
      m_lastTimePlugWasSelected = m_pCCSManager->getLastTime();
      m_pAccess->accessPlugin(selectedTrack(), slot);
    }
  } else {
    if (isModifierPressed(VK_CONTROL)) {
      m_favPlugins[slot] =
          tFav(m_pAccess->getPlugTrackGUID(), m_pAccess->getPlugSlot(),
               m_pCCSManager->getMCU()->GetActualFrameTime());
    } else if (isModifierPressed(VK_OPTION)) {
      m_favPlugins[slot] = tFav(GUID_NOT_ACTIVE, -1, 0);
    } else {
      MediaTrack *pMT = CSurf_MCU::TrackFromGUID(m_favPlugins[slot].get<0>());
      if (pMT) {
        m_lastTimePlugWasSelected = m_pCCSManager->getLastTime();
        m_pAccess->accessPlugin(pMT, m_favPlugins[slot].get<1>());
      }
    }
  }

  return true;
}

bool PlugMode::buttonFaderBanks(int button, bool pressed) {
  int bank = m_pAccess->getSelectedBank();
  int page = m_pAccess->getSelectedPageInSelectedBank();

  switch (button) {
  case B_BANK_UP:
  case B_BANK_DOWN:
    if (pressed) {
      m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::BANK);
    } else if (m_pCCSManager->getNumSoloButtonsPressed() == 0) {
      m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::NOTHING);
    }
    break;
  case B_CHANNEL_UP:
  case B_CHANNEL_DOWN:
    if (pressed) {
      m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::PAGE);
    } else if (m_pCCSManager->getNumMuteButtonsPressed() == 0) {
      m_pBankPagePlugSelector->activateSelector(BankPagePlugSelector::NOTHING);
    }
    break;
  }

  if (!pressed)
    return true;

  switch (button) {
  case B_BANK_UP:
    while (bank < 7) {
      bank++;
      if (m_pAccess->isBankUsed(bank)) {
        buttonSolo(bank + 1, pressed);
        m_pAccess->setSelectedBank(bank);
        break;
      }
    }
    break;
  case B_BANK_DOWN:
    while (bank > 0) {
      bank--;
      if (m_pAccess->isBankUsed(bank)) {
        buttonSolo(bank + 1, pressed);
        break;
      }
    }
    break;
  case B_CHANNEL_UP:
    while (page < 7) {
      page++;
      if (m_pAccess->isPageUsedInSelectedBank(page)) {
        buttonMute(page + 1, pressed);
        break;
      }
    }
    break;
  case B_CHANNEL_DOWN:
    while (page > 0) {
      page--;
      if (m_pAccess->isPageUsedInSelectedBank(page)) {
        buttonMute(page + 1, pressed);
        break;
      }
    }
    break;
  }

  switchDisplay();

  return true;
}

PlugMode::tFav PlugMode::getFavorite(unsigned i) {
  ASSERT(i < NUM_FAVORITES);
  return m_favPlugins[i];
}

#define PLUGMODE_NODE_ROOT JUCE_T("PLUGMODE")
#define PLUGMODE_NODE_LAST_CALLED_PRESETS JUCE_T("LAST_CALLED_PRESETS")
#define PLUGMODE_NODE_FAV JUCE_T("FAVORITE")
#define PLUGMODE_NODE_LAST_CALLED_PRESET JUCE_T("PRESET")
#define PLUGMODE_ATT_FAV_INDEX JUCE_T("index")
#define PLUGMODE_ATT_FAV_TRACK JUCE_T("track")
#define PLUGMODE_ATT_FAV_SLOT JUCE_T("slot")
#define PLUGMODE_ATT_LAST_CALLED_PRESET_FXGUID JUCE_T("fxguid")
#define PLUGMODE_ATT_LAST_CALLED_PRESET_PRESET JUCE_T("preset")

void PlugMode::projectChanged(XmlElement *pXmlElement,
                              ProjectConfig::EAction action) {
  XmlElement *pPlugModeNode;
  XmlElement *pPresetsNode;

  switch (action) {
  case ProjectConfig::WRITE:
    pPlugModeNode = new XmlElement(PLUGMODE_NODE_ROOT);
    pXmlElement->addChildElement(pPlugModeNode);
    writeFavsToProjectConfig(pPlugModeNode);
    pPresetsNode = new XmlElement(PLUGMODE_NODE_LAST_CALLED_PRESETS);
    pXmlElement->addChildElement(pPresetsNode);
    writeLastCalledPresetsToProjectConfig(pPresetsNode);
    break;
  case ProjectConfig::FREE:
    for (int slot = 0; slot < NUM_FAVORITES; slot++)
      m_favPlugins[slot] = tFav(GUID_NOT_ACTIVE, -1, 0);
    break;
  case ProjectConfig::READ:
    pPlugModeNode = pXmlElement->getChildByName(PLUGMODE_NODE_ROOT);
    if (pPlugModeNode) {
      readFavsFromProjectConfig(pPlugModeNode);
    }
    pPresetsNode =
        pXmlElement->getChildByName(PLUGMODE_NODE_LAST_CALLED_PRESETS);
    if (pPresetsNode) {
      readLastCalledPresetsFromProjectConfig(pPresetsNode);
    }
    break;
  }
}

void PlugMode::writeLastCalledPresetsToProjectConfig(XmlElement *pNode) {
  BOOST_FOREACH (tLCPs::value_type &tLCP, m_lastCalledPreset) {
    if (tLCP.second >= 0) { // -1 is used for never changed preset
      XmlElement *pPresetNode =
          new XmlElement(PLUGMODE_NODE_LAST_CALLED_PRESET);
      pPresetNode->setAttribute(PLUGMODE_ATT_LAST_CALLED_PRESET_FXGUID,
                                tLCP.first);
      pPresetNode->setAttribute(PLUGMODE_ATT_LAST_CALLED_PRESET_PRESET,
                                tLCP.second);
      pNode->addChildElement(pPresetNode);
    }
  }
}

void PlugMode::readLastCalledPresetsFromProjectConfig(XmlElement *pNode) {
  forEachXmlChildElement(*pNode, pChild) {
    if (pChild->getTagName() == PLUGMODE_NODE_LAST_CALLED_PRESET) {
      String guidString =
          pChild->getStringAttribute(PLUGMODE_ATT_LAST_CALLED_PRESET_FXGUID);
      int preset =
          pChild->getIntAttribute(PLUGMODE_ATT_LAST_CALLED_PRESET_PRESET);
      m_lastCalledPreset[guidString] = preset;
    }
  }
}

void PlugMode::writeFavsToProjectConfig(XmlElement *pNode) {
  for (int i = 0; i < NUM_FAVORITES; i++) {
    if (m_favPlugins[i].get<0>() != GUID_NOT_ACTIVE) {
      XmlElement *pFavNode = new XmlElement(PLUGMODE_NODE_FAV);
      pFavNode->setAttribute(PLUGMODE_ATT_FAV_INDEX, i);

      String guidAsString;
      GUID2String(&m_favPlugins[i].get<0>(), guidAsString);
      pFavNode->setAttribute(PLUGMODE_ATT_FAV_TRACK, guidAsString);

      pFavNode->setAttribute(PLUGMODE_ATT_FAV_SLOT, m_favPlugins[i].get<1>());
      pNode->addChildElement(pFavNode);
    }
  }
}

void PlugMode::readFavsFromProjectConfig(XmlElement *pNode) {
  forEachXmlChildElement(*pNode, pChild) {
    if (pChild->getTagName() == PLUGMODE_NODE_FAV) {
      int index = pChild->getIntAttribute(PLUGMODE_ATT_FAV_INDEX);

      String guidString = pChild->getStringAttribute(PLUGMODE_ATT_FAV_TRACK);
      GUID guid;
      String2GUID(guidString, &guid);

      int slot = pChild->getIntAttribute(PLUGMODE_ATT_FAV_SLOT);
      m_favPlugins[index] =
          tFav(guid, slot, m_pCCSManager->getMCU()->GetActualFrameTime());
    }
  }
}

void PlugMode::plugMoved(MediaTrack *pOldTrack, int oldSlot,
                         MediaTrack *pNewTrack, int newSlot) {
  GUID oldGUID = *GetTrackGUID(pOldTrack);
  GUID newGUID = pNewTrack != NULL ? *GetTrackGUID(pNewTrack) : GUID_NOT_ACTIVE;

  for (int i = 0; i < NUM_FAVORITES; i++) {
    if (m_favPlugins[i].get<0>() == oldGUID &&
        m_favPlugins[i].get<1>() == oldSlot &&
        m_favPlugins[i].get<2>() !=
            m_pCCSManager->getMCU()->GetActualFrameTime()) {
      m_favPlugins[i] =
          tFav(newGUID, newSlot, m_pCCSManager->getMCU()->GetActualFrameTime());
      break;
    }
  }
}

void PlugMode::followChanges() {
	static int onlyEveryTenth = 0;
	onlyEveryTenth++;
	if (onlyEveryTenth % 10 != 0)
		return;
	
  int numChangedValues = 0;
	int changeInBank = -1;
	int changeInPage = -1;

	for (int bank = 0; bank < 8; bank++) {
		for (int page = 0; page < 8; page ++) {
			for (int channel = 0; channel < 8; channel++) {
				double faderVal = m_pAccess->getParamValueDouble(
					   &PlugAccess::ElementDesc(bank, page, PlugAccess::ElementDesc::FADER, channel));
				double vpotVal = m_pAccess->getParamValueDouble(
					   &PlugAccess::ElementDesc(bank, page, PlugAccess::ElementDesc::VPOT, channel));
				if (faderVal != lastFaderValues[bank][page][channel] ||
						vpotVal != lastVPotValues[bank][page][channel]) {
					numChangedValues++;
					changeInBank = bank;
					changeInPage = page;
					lastFaderValues[bank][page][channel] = faderVal;
					lastVPotValues[bank][page][channel] = vpotVal;
				}
			}
		}
	}

	if (numChangedValues > 0 && numChangedValues < 3) {
		m_pAccess->setSelectedBank(changeInBank);
		m_pAccess->setSelectedPage(changeInBank, changeInPage);
	}
}

// MediaTrack* PlugMode::selectedTrack()
// {
//  MediaTrack* pMT = Tracks::instance()->getSelectedSingleTrack();
//  if (pMT == NULL) {
//    return GetMasterTrack(NULL);
//  }
//
//  return pMT;
// }
