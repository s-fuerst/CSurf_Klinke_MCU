/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "Options.h"

#define PMO2_MODE_CHANGE String("Mode change")
#define PMO2A_NOTHING String("do nothing")
#define PMO2A_OPEN_CLOSE String("show/hide wnd")
#define PMO2A_OPEN_CLOSE_MIXER String("s/h wnd|mixer")

#define PMO2_MOVE String("Move top left")
#define PMO2A_OFF String("off")
#define PMO2A_ON String("on")

#define PMO2_SHOW_DETAILS String("Touch details")

#define PMO2_FOLLOW_CHANGE String("Follow change")

class PlugMode2ndOptions : public Options {
public:
  PlugMode2ndOptions(DisplayHandler *pDH);

public:
  virtual ~PlugMode2ndOptions(void);

  String getConfigFileName();
};
