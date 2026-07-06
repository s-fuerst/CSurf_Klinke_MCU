/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "MultiTrackOptions2.h"
#include "Tracks.h"

MultiTrackOptions2::MultiTrackOptions2(DisplayHandler *pDH) : Options(pDH) {
  addOption(MTO2_TCP_ADJUCT);
  addAttribute(MTO2_TCP_ADJUCT, MTO2A_TCP_NO, true);
  addAttribute(MTO2_TCP_ADJUCT, MTO2A_TCP_BANK);
  addAttribute(MTO2_TCP_ADJUCT, MTO2A_TCP_SELECTED);
  addAttribute(MTO2_TCP_ADJUCT, MTO2A_TCP_ALL);

  addOption(MTO2_MCP_ADJUCT);
  addAttribute(MTO2_MCP_ADJUCT, MTO2A_MCP_NO, true);
  addAttribute(MTO2_MCP_ADJUCT, MTO2A_MCP_BANK);
  addAttribute(MTO2_MCP_ADJUCT, MTO2A_MCP_ALL);

  addOption(MTO2_FOLLOW_REAPER);
  addAttribute(MTO2_FOLLOW_REAPER, MTO2A_FOLLOW_REAPER_OFF);
  addAttribute(MTO2_FOLLOW_REAPER, MTO2A_FOLLOW_REAPER_ON, true);

  addOption(MTO2_AUTO_TOUCH);
  addAttribute(MTO2_AUTO_TOUCH, MTO2A_AUTO_TOUCH_OFF);
  addAttribute(MTO2_AUTO_TOUCH, MTO2A_AUTO_TOUCH_ON);

  readConfigFile();
}

MultiTrackOptions2::~MultiTrackOptions2(void) { writeConfigFile(); }

String MultiTrackOptions2::getConfigFileName() {
  return String("MultiTrackOptions2");
}

void MultiTrackOptions2::activateSelector() {
  m_tcpNotAdjust = isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_NO);
  m_mcpNotAdjust = isOptionSetTo(MTO2_MCP_ADJUCT, MTO2A_MCP_NO);
  Options::activateSelector();
}

void MultiTrackOptions2::checkAndModifyOptions() {
  if (m_tcpNotAdjust != isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_NO)) {
    m_tcpNotAdjust = !m_tcpNotAdjust;
    if (m_tcpNotAdjust) {
      Tracks::instance()->setTCP2TrackStates();
    }
  }

  if (m_mcpNotAdjust != isOptionSetTo(MTO2_MCP_ADJUCT, MTO2A_MCP_NO)) {
    m_mcpNotAdjust = !m_mcpNotAdjust;
    if (m_mcpNotAdjust) {
      Tracks::instance()->setMCP2TrackStates();
    }
  }

  Tracks::instance()->moveSelectedTrack2MCU();

  Options::checkAndModifyOptions();
}
