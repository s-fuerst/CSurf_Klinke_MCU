/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "Options.h"

#define MTO2_TCP_ADJUCT JUCE_T("Adjust TCP")
#define MTO2A_TCP_NO JUCE_T("don't adjust")
#define MTO2A_TCP_BANK JUCE_T("hide non MCU")
#define MTO2A_TCP_SELECTED JUCE_T("only selected")
#define MTO2A_TCP_ALL JUCE_T("show all")

#define MTO2_MCP_ADJUCT JUCE_T("Adjust Mixer")
#define MTO2A_MCP_NO JUCE_T("don't adjust")
#define MTO2A_MCP_BANK JUCE_T("hide non MCU")
#define MTO2A_MCP_ALL JUCE_T("show all")

#define MTO2_FOLLOW_REAPER JUCE_T("Follow Reaper")
#define MTO2A_FOLLOW_REAPER_OFF JUCE_T("no")
#define MTO2A_FOLLOW_REAPER_ON JUCE_T("yes")

#define MTO2_AUTO_TOUCH JUCE_T("Touch select")
#define MTO2A_AUTO_TOUCH_OFF JUCE_T("no")
#define MTO2A_AUTO_TOUCH_ON JUCE_T("yes")

class MultiTrackOptions2 : public Options {
public:
  MultiTrackOptions2(DisplayHandler *pDH);

public:
  virtual ~MultiTrackOptions2(void);

protected:
  String getConfigFileName();

  void activateSelector();
  void checkAndModifyOptions();

  bool m_tcpNotAdjust;
  bool m_mcpNotAdjust;
};
