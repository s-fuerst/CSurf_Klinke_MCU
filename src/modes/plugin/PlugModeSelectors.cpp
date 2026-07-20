/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PlugModeSelectors.h"
#include "PlugMode.h"
#include "Display.h"
#include "PlugAccess.h"

void PlugSelector::activateSelector() {
  m_startWith = 0;
  m_pDisplay->clearLine(0);
  writeTrackPlugTopLine();
  fillPlugNames();
  m_pDisplayHandler->switchTo(m_pDisplay);
}

void PlugModeSelector::writeTrackPlugTopLine() {
  if (m_pPlugMode->getPlugAccess()->plugExist()) {
    m_pDisplay->changeText(0, 0, "Track ", 6);
    m_pDisplay->changeText(0, 6,
                           m_pPlugMode->getCCSManager()->getMCU()->GetTrackName(
                               m_pPlugMode->getPlugAccess()->getPlugTrack()),
                           17);
    m_pDisplay->changeText(
        0, 28,
        String::formatted(String("FX%2d "),
                          m_pPlugMode->getPlugAccess()->getPlugSlot() + 1)
            .toRawUTF8(),
        10);
    m_pDisplay->changeText(
        0, 33, m_pPlugMode->getPlugAccess()->getPlugNameLong().toRawUTF8(), 17);
  }
}

void PlugModeSelector::writePlugBankPageTopLine() {
  PlugAccess *pPA = m_pPlugMode->getPlugAccess();
  if (pPA->plugExist()) {
    int u = m_pPlugMode->getActiveUnit();
    m_pDisplay->changeText(0, 0, pPA->getPlugNameLong().toRawUTF8(), 17, true);
    m_pDisplay->changeText(
        0, 19, pPA->getBankNameLong(pPA->selectedBankForUnit(u)).toRawUTF8(), 17,
        true);
    m_pDisplay->changeText(
        0, 38,
        pPA->getPageNameLongInSelectedBank(pPA->selectedPageForUnit(u))
            .toRawUTF8(),
        17, true);
  }
}

void PlugSelector::fillPlugNames() {
  m_pDisplay->clearLine(1);

  if (m_startWith > 0)
    m_pDisplay->changeField(1, 1, "  <<");
  else
    m_pDisplay->changeField(1, 1, "");

  if (m_pPlugMode->getNumPlugsInSelectedTrack() > (m_startWith + 6))
    m_pDisplay->changeField(1, 8, "  >>");
  else
    m_pDisplay->changeField(1, 8, "");

  for (int i = 0; i < 6; i++)
    m_pDisplay->changeField(
        1, i + 2, m_pPlugMode->getPlugNameShort(i + m_startWith).toRawUTF8());

  if (m_pPlugMode->getNumPlugsInSelectedTrack() == 0)
    m_pDisplay->changeTextFullLine(1, "No FX exist in selected track.", true);
}

bool PlugSelector::select(int index) {
  if (index == 0) {
    if (m_startWith > 0) {
      m_startWith -= 6;
      fillPlugNames();
    }
    return true;
  } else if (index == 7 &&
             m_pPlugMode->getNumPlugsInSelectedTrack() > (m_startWith + 6)) {
    m_startWith += 6;
    fillPlugNames();
    return true;
  }

  int selectedSlot = index - 1 + m_startWith;
  if (selectedSlot < m_pPlugMode->getNumPlugsInSelectedTrack()) {
    m_pPlugMode->setLastTimePlugWasSelected(
        m_pPlugMode->getCCSManager()->getLastTime());
    m_pPlugMode->getPlugAccess()->accessPlugin(m_pPlugMode->selectedTrack(),
                                               index - 1 + m_startWith);
    writeTrackPlugTopLine();
  }

  return true;
}

BankPagePlugSelector::BankPagePlugSelector(DisplayHandler *pDH, PlugMode *pPM, int unit)
    : PlugModeSelector(pDH, pPM), m_selectWhat(NOTHING), m_unit(unit) {}

void BankPagePlugSelector::activateSelector() {}

void BankPagePlugSelector::activateSelector(eSelect newSelect) {
  if (newSelect != m_selectWhat) {
    m_selectWhat = newSelect;

    m_pDisplay->clear();
    updateDisplay();
  }
}

// void BankPagePlugSelector::setWhatToSelect(eSelect newSelect) {
//  if (newSelect != m_selectWhat) {
//    m_selectWhat = newSelect;
//  }
// }

bool BankPagePlugSelector::select(int index) {
  //  updateDisplay();
  //
  return true;
}

void BankPagePlugSelector::updateDisplay() {
  PlugAccess *pPA = m_pPlugMode->getPlugAccess();
  // WP-PlugMode Phase 3: read per-unit state
  m_pPlugMode->setActiveUnit(m_unit);

  switch (m_selectWhat) {
  case NOTHING:
    break;
  case BANK:
    writePlugBankPageTopLine();
    for (int i = 0; i < 8; i++) {
      if (pPA->isBankUsed(i))
        m_pDisplay->changeField(1, i + 1, pPA->getBankNameShort(i).toRawUTF8(),
                                true);
    }
    m_pDisplay->resendRow(1);
    break;
  case PAGE:
    writePlugBankPageTopLine();
    for (int i = 0; i < 8; i++) {
      if (pPA->isPageUsedInSelectedBank(i))
        m_pDisplay->changeField(
            1, i + 1, pPA->getPageNameShortInSelectedBank(i).toRawUTF8(), true);
    }
    m_pDisplay->resendRow(1);
    break;
  case PLUG:
    if (m_pPlugMode->isFollowTrack()) {
      writeTrackPlugTopLine();
      int offset = m_pPlugMode->isModifierPressed(VK_SHIFT) ? 8 : 0;
      for (int i = 0; i < 8; i++) {
        m_pDisplay->changeField(
            1, i + 1,
            pPA->getPlugNameShort(m_pPlugMode->selectedTrack(), i + offset)
                .toRawUTF8());
      }
    } else {
      int offset = m_pPlugMode->isModifierPressed(VK_SHIFT) ? 8 : 0;
      for (int i = 0; i < 8; i++) {
        PlugMode::tFav fav = m_pPlugMode->getFavorite(i + offset);
        if (fav.get<0>() != GUID_NOT_ACTIVE) {
          m_pDisplay->changeField(
              0, i + 1,
              m_pPlugMode->getCCSManager()->getMCU()->GetTrackName(
                  CSurf_MCU::TrackFromGUID(fav.get<0>())));
          m_pDisplay->changeField(
              1, i + 1,
              pPA->getPlugNameShort(CSurf_MCU::TrackFromGUID(fav.get<0>()),
                                    fav.get<1>())
                  .toRawUTF8());
        } else {
          m_pDisplay->changeField(0, i + 1, "");
          m_pDisplay->changeField(1, i + 1, "");
        }
      }
    }
    break;
  }
}
