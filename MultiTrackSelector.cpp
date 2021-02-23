/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "MultiTrackSelector.h"
#include "csurf_mcu.h"
#include "Tracks.h"
#include "MultiTrackOptions.h"

MultiTrackSelector::MultiTrackSelector(DisplayHandler *pDH) : Selector(pDH) {}

MultiTrackSelector::~MultiTrackSelector(void) {}

void MultiTrackSelector::activateSelector() {
  gatherQuickJumps();

  m_pDisplay->clear();
  for (int i = 0; i < 8; i++) {
    if (m_quickJumps[i]) {
      m_pDisplay->changeField(
          asRoot(m_quickJumps[i]) ? 0 : 1, i + 1,
          m_quickJumps[i]->showQuickNameInDisplay().toCString());
    }
  }
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
  m_quickJumps.resize(8, NULL);

  for (TrackIterator ti; !ti.end(); ++ti) {
    TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(*ti);
    if (pTS->getQuickJumpChannel() > 0) {
      m_quickJumps[pTS->getQuickJumpChannel() - 1] = pTS;
    }
  }
}

bool MultiTrackSelector::asRoot(TrackState *pTS) {
  return (pTS->useAsRootInQuick() &&
          !Tracks::instance()->getOptions()->isOptionSetTo(MTO_REFLECT_FOLDER,
                                                           MTOA_REFLECT_NO));
}
