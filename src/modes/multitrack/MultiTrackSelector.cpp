/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "MultiTrackSelector.h"
#include "csurf_mcu.h"
#include "Tracks.h"
#include "MultiTrackOptions.h"
#include "MultiDisplay.h"
#include "HardwareUnit.h"

MultiTrackSelector::MultiTrackSelector(DisplayHandler *pDH, CSurf_MCU *pMCU)
    : Selector(pDH, NULL) {
  if (pMCU->numUnits() > 1) {
    MultiDisplay *md = new MultiDisplay(pDH, 2);
    for (int i = 0; i < pMCU->numUnits(); i++) {
      HardwareUnit *u = pMCU->unitForChannel(i * 8 + 1);
      if (u) {
        Display *child = new Display(u->displayHandler(), 2);
        md->addChild(child);
      }
    }
    m_pDisplay = md;
  } else {
    m_pDisplay = new Display(pDH, 2);
  }
}

MultiTrackSelector::~MultiTrackSelector(void) {}

void MultiTrackSelector::activateSelector() {
  gatherQuickJumps();

  m_pDisplay->clear();
  for (int i = 0; i < Tracks::instance()->getNumberOfChannelStrips(); i++) {
    if (m_quickJumps[i]) {
      m_pDisplay->changeField(
          asRoot(m_quickJumps[i]) ? 0 : 1, i + 1,
          m_quickJumps[i]->showQuickNameInDisplay().toRawUTF8());
    }
  }
  MultiDisplay *md = dynamic_cast<MultiDisplay *>(m_pDisplay);
  if (md)
    md->switchToAll();
  else
    m_pDisplayHandler->switchTo(m_pDisplay);
}

bool MultiTrackSelector::select(int index) {
  if (m_quickJumps[index]) {
    if (asRoot(m_quickJumps[index])) {
      Tracks::instance()->setGlobalOffset(0);
      Tracks::instance()->moveBaseTrack(m_quickJumps[index]->getMediaTrack());
    } else {
      Tracks::instance()->moveTrackToLeftMostChannel(
          m_quickJumps[index]->getMediaTrack());
    }
    return false;
  }

  return true;
}

void MultiTrackSelector::gatherQuickJumps() {
  m_quickJumps.clear();
  m_quickJumps.resize(Tracks::instance()->getNumberOfChannelStrips(), NULL);

  for (TrackIterator ti; !ti.end(); ++ti) {
    TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(*ti);
    if (pTS->getQuickJumpChannel() > 0 &&
        pTS->getQuickJumpChannel() <= (int)m_quickJumps.size()) {
      m_quickJumps[pTS->getQuickJumpChannel() - 1] = pTS;
    }
  }
}

bool MultiTrackSelector::asRoot(TrackState *pTS) {
  return (pTS->useAsRootInQuick() &&
          !Tracks::instance()->getOptions()->isOptionSetTo(MTO_REFLECT_FOLDER,
                                                           MTOA_REFLECT_NO));
}
