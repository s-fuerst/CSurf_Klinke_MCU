/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PlugMode2ndOptions.h"
#include "SurfaceConfig.h"

PlugMode2ndOptions::PlugMode2ndOptions(DisplayHandler *pDH) : Options(pDH) {
  addOption(PMO2_MODE_CHANGE);
  addAttribute(PMO2_MODE_CHANGE, PMO2A_NOTHING, true);
  addAttribute(PMO2_MODE_CHANGE, PMO2A_OPEN_CLOSE);
  addAttribute(PMO2_MODE_CHANGE, PMO2A_OPEN_CLOSE_MIXER);

  //  addOption(PMO2_KEEP_IN_FRONT);
  //  addAttribute(PMO2_KEEP_IN_FRONT, PMO2A_OFF, true);
  //  addAttribute(PMO2_KEEP_IN_FRONT, PMO2A_ON);
  //  addAttribute(PMO2_KEEP_IN_FRONT, PMO2A_MOVE);
  addOption(PMO2_MOVE);
  addAttribute(PMO2_MOVE, PMO2A_OFF, true);
  addAttribute(PMO2_MOVE, PMO2A_ON);

	addOption(PMO2_SHOW_DETAILS);
	addAttribute(PMO2_SHOW_DETAILS, PMO2A_OFF);
	addAttribute(PMO2_SHOW_DETAILS, PMO2A_ON, true);

	// multi-valued follow-change target.
	// OFF (default) + "Unit 1" .. "Unit N" (N = MAX_SURFACE_UNITS).
	// The cyclic VPOT selection reaches all values; units beyond numUnits()
	// are ignored at runtime by PlugMode::followChangeUnit().
	addOption(PMO2_FOLLOW_CHANGE);
	addAttribute(PMO2_FOLLOW_CHANGE, PMO2A_OFF, true);
	for (int u = 0; u < MAX_SURFACE_UNITS; u++)
		addAttribute(PMO2_FOLLOW_CHANGE, "Unit " + String(u + 1));
	
  readConfigFile();
}

PlugMode2ndOptions::~PlugMode2ndOptions(void) { writeConfigFile(); }

int PlugMode2ndOptions::followChangeUnit(int numUnits) {
	String sel = getSelectedOptionAsString(PMO2_FOLLOW_CHANGE);
	if (sel == PMO2A_OFF || !sel.startsWith("Unit "))
		return -1;
	int unit = sel.substring(5).getIntValue() - 1; // "Unit N" -> N-1
	if (unit < 0 || unit >= numUnits)
		return -1;
	return unit;
}

String PlugMode2ndOptions::getConfigFileName() {
  return String("PlugModeOptions2");
}
