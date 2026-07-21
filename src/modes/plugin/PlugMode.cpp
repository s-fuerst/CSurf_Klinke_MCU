/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include "PlugMode.h"
#include "PlugModeComponent.h"
#include "PlugAccess.h"
#include "PlugModeMeterBridge.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include "Display.h"
#include "MultiDisplay.h"
#include "std_helper.h"
#include <math.h>
#include "PlugModeOptions.h"
#include "PlugMode2ndOptions.h"
#include "PlugWindowManager.h"
#include "PlugMoveWatcher.h"
#include "PlugPresetManager.h"
#include "Tracks.h"

#define NUM_FAVORITES 16
#define NUM_PRESETS 16
#define TIMETOSWITCHPLUGINMS 2000

PlugMode::PlugMode(CCSManager *pManager)
    : CCSMode(pManager), m_iSingleFaderTouched(0), m_iSingleVPotTouched(0),
      m_pAccess(NULL), m_pPlugEditor(NULL), m_buttonNameValuePressed(false),
      m_followTrack(true), m_lastTimePlugWasSelected(0), m_activeUnit(0),
      m_paramCacheValid(false) {
  lastFaderValues.assign(8 * 8 * 8, 0.0);
  lastVPotValues.assign(8 * 8 * 8, 0.0);
  m_pAccess = new PlugAccess(this);

  m_pPlugModeOptions = new PlugModeOptions(pManager->getDisplayHandler());
  m_pPlugMode2ndOptions = new PlugMode2ndOptions(pManager->getDisplayHandler());

  m_pParamsDisplay = pManager->createDisplay(4);
  m_pTouchedDisplay = pManager->createDisplay(4);
  m_pValueDisplay = pManager->createDisplay(2);

	m_pMeterBridge = new PlugModeMeterBridge(this);

  m_pSingleTrackMessage = pManager->createDisplay(2);
  m_pSingleTrackMessage->changeTextFullLine(
      0, "You must select a single track.", true);
  m_pSingleTrackMessage->clearLine(1);

  m_pNoPlugMessage = pManager->createDisplay(2);
  m_pNoPlugMessage->clearLine(0);
  m_pNoPlugMessage->changeTextFullLine(1, "No FX exist in selected track.",
                                       true);

  m_pNoPlugSelectedMessage = pManager->createDisplay(2);
  m_pNoPlugSelectedMessage->clearLine(0);
  m_pNoPlugSelectedMessage->changeTextFullLine(1, "You must select a FX.",
                                               true);

  m_pPlugSelector = new PlugSelector(pManager->getDisplayHandler(), this);
  // WP-PlugMode Phase 3: per-unit BankPagePlugSelector instances
  int nUnits = pManager->getMCU()->numUnits();
  for (int u = 0; u < MAX_SURFACE_UNITS; u++) {
    if (u < nUnits) {
      DisplayHandler *unitDH =
          pManager->getMCU()->unitForChannel(u * 8 + 1)->displayHandler();
      m_pBankPagePlugSelectorPerUnit[u] =
          new BankPagePlugSelector(unitDH, this, u);
    } else {
      m_pBankPagePlugSelectorPerUnit[u] = NULL;
    }
  }

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
  for (int u = 0; u < MAX_SURFACE_UNITS; u++)
    safe_delete(m_pBankPagePlugSelectorPerUnit[u]);
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

  // Sync known chain/window states so the next frame update doesn't detect
  // false changes from deactivate/activate cycles and spuriously switch
  // the selected track.
  m_pAccess->syncKnownStates();
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
    if ((presetNr = m_lastCalledPreset[fxGUID]))
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
  // WP-PlugMode Phase 2: per-unit (R8)
  int unit = (channel - 1) / 8;
  int localCh = (channel - 1) % 8;
  setActiveUnit(unit);

  if (pressed) {
    safe_call(m_pPlugEditor, selectedBankChanged(localCh))

        m_pBankPagePlugSelectorPerUnit[unit]->select(localCh);
  }

  if (pressed && m_pAccess->isBankUsed(localCh)) {
    // WP-PlugMode Phase 4b: Control+cascade (R3). When Control is held, bank
    // selection cascades to units `unit..N-1`, spreading their pages along
    // the used-page sequence from offset 0. Units 0..unit-1 unchanged.
    // Without Control the bank is set for this unit only (legacy behaviour).
    if (isModifierPressed(VK_CONTROL)) {
      cascadeFromUnit(unit, localCh, 0);
    } else {
      m_pAccess->setSelectedBank(localCh, unit);
    }
  }

  if (pressed) {
    m_pBankPagePlugSelectorPerUnit[unit]->activateSelector(BankPagePlugSelector::BANK);
  } else if (m_pCCSManager->getNumSoloButtonsPressed() == 0) {
    m_pBankPagePlugSelectorPerUnit[unit]->activateSelector(BankPagePlugSelector::NOTHING);
  }

  return true;
}

bool PlugMode::buttonMute(int channel, bool pressed) {
  // WP-PlugMode Phase 2: per-unit (R8)
  int unit = (channel - 1) / 8;
  int localCh = (channel - 1) % 8;
  setActiveUnit(unit);

  if (pressed)
    safe_call(m_pPlugEditor, selectedPageChanged(localCh))

        if (pressed && isModifierPressed(VK_CONTROL) &&
            m_pAccess->isPageUsedInSelectedBank(localCh)) {
      // WP-PlugMode Phase 4c: Control+Mute cascades pages — unit N gets
      // page localCh, unit N+1 the next used page, N+2 the one after, etc.
      m_pAccess->setSelectedPageInSelectedBank(localCh, unit);
      int bank = m_pAccess->selectedBankForUnit(unit);
      int resolved = m_pAccess->resolveBankReference(bank);
      int baseOffset = m_pAccess->pageUsedOffsetForPage(resolved, localCh);
      if (baseOffset >= 0)
        cascadeFromUnit(unit, bank, baseOffset);
    } else if (pressed && isModifierPressed(VK_SHIFT)) {
      for (int iBank = 0; iBank < 8; iBank++) {
        if (m_pAccess->isPageUsed(iBank, localCh)) {
          m_pAccess->setSelectedPage(iBank, localCh, unit);
        }
      }
    }
  else if (pressed && m_pAccess->isPageUsedInSelectedBank(localCh)) {
    m_pAccess->setSelectedPageInSelectedBank(localCh, unit);
  }

  if (pressed) {
    m_pBankPagePlugSelectorPerUnit[unit]->activateSelector(BankPagePlugSelector::PAGE);
  } else if (m_pCCSManager->getNumMuteButtonsPressed() == 0) {
    m_pBankPagePlugSelectorPerUnit[unit]->activateSelector(BankPagePlugSelector::NOTHING);
  }

  return true;
}

bool PlugMode::fader(int channel, int value) {
  // WP-PlugMode Phase 2: per-unit fader resolution
  int unit = (channel > 0) ? (channel - 1) / 8 : 0;
  int localCh = (channel > 0) ? (channel - 1) % 8 : 0;
  setActiveUnit(unit);

  if (channel > 0)
    safe_call(m_pPlugEditor, selectedChannelChanged(localCh, true))

        if (m_followTrack && !selectedTrack()) {
      m_pCCSManager->setFader(this, channel, 0);
    }

  if (channel == 0 /* Master fader */) {
    m_pAccess->setParamValueInt(PlugAccess::ElementDesc::DRYWET, 0, value);
  } else {
    int bank = m_pAccess->selectedBankForUnit(unit);
    int page = m_pAccess->selectedPageForUnit(unit);
    m_pAccess->setParamValueInt(bank, page, PlugAccess::ElementDesc::FADER,
                                localCh, value);
  }
  m_pCCSManager->setFader(this, channel, value);

  return true;
}

bool PlugMode::singleFaderTouched(int channel) {
  // WP-PlugMode: editor callback uses local channel (0-7)
  int localCh = (channel > 0) ? (channel - 1) % 8 : 0;
  if (channel > 0)
    safe_call(m_pPlugEditor, selectedChannelChanged(localCh, true))
        m_iSingleFaderTouched = channel;
  switchDisplay();
  return true;
}

bool PlugMode::singleVPotTouched(int channel) {
  // WP-PlugMode: editor callback uses local channel (0-7)
  int localCh = (channel > 0) ? (channel - 1) % 8 : 0;
  if (channel > 0)
    safe_call(m_pPlugEditor, selectedChannelChanged(localCh, false))

        m_iSingleVPotTouched = channel;
  switchDisplay();
  return true;
}

bool PlugMode::vpotMoved(int channel, int numSteps) {
  // WP-PlugMode Phase 2: per-unit VPOT resolution
  int unit = (channel - 1) / 8;
  int localCh = (channel - 1) % 8;
  setActiveUnit(unit);
  int bank = m_pAccess->selectedBankForUnit(unit);
  int page = m_pAccess->selectedPageForUnit(unit);

  PMVPot::tSteps *pStepMap = m_pAccess->getParamSteps(bank, page, localCh);
  double val = m_pAccess->getParamValueDouble(bank, page,
      PlugAccess::ElementDesc::VPOT, localCh);

  if (pStepMap && !pStepMap->empty()) {
    int index = findIndexFromKeyInMap(val, pStepMap);
    (numSteps < 0) ? index-- : index++;
    if (index >= (signed)pStepMap->size()) {
      index = pStepMap->size() - 1;
    } else if (index < 0) {
      index = 0;
    }
    m_pAccess->setParamValueDouble(bank, page, PlugAccess::ElementDesc::VPOT,
                                   localCh,
                                   getNthKeyFromMap(index, pStepMap));
  }
  return true;
}

bool PlugMode::vpotPressed(int channel, bool pressed) {
  // WP-PlugMode Phase 3: per-unit vpotPressed dispatch (R4)
  int unit = (channel - 1) / 8;
  int localCh = (channel - 1) % 8;
  setActiveUnit(unit);

  if (pressed) {
    switch (m_pBankPagePlugSelectorPerUnit[m_activeUnit]->getWhatToSelect()) {
    case BankPagePlugSelector::NOTHING:
      break;
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
    int bank = m_pAccess->selectedBankForUnit(unit);
    int page = m_pAccess->selectedPageForUnit(unit);
    PMVPot::tSteps *pStepMap = m_pAccess->getParamSteps(bank, page, localCh);
    double val = m_pAccess->getParamValueDouble(bank, page,
        PlugAccess::ElementDesc::VPOT, localCh);

    if (pStepMap && !pStepMap->empty()) {
      int index = findIndexFromKeyInMap(val, pStepMap);
      isModifierPressed(VK_SHIFT) ? index-- : index++;
      if (index >= (signed)pStepMap->size()) {
        index = 0;
      } else if (index < 0) {
        index = pStepMap->size() - 1;
      }
      m_pAccess->setParamValueDouble(bank, page, PlugAccess::ElementDesc::VPOT,
                                     localCh,
                                     getNthKeyFromMap(index, pStepMap));
    }
  }
  return true;
}

void PlugMode::updateSoloLEDs() {
  // WP-PlugMode Phase 2: per-unit LED loop
  int nStrips = m_pCCSManager->getMCU()->numUnits() * 8;
  for (int i = 0; i < nStrips; i++) {
    int unit = i / 8;
    int localCh = i % 8;
    int globalCh = i + 1;
    setActiveUnit(unit);
    if (m_pAccess->plugExist() && m_pAccess->selectedBankForUnit(unit) == localCh &&
        m_pAccess->isBankUsed(localCh)) {
      m_pCCSManager->setSoloLED(this, globalCh, LED_BLINK);
    } else if (m_pAccess->plugExist() && m_pAccess->isBankUsed(localCh)) {
      m_pCCSManager->setSoloLED(this, globalCh, LED_ON);
    } else {
      m_pCCSManager->setSoloLED(this, globalCh, LED_OFF);
    }
  }
}

void PlugMode::updateMuteLEDs() {
  // WP-PlugMode Phase 2: per-unit LED loop
  int nStrips = m_pCCSManager->getMCU()->numUnits() * 8;
  for (int i = 0; i < nStrips; i++) {
    int unit = i / 8;
    int localCh = i % 8;
    int globalCh = i + 1;
    setActiveUnit(unit);
    if (m_pAccess->plugExist() &&
        m_pAccess->selectedPageForUnit(unit) == localCh &&
        m_pAccess->isPageUsedInSelectedBank(localCh)) {
      m_pCCSManager->setMuteLED(this, globalCh, LED_BLINK);
    } else if (m_pAccess->plugExist() &&
               m_pAccess->isPageUsedInSelectedBank(localCh)) {
      m_pCCSManager->setMuteLED(this, globalCh, LED_ON);
    } else {
      m_pCCSManager->setMuteLED(this, globalCh, LED_OFF);
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

  // WP-PlugMode Phase 2: per-unit fader update
  int nStrips = m_pCCSManager->getMCU()->numUnits() * 8;
  for (int i = 0; i < nStrips; i++) {
    int unit = i / 8;
    int localCh = i % 8;
    int globalCh = i + 1;
    setActiveUnit(unit);
    int bank = m_pAccess->selectedBankForUnit(unit);
    int page = m_pAccess->selectedPageForUnit(unit);
    m_pCCSManager->setFader(
        this, globalCh,
        m_pAccess->getParamValueInt(bank, page, PlugAccess::ElementDesc::FADER,
                                    localCh));
  }

  // set master fader to the drywet value (anchor unit)
  m_pCCSManager->setFader(
      this, 0, m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DRYWET));
}

void PlugMode::updateVPOTs() {
  // WP-PlugMode Phase 2: per-unit VPOT update
  int nStrips = m_pCCSManager->getMCU()->numUnits() * 8;
  for (int i = 0; i < nStrips; i++) {
    int unit = i / 8;
    int localCh = i % 8;
    int globalCh = i + 1;
    setActiveUnit(unit);
    int bank = m_pAccess->selectedBankForUnit(unit);
    int page = m_pAccess->selectedPageForUnit(unit);

    PMVPot::tSteps *pStepMap = m_pAccess->getParamSteps(bank, page, localCh);
    VPOT_LED *pVPot = m_pCCSManager->getVPOT(globalCh);
    double val = m_pAccess->getParamValueDouble(bank, page,
        PlugAccess::ElementDesc::VPOT, localCh);

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
  // WP-PlugMode Phase 1/6: per-unit switchDisplay (R5/R6).
  //
  // Each unit's handler is switched EXACTLY ONCE to its final target per
  // frame: a unit whose bank/page/plug selector is active (Solo=BANK,
  // Mute=PAGE, Select=PLUG held) shows its selector display, every other
  // unit shows the shared content (params / touched / message).
  // switchTo()'s same-display early-return then makes this a no-op when
  // nothing changed — which is what stops the flicker (switchToAll followed
  // by a per-unit override would activate the shared child AND the selector
  // every frame, fighting each other).
  //
  // Guarded by canSwitchDisplay() (selector-overlay / option / NameValue).
  if (!m_pCCSManager->canSwitchDisplay(this))
    return;

  Display *pShared;
  bool globalMessage = false;
  if (m_followTrack && selectedTrack() == NULL) {
    pShared = m_pSingleTrackMessage;
    globalMessage = true;
  } else if (getNumPlugsInSelectedTrack() == 0 && m_followTrack) {
    pShared = m_pNoPlugMessage;
    globalMessage = true;
  } else if (m_pAccess->getPlugSlot() == -1) {
    pShared = m_pNoPlugSelectedMessage;
    globalMessage = true;
  } else if ((isSingleFaderTouched() || isSingleVPotTouched()) &&
             !m_buttonNameValuePressed &&
             m_pPlugMode2ndOptions->isOptionSetTo(PMO2_SHOW_DETAILS, PMO2A_ON)) {
    pShared = m_pTouchedDisplay;
  } else {
    pShared = m_pParamsDisplay;
  }

  MultiDisplay *md = dynamic_cast<MultiDisplay *>(pShared);
  // Global messages clear the non-anchor children so only the anchor shows
  // the message; other units go blank.
  if (globalMessage && md)
    clearNonAnchorChildren(pShared);

  // Switch each unit's handler once to its final target.
  int nUnits = m_pCCSManager->getMCU()->numUnits();
  for (int u = 0; u < nUnits; u++) {
    BankPagePlugSelector *sel = m_pBankPagePlugSelectorPerUnit[u];
    if (!sel)
      continue;
    Display *target;
    if (sel->getWhatToSelect() != BankPagePlugSelector::NOTHING) {
      target = sel->getSelectorDisplay();
    } else if (md && u < (int)md->children().size()) {
      target = md->children()[u];
    } else {
      target = pShared; // N=1: plain Display on the single handler
    }
    sel->getDisplayHandler()->switchTo(target);
  }
}

void PlugMode::updateParamsDisplay() {
  // WP-PlugMode Phase 1/6: per-unit display loop (R5). Phase 6 (R7) selects
  // the 4-row ProX vs 2-row MCU layout per owning unit's isProX() instead of
  // the global CONFIG_FLAG_PROX flag, so mixed main/extender configs render
  // each unit in its native layout.
  int nUnits = m_pCCSManager->getMCU()->numUnits();
  int nStrips = nUnits * 8;

  for (int iChannel = 0; iChannel < nStrips; iChannel++) {
    int unit = iChannel / 8;
    int localCh = iChannel % 8;
    setActiveUnit(unit);
    int bank = m_pAccess->selectedBankForUnit(unit);
    int page = m_pAccess->selectedPageForUnit(unit);
    HardwareUnit *hu = m_pCCSManager->getMCU()->unitForChannel(iChannel + 1);
    bool prox = hu && hu->isProX();

    if (prox) {
      // ProX 4-row layout: VPOT name/value (rows 0/1), FADER name/value
      // (rows 2/3).
      m_pParamsDisplay->changeField(1, iChannel + 1,
          m_pAccess
              ->getParamValueShort(bank, page, PlugAccess::ElementDesc::VPOT,
                                   localCh)
              .toRawUTF8());
      m_pParamsDisplay->changeField(0, iChannel + 1,
          m_pAccess
              ->getParamNameShort(bank, page, PlugAccess::ElementDesc::VPOT,
                                  localCh)
              .toRawUTF8());
      m_pParamsDisplay->changeField(3, iChannel + 1,
          m_pAccess
              ->getParamValueShort(bank, page, PlugAccess::ElementDesc::FADER,
                                   localCh)
              .toRawUTF8());
      m_pParamsDisplay->changeField(2, iChannel + 1,
          m_pAccess
              ->getParamNameShort(bank, page, PlugAccess::ElementDesc::FADER,
                                  localCh)
              .toRawUTF8());
    } else {
      // MCU 2-row layout: name/value switches with touch / name-value button.
      if (m_pCCSManager->getVPotTouched(iChannel + 1) ||
          m_buttonNameValuePressed)
        m_pParamsDisplay->changeField(
            0, iChannel + 1,
            m_pAccess
                ->getParamValueShort(bank, page, PlugAccess::ElementDesc::VPOT,
                                     localCh)
                .toRawUTF8());
      else
        m_pParamsDisplay->changeField(
            0, iChannel + 1,
            m_pAccess
                ->getParamNameShort(bank, page, PlugAccess::ElementDesc::VPOT,
                                localCh)
                .toRawUTF8());

      if (m_pCCSManager->getFaderTouched(iChannel + 1) ||
          m_buttonNameValuePressed)
        m_pParamsDisplay->changeField(
            1, iChannel + 1,
            m_pAccess
                ->getParamValueShort(bank, page, PlugAccess::ElementDesc::FADER,
                                     localCh)
                .toRawUTF8());
      else
        m_pParamsDisplay->changeField(
            1, iChannel + 1,
            m_pAccess
                ->getParamNameShort(bank, page, PlugAccess::ElementDesc::FADER,
                                localCh)
                .toRawUTF8());
    }
  }

  // "Wet" field 9 -> render on the anchor unit when it is ProX (N=1 ProX is
  // byte-identical to the legacy global-flag path; non-ProX anchors skip it).
  Display *anchor = mainChildOrNull(m_pParamsDisplay);
  if (anchor) {
    HardwareUnit *anchorU = m_pCCSManager->getMCU()->unitForChannel(1);
    if (anchorU && anchorU->isProX()) {
      anchor->changeField(2, 9, " Wet");
      int wet = m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DRYWET);
      char text[8];
      sprintf(text, " %3.0f%%", (100 * (double)wet / 16368.));
      anchor->changeField(3, 9, text);
    }
  }
}
void PlugMode::updateTouchedDisplay() {
  if (!m_pAccess->plugExist())
    return;

  // WP-PlugMode Phase 1/6 (R5/R7): the touched element lives on one unit; the
  // owning unit's isProX() selects the 4-row vs 2-row touched layout (was a
  // global CONFIG_FLAG_PROX flag + a separate updateTouchedDisplayProX).
  int touchedCh = (m_iSingleFaderTouched > 0) ? m_iSingleFaderTouched
                                              : m_iSingleVPotTouched;
  int unit = (touchedCh > 0) ? (touchedCh - 1) / 8 : 0;
  setActiveUnit(unit);

  // Resolve target child
  Display *target = m_pTouchedDisplay;
  MultiDisplay *md = dynamic_cast<MultiDisplay *>(target);
  if (md && unit < (int)md->children().size())
    target = md->children()[unit];

  HardwareUnit *hu =
      m_pCCSManager->getMCU()->unitForChannel(touchedCh > 0 ? touchedCh : 1);
  bool prox = hu && hu->isProX();

  if (prox) {
    static int last_iSingleVPotTouched = -1;
    if (m_iSingleVPotTouched != last_iSingleVPotTouched) {
      last_iSingleVPotTouched = m_iSingleVPotTouched;
      target->clearLine(0);
      target->clearLine(1);
    }

    static int last_iSingleFaderTouched = -1;
    if (m_iSingleFaderTouched != last_iSingleFaderTouched) {
      last_iSingleFaderTouched = m_iSingleFaderTouched;
      target->clearLine(2);
      target->clearLine(3);
    }

    if (m_iSingleVPotTouched > 0) {
      target->changeText(0, 0,
          m_pAccess->getBankNameLong(m_pAccess->getSelectedBank()).toRawUTF8(),
          17, true);
      target->changeText(0, 19,
          m_pAccess->getPageNameLongInSelectedBank(
                       m_pAccess->getSelectedPageInSelectedBank()).toRawUTF8(),
          17, true);
      target->changeText(0, 38,
          m_pAccess->getParamNameLong(PlugAccess::ElementDesc::VPOT, m_iSingleVPotTouched - 1).toRawUTF8(),
          17, true);
      target->changeText(1, 0,
          m_pCCSManager->getMCU()->GetTrackName(m_pAccess->getPlugTrack()),
          17, true);
      target->changeText(1, 19,
          m_pAccess->getPlugNameLong().toRawUTF8(), 17, true);
      target->changeText(1, 38,
          m_pAccess->getParamValueLong(PlugAccess::ElementDesc::VPOT, m_iSingleVPotTouched - 1).toRawUTF8(),
          17, true);
    } else {
      for (int iChannel = 0; iChannel < 8; iChannel++) {
        target->changeField(1, iChannel + 1,
            m_pAccess
                ->getParamValueShort(PlugAccess::ElementDesc::VPOT, iChannel)
                .toRawUTF8());

        target->changeField(0, iChannel + 1,
            m_pAccess->getParamNameShort(PlugAccess::ElementDesc::VPOT, iChannel)
                .toRawUTF8());
      }
    }

    if (m_iSingleFaderTouched > 0) {
      target->changeText(2, 0,
          m_pAccess->getBankNameLong(m_pAccess->getSelectedBank()).toRawUTF8(),
          17, true);
      target->changeText(2, 19,
          m_pAccess
              ->getPageNameLongInSelectedBank(
                  m_pAccess->getSelectedPageInSelectedBank())
              .toRawUTF8(),
          17, true);
      target->changeText(2, 38,
          m_pAccess->getParamNameLong(PlugAccess::ElementDesc::FADER, m_iSingleFaderTouched - 1).toRawUTF8(),
          17, true);
      target->changeText(3, 0,
          m_pCCSManager->getMCU()->GetTrackName(m_pAccess->getPlugTrack()),
          17, true);
      target->changeText(3, 19,
          m_pAccess->getPlugNameLong().toRawUTF8(), 17, true);
      target->changeText(3, 38,
          m_pAccess->getParamValueLong(PlugAccess::ElementDesc::FADER, m_iSingleFaderTouched - 1).toRawUTF8(),
          17, true);
    } else {
      for (int iChannel = 0; iChannel < 8; iChannel++) {
        target->changeField(3, iChannel + 1,
            m_pAccess
                ->getParamValueShort(PlugAccess::ElementDesc::FADER, iChannel)
                .toRawUTF8());

        target->changeField(2, iChannel + 1,
            m_pAccess->getParamNameShort(PlugAccess::ElementDesc::FADER, iChannel)
                .toRawUTF8());
      }
      // "Wet" on anchor unit
      Display *anchor = mainChildOrNull(m_pParamsDisplay);
      if (anchor) {
        anchor->changeField(2, 9, " Wet");
        int wet = m_pAccess->getParamValueInt(PlugAccess::ElementDesc::DRYWET);
        char text[8];
        sprintf(text, " %3.0f%%", (100 * (double)wet / 16368.));
        anchor->changeField(3, 9, text);
      }
    }
  } else {
    PlugAccess::ElementDesc::eType element =
        (m_iSingleFaderTouched > 0) ? PlugAccess::ElementDesc::FADER
                                    : PlugAccess::ElementDesc::VPOT;
    int iChannel = (m_iSingleFaderTouched > 0) ? m_iSingleFaderTouched
                                               : m_iSingleVPotTouched;

    target->changeText(0, 0,
        m_pAccess->getBankNameLong(m_pAccess->getSelectedBank()).toRawUTF8(),
        17, true);
    target->changeText(0, 19,
        m_pAccess
            ->getPageNameLongInSelectedBank(
                m_pAccess->getSelectedPageInSelectedBank())
            .toRawUTF8(),
        17, true);
    target->changeText(0, 38,
        m_pAccess->getParamNameLong(element, iChannel - 1).toRawUTF8(),
        17, true);
    target->changeText(1, 0,
        m_pCCSManager->getMCU()->GetTrackName(m_pAccess->getPlugTrack()),
        17, true);
    target->changeText(1, 19,
        m_pAccess->getPlugNameLong().toRawUTF8(), 17, true);
    target->changeText(1, 38,
        m_pAccess->getParamValueLong(element, iChannel - 1).toRawUTF8(),
        17, true);
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

  return String();
}

int PlugMode::getNumPlugsInSelectedTrack() {
  if (selectedTrack()) {
    return TrackFX_GetCount(selectedTrack());
  }

  return 0;
}

String PlugMode::shortPlugName(const char *pName) {
  String name = pName;
  if (name.contains(String(":")))
    return name.fromFirstOccurrenceOf(String(":"), false, false)
        .upToFirstOccurrenceOf(String("("), false, false)
        .substring(1, 7);
  else
    return name.substring(0, 6);
}

String PlugMode::longPlugName(const char *pName) {
  String name = pName;
  if (name.contains(String(":")))
    return name.fromFirstOccurrenceOf(String(":"), false, false)
        .substring(1, 18);
  else
    return name.substring(0, 17);
}

void PlugMode::updateEverything() {
  switchDisplay();
  // WP-PlugMode Phase 3: update all units' selector displays
  for (int u = 0; u < m_pCCSManager->getMCU()->numUnits(); u++)
    m_pBankPagePlugSelectorPerUnit[u]->updateDisplay();
  CCSMode::updateEverything();
}

bool PlugMode::buttonGView(bool pressed) {
  if (!pressed)
    return true;

  m_followTrack = !m_followTrack;
  if (m_followTrack) {
    m_pAccess->trackChanged(selectedTrack());
  }

  m_pBankPagePlugSelectorPerUnit[anchorUnit()]->clearDisplay();
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
  // WP-PlugMode Phase 8d (R8): the on-screen editor reflects m_activeUnit.
  // PlugAccess' active-unit alias layer reads the per-unit bank/page state
  // of whatever unit was last pinned by a unit-specific callback (default
  // 0 = anchor). No structural editor change needed; m_activeUnit is already
  // maintained everywhere a hardware event lands. Opening the editor does
  // not itself pin a unit, so it shows the last hardware-active unit's state.
  // Ensure we have the right track even when the editor is opened via ALT+PLUG
  // without switching to PlugMode as the active mode.
  if (m_followTrack)
    m_pAccess->trackChanged(selectedTrack());

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

	// workaround for controllers, that doesn send all touched events
	// (in combination with the ResetAllTouch action)
	if (m_pCCSManager->getNumFadersTouched() == 0)
		m_iSingleFaderTouched = 0;
	
  updateEverything();
  if (m_pCCSManager->getNumSelectButtonsPressed() == 0 &&
      m_lastTimePlugWasSelected + TIMETOSWITCHPLUGINMS <
          m_pCCSManager->getLastTime()) {
    m_pAccess->checkChainChanges();
  }

  if (m_followTrack && m_pPlugModeOptions->isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS))
    m_pAccess->trackChanged(selectedTrack());

  m_pAccess->checkFloatWindows();

  m_pAccess->getPlugWindowManager()->moveWnd();

	// WP-PlugMode Phase 5a (R2): follow-change now multi-valued; active
	// whenever a valid follow unit is selected.
	if (followChangeUnit() >= 0)
		followChanges();
}

void PlugMode::updateRecLEDs() {
  // WP-PlugMode Phase 3: replicate LED state to all units
  if (!m_pAccess->plugExist())
    return;
  String fxGUID = GUID2String(
      TrackFX_GetFXGUID(m_pAccess->getPlugTrack(), m_pAccess->getPlugSlot()));

  int nUnits = m_pCCSManager->getMCU()->numUnits();
  int start = isModifierPressed(VK_SHIFT) ? 8 : 0;
  for (int unit = 0; unit < nUnits; unit++) {
    for (int channel = 0; channel < 8; channel++) {
      int globalCh = unit * 8 + channel + 1;
      if (channel + start == m_lastCalledPreset[fxGUID]) {
        if (m_pPresetManager->presetMatchState(m_pAccess->getPlugTrack(),
                                               m_pAccess->getPlugSlot(),
                                               channel + start)) {
          m_pCCSManager->setRecLED(
              this, globalCh, isModifierPressed(VK_ALT) ? LED_ON : LED_BLINK);
          continue;
        } else {
          m_lastCalledPreset[fxGUID] = -1;
        }
      }
      m_pCCSManager->setRecLED(
          this, globalCh,
          m_pPresetManager->hasPreset(fxGUID, channel + start) ? LED_ON
                                                               : LED_OFF);
    }
  }
}
bool PlugMode::isSlotBypassed(MediaTrack *pPlugTrack, int iSlot) {
  double min, max;
  int bypassID = TrackFX_GetNumParams(pPlugTrack, iSlot) - 3;
	return TrackFX_GetParam(pPlugTrack, iSlot, bypassID, &min, &max) > 0;
}

void PlugMode::updateSelectLEDs() {
  // WP-PlugMode Phase 3: replicate LED state to all units
  int nUnits = m_pCCSManager->getMCU()->numUnits();
  int start = isModifierPressed(VK_SHIFT) ? 8 : 0;
  if (m_followTrack) {
    for (int unit = 0; unit < nUnits; unit++) {
      for (int channel = 0; channel < 8; channel++) {
        int globalCh = unit * 8 + channel + 1;
        if (channel + start == m_pAccess->getPlugSlot())
          m_pCCSManager->setSelectLED(this, globalCh, LED_BLINK);
        else if (channel + start < getNumPlugsInSelectedTrack()) {
          if (isSlotBypassed(m_pAccess->getPlugTrack(), channel + start))
            m_pCCSManager->setSelectLED(this, globalCh, LED_BLINK_BYPASSED);
          else
            m_pCCSManager->setSelectLED(this, globalCh, LED_ON);
        }
        else
          m_pCCSManager->setSelectLED(this, globalCh, LED_OFF);
      }
    }
  } else {
    for (int unit = 0; unit < nUnits; unit++) {
      for (int channel = 0; channel < 8; channel++) {
        int globalCh = unit * 8 + channel + 1;
        MediaTrack *pMT = NULL;
        if (m_favPlugins[start + channel].get<0>() != GUID_NOT_ACTIVE) {
          pMT = CSurf_MCU::TrackFromGUID(m_favPlugins[start + channel].get<0>());
          int slot = m_favPlugins[start + channel].get<1>();
          if (slot == m_pAccess->getPlugSlot() &&
              pMT == m_pAccess->getPlugTrack() && pMT != NULL)
            m_pCCSManager->setSelectLED(this, globalCh, LED_BLINK);
          else if (pMT != NULL) {
            if (isSlotBypassed(pMT, channel + start))
              m_pCCSManager->setSelectLED(this, globalCh, LED_BLINK_BYPASSED);
            else
              m_pCCSManager->setSelectLED(this, globalCh, LED_ON);
          }
        } else
          m_pCCSManager->setSelectLED(this, globalCh, LED_OFF);
      }
    }
  }
}
bool PlugMode::buttonSelect(int channel, bool pressed) {
  // WP-PlugMode: channel is global, compute unit-local slot
  int unit = (channel - 1) / 8;
  int localCh = (channel - 1) % 8;
  setActiveUnit(unit);

  if (pressed) {
    m_pBankPagePlugSelectorPerUnit[unit]->activateSelector(BankPagePlugSelector::PLUG);
  } else {
    m_pBankPagePlugSelectorPerUnit[unit]->activateSelector(BankPagePlugSelector::NOTHING);
  }

  if (!pressed)
    return true;

  int slot = localCh + (isModifierPressed(VK_SHIFT) ? 8 : 0);
  if (m_followTrack) {
    if (slot < getNumPlugsInSelectedTrack()) {
      m_lastTimePlugWasSelected = m_pCCSManager->getLastTime();
      m_pAccess->accessPlugin(selectedTrack(), slot);
      invalidateParamCache();
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
        invalidateParamCache();
      }
    }
  }

  return true;
}

bool PlugMode::accessFXFavorite(int slot) {
	MediaTrack *pMT = CSurf_MCU::TrackFromGUID(m_favPlugins[slot].get<0>());
	if (pMT) {
		m_lastTimePlugWasSelected = m_pCCSManager->getLastTime();
		m_pAccess->accessPlugin(pMT, m_favPlugins[slot].get<1>());
		invalidateParamCache();
	}

  return true;
}

bool PlugMode::buttonFaderBanks(int button, bool pressed) {
  // WP-PlugMode Phase 4: transport lock-step page-spread over the used-page
  // sequence (R3, R11). Transport buttons (BANK_UP/DOWN, CHANNEL_UP/DOWN)
  // carry no channel and no unit identity, so they operate on a SHARED window
  // across all units, not a per-unit identity. Transport deliberately
  // collapses per-unit Bank divergence back to that shared window.
  //
  // N=1 note: CHANNEL_UP/DOWN stays behaviour-equivalent (steps one used page
  // in the sequence). BANK_UP/DOWN additionally resets each unit's page to
  // the first used page of the new bank (the window position), which is a
  // minor, intended change from the legacy per-bank remembered page.
  setActiveUnit(anchorUnit());

  // Selector activation (press/release) on the anchor unit's selector.
  switch (button) {
  case B_BANK_UP:
  case B_BANK_DOWN:
    if (pressed) {
      m_pBankPagePlugSelectorPerUnit[anchorUnit()]->activateSelector(BankPagePlugSelector::BANK);
    } else if (m_pCCSManager->getNumSoloButtonsPressed() == 0) {
      m_pBankPagePlugSelectorPerUnit[anchorUnit()]->activateSelector(BankPagePlugSelector::NOTHING);
    }
    break;
  case B_CHANNEL_UP:
  case B_CHANNEL_DOWN:
    if (pressed) {
      m_pBankPagePlugSelectorPerUnit[anchorUnit()]->activateSelector(BankPagePlugSelector::PAGE);
    } else if (m_pCCSManager->getNumMuteButtonsPressed() == 0) {
      m_pBankPagePlugSelectorPerUnit[anchorUnit()]->activateSelector(BankPagePlugSelector::NOTHING);
    }
    break;
  }

  if (!pressed)
    return true;

  int a = anchorUnit();
  int N = m_pCCSManager->getMCU()->numUnits();

  switch (button) {
  case B_BANK_UP:
  case B_BANK_DOWN: {
    // Bank UP/DOWN: reference bank = anchor unit's bank; find the next/prev
    // USED bank and reset the page window to sequence positions [0..N-1]
    // across all units.
    int refBank = m_pAccess->selectedBankForUnit(a);
    int newBank = refBank;
    if (button == B_BANK_UP) {
      for (int b = refBank + 1; b < 8; b++) {
        if (m_pAccess->isBankUsed(b)) {
          newBank = b;
          break;
        }
      }
    } else {
      for (int b = refBank - 1; b >= 0; b--) {
        if (m_pAccess->isBankUsed(b)) {
          newBank = b;
          break;
        }
      }
    }
    if (newBank != refBank) {
      for (int u = 0; u < N; u++) {
        m_pAccess->setSelectedBank(newBank, u);
        m_pAccess->setSelectedPage(newBank,
                                   m_pAccess->pageAtUsedOffset(newBank, u), u);
      }
      safe_call(m_pPlugEditor, selectedBankChanged(newBank));
    }
    break;
  }
  case B_CHANNEL_UP:
  case B_CHANNEL_DOWN: {
    // Page UP/DOWN: shift the page window start by +/-N sequence positions;
    // each unit u shows pageAtUsedOffset(bank, newOffset + u). Clamp, no wrap.
    int bank = m_pAccess->selectedBankForUnit(a);
    int curOffset = m_pAccess->pageUsedOffsetForPage(
        bank, m_pAccess->selectedPageForUnit(a));
    if (curOffset < 0)
      curOffset = 0;
    int count = m_pAccess->usedPageCount(bank);
    int delta = (button == B_CHANNEL_UP) ? N : -N;
    int newOffset = curOffset + delta;
    if (count <= 0) {
      newOffset = 0;
    } else {
      if (newOffset < 0)
        newOffset = 0;
      if (newOffset > count - 1)
        newOffset = count - 1;
    }
    if (newOffset != curOffset) {
      for (int u = 0; u < N; u++) {
        m_pAccess->setSelectedPage(
            bank, m_pAccess->pageAtUsedOffset(bank, newOffset + u), u);
      }
      safe_call(m_pPlugEditor,
                selectedPageChanged(m_pAccess->pageAtUsedOffset(bank, newOffset)));
    }
    break;
  }
  }

  switchDisplay();

  return true;
}

PlugMode::tFav PlugMode::getFavorite(unsigned i) {
  ASSERT(i < NUM_FAVORITES);
  return m_favPlugins[i];
}

#define PLUGMODE_NODE_ROOT String("PLUGMODE")
#define PLUGMODE_NODE_LAST_CALLED_PRESETS String("LAST_CALLED_PRESETS")
#define PLUGMODE_NODE_FAV String("FAVORITE")
#define PLUGMODE_NODE_LAST_CALLED_PRESET String("PRESET")
#define PLUGMODE_ATT_FAV_INDEX String("index")
#define PLUGMODE_ATT_FAV_TRACK String("track")
#define PLUGMODE_ATT_FAV_SLOT String("slot")
#define PLUGMODE_ATT_LAST_CALLED_PRESET_FXGUID String("fxguid")
#define PLUGMODE_ATT_LAST_CALLED_PRESET_PRESET String("preset")

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
  GUID oldGUID = *CSurf_MCU::GUIDfromTrack(pOldTrack);
  GUID newGUID = pNewTrack != NULL ? *CSurf_MCU::GUIDfromTrack(pNewTrack) : GUID_NOT_ACTIVE;

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

  // WP-PlugMode Phase 5c (R2): refill the cache on first scan and after
  // every plugin/map change so the old map's values are not seen as changes.
  if (!m_paramCacheValid) {
    refillParamCache();
    return;
  }

  int numChangedValues = 0;
	int changeInBank = -1;
	int changeInPage = -1;

	for (int bank = 0; bank < 8; bank++) {
		for (int page = 0; page < 8; page ++) {
			for (int channel = 0; channel < 8; channel++) {
				PlugAccess::ElementDesc faderDesc(bank, page, PlugAccess::ElementDesc::FADER, channel);
				double faderVal = m_pAccess->getParamValueDouble(&faderDesc);
				PlugAccess::ElementDesc vpotDesc(bank, page, PlugAccess::ElementDesc::VPOT, channel);
				double vpotVal = m_pAccess->getParamValueDouble(&vpotDesc);
				int idx = paramCacheIndex(bank, page, channel);
				if (faderVal != lastFaderValues[idx] ||
						vpotVal != lastVPotValues[idx]) {
					numChangedValues++;
					changeInBank = bank;
					changeInPage = page;
					lastFaderValues[idx] = faderVal;
					lastVPotValues[idx] = vpotVal;
				}
			}
		}
	}

	if (numChangedValues == 1) {
		// WP-PlugMode Phase 5b (R2): jump the selected follow-unit's cursor,
		// not the active unit's. With OFF (fu < 0) no action is taken.
		int fu = followChangeUnit();
		if (fu >= 0) {
			m_pAccess->setSelectedBank(changeInBank, fu);
			m_pAccess->setSelectedPage(changeInBank, changeInPage, fu);
		}
	}
}

int PlugMode::followChangeUnit() {
	return m_pPlugMode2ndOptions->followChangeUnit(
		m_pCCSManager->getMCU()->numUnits());
}

void PlugMode::invalidateParamCache() { m_paramCacheValid = false; }

void PlugMode::refillParamCache() {
	for (int bank = 0; bank < 8; bank++) {
		for (int page = 0; page < 8; page++) {
			for (int channel = 0; channel < 8; channel++) {
				int idx = paramCacheIndex(bank, page, channel);
				PlugAccess::ElementDesc faderDesc(bank, page,
														 PlugAccess::ElementDesc::FADER, channel);
				lastFaderValues[idx] = m_pAccess->getParamValueDouble(&faderDesc);
				PlugAccess::ElementDesc vpotDesc(bank, page,
														PlugAccess::ElementDesc::VPOT, channel);
				lastVPotValues[idx] = m_pAccess->getParamValueDouble(&vpotDesc);
			}
		}
	}
	m_paramCacheValid = true;
}

// MediaTrack* PlugMode::selectedTrack()
// {
// 	if (Tracks::instance()->getNumberOfSelectedTracks() == 0) {
// 		return GetMasterTrack(NULL);
// 	}

// 	return Tracks::instance()->getSelectedSingleTrack();
// }

MediaTrack *PlugMode::selectedTrack() {
  return Tracks::instance()->getSelectedSingleTrack(true);
}

void PlugMode::plugChanged() {
	 if (m_pPlugEditor) m_pPlugEditor->changePlug(m_pAccess);
}

// ---- WP-PlugMode: active unit & display helpers (Phase 0) ----

void PlugMode::setActiveUnit(int unit) {
  ASSERT(unit >= 0 && unit < m_pCCSManager->getMCU()->numUnits());
  m_activeUnit = unit;
}

int PlugMode::anchorUnit() {
  // Return first main-capable unit, else 0.
  // Zero-main-unit configs are valid — anchor falls back to unit 0.
  for (int i = 0; i < m_pCCSManager->getMCU()->numUnits(); i++) {
    HardwareUnit *u = m_pCCSManager->getMCU()->unitForChannel(i * 8 + 1);
    if (u && u->isMain())
      return i;
  }
  return 0;
}

Display *PlugMode::mainChildOrNull(Display *d) {
  MultiDisplay *md = dynamic_cast<MultiDisplay *>(d);
  if (!md)
    return d; // N=1: plain Display, no composite
  int a = anchorUnit();
  if (a < (int)md->children().size())
    return md->children()[a];
  return NULL;
}

void PlugMode::clearNonAnchorChildren(Display *d) {
  MultiDisplay *md = dynamic_cast<MultiDisplay *>(d);
  if (!md)
    return;
  int a = anchorUnit();
  for (int i = 0; i < (int)md->children().size(); i++) {
    if (i != a)
      md->children()[i]->clear();
  }
}

// ---- WP-PlugMode Phase 4b: Control+cascade (R3) ----
// Spread bank/page selection to units `unit..N-1`. Unit u gets the bank and
// the page at sequence offset `baseOffset + (u - unit)` in that bank's
// used-page list. Units 0..unit-1 are left untouched. Called from
// Control+SOLO so the page window can be re-anchored starting at any unit.
void PlugMode::cascadeFromUnit(int unit, int bank, int baseOffset) {
  int N = m_pCCSManager->getMCU()->numUnits();
  // Page math runs on the RESOLVED bank (a bank may `refer` to another, where
  // the used-page sequence actually lives), but the selection is stored under
  // the RAW bank (setSelectedBank/setSelectedPage key on the raw bank so
  // selectedPageForUnit reads it back correctly).
  int resolved = m_pAccess->resolveBankReference(bank);
  for (int u = unit; u < N; u++) {
    int page = m_pAccess->pageAtUsedOffset(resolved, baseOffset + (u - unit));
    m_pAccess->setSelectedBank(bank, u);
    m_pAccess->setSelectedPage(bank, page, u);
  }
}
