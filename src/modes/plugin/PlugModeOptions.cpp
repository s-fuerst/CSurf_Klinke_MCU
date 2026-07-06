/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PlugModeOptions.h"

PlugModeOptions::PlugModeOptions(DisplayHandler *pDH)
    : Options(pDH), m_bMCUFollowModified(false), m_iLastMCUFollowOption(-1) {
  addOption(PMO_MCU_FOLLOW);
  addAttribute(PMO_MCU_FOLLOW, PMOA_OFF);
  addAttribute(PMO_MCU_FOLLOW, PMOA_SAME_TRACK, true);
  addAttribute(PMO_MCU_FOLLOW,
               PMOA_ALWAYS); // if ALWAYS change the place,
                             // checkAndModifyOptions must be adjusted too

  addOption(PMO_GUI_FOLLOW);
  addAttribute(PMO_GUI_FOLLOW, PMOA_OFF);
  addAttribute(PMO_GUI_FOLLOW, PMOA_IF_CHAIN_OPEN, true);
  addAttribute(PMO_GUI_FOLLOW, PMOA_OPEN_CHAIN);
  addAttribute(PMO_GUI_FOLLOW, PMOA_OPEN_FLOATING);

  addOption(PMO_ALT_OPEN);
  addAttribute(PMO_ALT_OPEN, PMOA_OPEN_CHAIN);
  addAttribute(PMO_ALT_OPEN, PMOA_OPEN_CHAIN_CLOSE_FLOAT);
  addAttribute(PMO_ALT_OPEN, PMOA_OPEN_FLOATING, true);

  addOption(PMO_LIMIT_FLOATING);
  addAttribute(PMO_LIMIT_FLOATING, PMOA_OFF, true);
  addAttribute(PMO_LIMIT_FLOATING, PMOA_ONLY_ONE_MCU);
  addAttribute(PMO_LIMIT_FLOATING, PMOA_ONLY_ONE_GLOBAL);
  addAttribute(PMO_LIMIT_FLOATING, PMOA_ONLY_CHAIN);

  readConfigFile();
}

PlugModeOptions::~PlugModeOptions(void) { writeConfigFile(); }

String PlugModeOptions::getConfigFileName() {
  return String("PlugModeOptions1");
}

void PlugModeOptions::checkAndModifyOptions() {
  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_ONE_GLOBAL)) {
    setOptionTo(PMO_MCU_FOLLOW, 2 /*PMOA_ALWAYS*/);
    m_bMCUFollowModified = true;
  } else {
    if (m_bMCUFollowModified) {
      m_bMCUFollowModified = false;
      setOptionTo(PMO_MCU_FOLLOW, m_iLastMCUFollowOption);
    } else {
      m_iLastMCUFollowOption = getSelectedOption(PMO_MCU_FOLLOW);
    }
  }
}
