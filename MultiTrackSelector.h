/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "selector.h"
#include <vector>

class TrackState;

class MultiTrackSelector : public Selector {
public:
  MultiTrackSelector(DisplayHandler *pDH);
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
