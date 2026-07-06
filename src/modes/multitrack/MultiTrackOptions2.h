/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "Options.h"

#define MTO2_TCP_ADJUCT String("Adjust TCP")
#define MTO2A_TCP_NO String("don't adjust")
#define MTO2A_TCP_BANK String("hide non MCU")
#define MTO2A_TCP_SELECTED String("only selected")
#define MTO2A_TCP_ALL String("show all")

#define MTO2_MCP_ADJUCT String("Adjust Mixer")
#define MTO2A_MCP_NO String("don't adjust")
#define MTO2A_MCP_BANK String("hide non MCU")
#define MTO2A_MCP_ALL String("show all")

#define MTO2_FOLLOW_REAPER String("Follow Reaper")
#define MTO2A_FOLLOW_REAPER_OFF String("no")
#define MTO2A_FOLLOW_REAPER_ON String("yes")

#define MTO2_AUTO_TOUCH String("Touch select")
#define MTO2A_AUTO_TOUCH_OFF String("no")
#define MTO2A_AUTO_TOUCH_ON String("yes")

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
