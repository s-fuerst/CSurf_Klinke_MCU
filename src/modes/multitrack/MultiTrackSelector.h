/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "Selector.h"
#include <vector>

class TrackState;
class CSurf_MCU;

class MultiTrackSelector : public Selector {
public:
  MultiTrackSelector(DisplayHandler *pDH, CSurf_MCU *pMCU);
  ~MultiTrackSelector(void);

  void activateSelector();
  // returns true if selector should still be active
  bool select(int index); // 0-7

protected:
  void gatherQuickJumps();
  bool asRoot(TrackState *pTS);

  typedef std::vector<TrackState *> tTrackStates;
  tTrackStates m_quickJumps;
};
