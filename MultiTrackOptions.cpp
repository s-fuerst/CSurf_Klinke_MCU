/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "MultiTrackOptions.h"
#include "Tracks.h"

MultiTrackOptions::MultiTrackOptions(DisplayHandler *pDH) : Options(pDH) {
  addOption(MTO_SHOW);
  addAttribute(MTO_SHOW, MTOA_SHOW_ALL, true);
  addAttribute(MTO_SHOW, MTOA_SHOW_SET);
  addAttribute(MTO_SHOW, MTOA_SHOW_MCP);
  addAttribute(MTO_SHOW, MTOA_SHOW_TCP);
  //  addAttribute(MTO_SHOW, MTOA_SHOW_SENDS);
  //  addAttribute(MTO_SHOW, MTOA_SHOW_RECEIVES);

  addOption(MTO_REFLECT_FOLDER);
  addAttribute(MTO_REFLECT_FOLDER, MTOA_REFLECT_NO, true);
  addAttribute(MTO_REFLECT_FOLDER, MTOA_REFLECT_YES);
  addAttribute(MTO_REFLECT_FOLDER, MTOA_REFLECT_PLUS);

  addOption(MTO_DISABLE_ANCHORS);
  addAttribute(MTO_DISABLE_ANCHORS, MTOA_ANCHORS_NO);
  addAttribute(MTO_DISABLE_ANCHORS, MTOA_ANCHORS_YES, true);

  readConfigFile();

  m_iReflect = getSelectedOption(MTO_REFLECT_FOLDER);
}

MultiTrackOptions::~MultiTrackOptions(void) { writeConfigFile(); }

String MultiTrackOptions::getConfigFileName() {
  return String("MultiTrackOptions1");
}

void MultiTrackOptions::checkAndModifyOptions() {
  if (m_iReflect != getSelectedOption(MTO_REFLECT_FOLDER)) {
    m_iReflect = getSelectedOption(MTO_REFLECT_FOLDER);
    Tracks::instance()->setGlobalOffset(0);
    Tracks::instance()->tracksStatesChanged();
    if (!m_iReflect)
      while (Tracks::instance()->moveBaseTrackToParent()) {
      }
  }

  Tracks::instance()->buildGraph();
  //Tracks::instance()->moveSelectedTrack2MCU();

  Options::checkAndModifyOptions();
}
