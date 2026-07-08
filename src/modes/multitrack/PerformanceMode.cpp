/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PerformanceMode.h"
#include "CCSManager.h"
#include "CCSMode.h"
#include "Display.h"
#include "DisplayHandler.h"
#include "csurf_mcu.h"

PerformanceMode::PerformanceMode(CCSManager *pManager) : CCSMode(pManager) {
  m_pDisplay = pManager->createDisplay(2);
  m_pDisplay->changeText(0, 0, "Performance Mode", 55);
}

PerformanceMode::~PerformanceMode(void) { safe_delete(m_pDisplay); }

void PerformanceMode::updateDisplay() {
  m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
}
