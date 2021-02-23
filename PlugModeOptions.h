/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "options.h"

#define PMO_MCU_FOLLOW JUCE_T("MCU follow")
#define PMO_GUI_FOLLOW JUCE_T("GUI follow")
#define PMO_ALT_OPEN JUCE_T("ALT SELECT")
#define PMO_LIMIT_FLOATING JUCE_T("Limit float")

// MCU follow
#define PMOA_OFF JUCE_T("off")
#define PMOA_SAME_TRACK JUCE_T("same track")
#define PMOA_ALWAYS JUCE_T("always")

// GUI follow
#define PMOA_IF_CHAIN_OPEN JUCE_T("if chain open")
#define PMOA_OPEN_CHAIN JUCE_T("open chain")
#define PMOA_OPEN_FLOATING JUCE_T("open floating")

// ALT_OPEN
// PMOA_OPEN_CHAIN
#define PMOA_OPEN_CHAIN_CLOSE_FLOAT JUCE_T("chain -float")
// PMOA_OPEN_FLOATING

// LIMIT_FLOATING
// PMOA_OFF
#define PMOA_ONLY_ONE_MCU JUCE_T("only 1 MCU")
#define PMOA_ONLY_ONE_GLOBAL JUCE_T("only selected")
#define PMOA_ONLY_CHAIN JUCE_T("only chain")

class PlugModeOptions : public Options {
public:
  PlugModeOptions(DisplayHandler *pDH);

public:
  virtual ~PlugModeOptions(void);

protected:
  String getConfigFileName();

  void checkAndModifyOptions();

private:
  bool m_bMCUFollowModified;
  int m_iLastMCUFollowOption;
};
