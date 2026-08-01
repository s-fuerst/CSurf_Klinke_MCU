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
  // Multi-unit plugin list: render the window on every unit's display and
  // route each unit's handler to it. Field 1 of unit 0 is the scroll-back
  // slot, field 8 of the last unit the scroll-forward slot; every other
  // field across all units shows one plugin of the contiguous window.
  renderAllUnits();
}

void PlugModeSelector::writeTrackPlugTopLine(Display *d) {
  if (m_pPlugMode->getPlugAccess()->plugExist()) {
    d->changeText(0, 0, "Track ", 6);
    d->changeText(0, 6,
                  m_pPlugMode->getCCSManager()->getMCU()->GetTrackName(
                      m_pPlugMode->getPlugAccess()->getPlugTrack()),
                  17);
    d->changeText(
        0, 28,
        String::formatted(String("FX%2d "),
                          m_pPlugMode->getPlugAccess()->getPlugSlot() + 1)
            .toRawUTF8(),
        10);
    d->changeText(0, 33,
                  m_pPlugMode->getPlugAccess()->getPlugNameLong().toRawUTF8(),
                  17);
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

void PlugSelector::renderAllUnits() {
  int nUnits = m_pPlugMode->getCCSManager()->getMCU()->numUnits();
  for (int u = 0; u < nUnits; u++) {
    Display *d = m_pPlugMode->selectorDisplayForUnit(u);
    if (!d)
      continue;
    d->clear();
    writeTrackPlugTopLine(d);
    fillPlugNames(d, u);
    m_pPlugMode->selectorHandlerForUnit(u)->switchTo(d);
  }
}

void PlugSelector::refreshHeaderAllUnits() {
  int nUnits = m_pPlugMode->getCCSManager()->getMCU()->numUnits();
  for (int u = 0; u < nUnits; u++) {
    Display *d = m_pPlugMode->selectorDisplayForUnit(u);
    if (d)
      writeTrackPlugTopLine(d);
  }
}

void PlugSelector::fillPlugNames(Display *d, int unit) {
  d->clearLine(1);
  int nUnits = m_pPlugMode->getCCSManager()->getMCU()->numUnits();
  int numPlugs = m_pPlugMode->getNumPlugsInSelectedTrack();
  int window = nUnits * 8 - 2; // selectable plugin slots per page

  for (int f = 0; f < 8; f++) {
    int globalPos = unit * 8 + f; // 0 .. nUnits*8-1
    if (globalPos == 0) {
      // first field of unit 0: scroll-back indicator
      d->changeField(1, f + 1, m_startWith > 0 ? "  <<" : "");
    } else if (globalPos == nUnits * 8 - 1) {
      // last field of the last unit: scroll-forward indicator
      d->changeField(1, f + 1, numPlugs > m_startWith + window ? "  >>" : "");
    } else {
      int slot = m_startWith + (globalPos - 1);
      d->changeField(1, f + 1,
                     slot < numPlugs
                         ? m_pPlugMode->getPlugNameShort(slot).toRawUTF8()
                         : "");
    }
  }

  if (numPlugs == 0)
    d->changeTextFullLine(1, "No FX exist in selected track.", true);
}

bool PlugSelector::select(int index) {
  int nUnits = m_pPlugMode->getCCSManager()->getMCU()->numUnits();
  int window = nUnits * 8 - 2;
  int lastPos = nUnits * 8 - 1;

  if (index == 0) {
    // scroll back: first VPOT of unit 0
    if (m_startWith > 0) {
      m_startWith -= window;
      if (m_startWith < 0)
        m_startWith = 0;
      renderAllUnits();
    }
    return true;
  } else if (index == lastPos &&
             m_pPlugMode->getNumPlugsInSelectedTrack() > m_startWith + window) {
    // scroll forward: last VPOT of the last unit
    m_startWith += window;
    renderAllUnits();
    return true;
  }

  int slot = m_startWith + (index - 1);
  if (slot < m_pPlugMode->getNumPlugsInSelectedTrack()) {
    m_pPlugMode->setLastTimePlugWasSelected(
        m_pPlugMode->getCCSManager()->getLastTime());
    m_pPlugMode->getPlugAccess()->accessPlugin(m_pPlugMode->selectedTrack(),
                                               slot);
    refreshHeaderAllUnits();
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
  // read per-unit state
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
    renderPlugOverlay();
    break;
  }
}

void BankPagePlugSelector::renderPlugOverlay() {
  PlugAccess *pPA = m_pPlugMode->getPlugAccess();
  // read per-unit state. This also runs for units that are in NOTHING state
  // but receive the broadcast overlay while Select is held on another unit.
  m_pPlugMode->setActiveUnit(m_unit);

  if (m_pPlugMode->isFollowTrack()) {
    writeTrackPlugTopLine(m_pDisplay);
    // mirror the per-unit Select-button mapping in updateSelectLEDs:
    // unit N -> slots N*8..N*8+7, Shift advances the window by 8*nUnits.
    int nUnits = m_pPlugMode->getCCSManager()->getMCU()->numUnits();
    int offset = m_unit * 8 +
                 (m_pPlugMode->isModifierPressed(VK_SHIFT) ? 8 * nUnits : 0);
    for (int i = 0; i < 8; i++) {
      m_pDisplay->changeField(
          1, i + 1,
          pPA->getPlugNameShort(m_pPlugMode->selectedTrack(), i + offset)
              .toRawUTF8());
    }
  } else {
    int nUnits = m_pPlugMode->getCCSManager()->getMCU()->numUnits();
    int offset = m_unit * 8 +
                 (m_pPlugMode->isModifierPressed(VK_SHIFT) ? 8 * nUnits : 0);
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
}
