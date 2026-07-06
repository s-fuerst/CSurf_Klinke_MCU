/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "Options.h"

#define PMO_MCU_FOLLOW String("MCU follow")
#define PMO_GUI_FOLLOW String("GUI follow")
#define PMO_ALT_OPEN String("ALT SELECT")
#define PMO_LIMIT_FLOATING String("Limit float")

// MCU follow
#define PMOA_OFF String("off")
#define PMOA_SAME_TRACK String("same track")
#define PMOA_ALWAYS String("always")

// GUI follow
#define PMOA_IF_CHAIN_OPEN String("if chain open")
#define PMOA_OPEN_CHAIN String("open chain")
#define PMOA_OPEN_FLOATING String("open floating")

// ALT_OPEN
// PMOA_OPEN_CHAIN
#define PMOA_OPEN_CHAIN_CLOSE_FLOAT String("chain -float")
// PMOA_OPEN_FLOATING

// LIMIT_FLOATING
// PMOA_OFF
#define PMOA_ONLY_ONE_MCU String("only 1 MCU")
#define PMOA_ONLY_ONE_GLOBAL String("only selected")
#define PMOA_ONLY_CHAIN String("only chain")

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
