/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "options.h"

#define PMO2_MODE_CHANGE JUCE_T("Mode change")
#define PMO2A_NOTHING JUCE_T("do nothing")
#define PMO2A_OPEN_CLOSE JUCE_T("show/hide wnd")
#define PMO2A_OPEN_CLOSE_MIXER JUCE_T("s/h wnd|mixer")

#define PMO2_MOVE JUCE_T("Move top left")
#define PMO2A_OFF JUCE_T("off")
#define PMO2A_ON JUCE_T("on")

class PlugMode2ndOptions :
public Options
{
 public:
  PlugMode2ndOptions(DisplayHandler* pDH);
 public:
  virtual ~PlugMode2ndOptions(void);

  String getConfigFileName();
};
