/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "CCSMode.h"
#include "csurf_mcu.h"
#include "tracks.h"

CCSMode::CCSMode(CCSManager *pManager) { m_pCCSManager = pManager; }

CCSMode::~CCSMode(void) {}

void CCSMode::activate() { updateEverything(); }

void CCSMode::updateRecLEDs() {
  for (int i = 1; i < 9; i++) {
    m_pCCSManager->setRecLED(this, i, false);
  }
}

void CCSMode::updateSelectLEDs() {
  for (int i = 1; i < 9; i++) {
    m_pCCSManager->setSelectLED(this, i, false);
  }
}

void CCSMode::updateSoloLEDs() {
  for (int i = 1; i < 9; i++) {
    m_pCCSManager->setSoloLED(this, i, false);
  }
}

void CCSMode::updateMuteLEDs() {
  for (int i = 1; i < 9; i++) {
    m_pCCSManager->setMuteLED(this, i, false);
  }
}

void CCSMode::updateFlipLED() { m_pCCSManager->setFlipLED(this, false); }

void CCSMode::updateGlobalViewLED() {
  m_pCCSManager->setGlobalViewLED(this, false);
}

void CCSMode::updateAssignmentDisplay() {
  m_pCCSManager->setAssignmentDisplay(this, "  ");
}

bool CCSMode::isModifierPressed(int modifier) {
  return m_pCCSManager->getMCU()->IsModifierPressed(modifier);
}

void CCSMode::updateEverything() {
  updateFaders();
  updateFlipLED();
  updateGlobalViewLED();
  updateMuteLEDs();
  updateRecLEDs();
  updateSelectLEDs();
  updateSoloLEDs();
  updateVPOTs();
  updateAssignmentDisplay();
  updateDisplay();
}

void CCSMode::updateFaders() {
  for (int i = 1; i < 9; i++) {
    m_pCCSManager->setFader(this, i, 0);
  }
}

void CCSMode::updateVPOTs() {
  for (int i = 1; i < 9; i++) {
    m_pCCSManager->getVPOT(i)->setMode(VPOT_LED::FROM_LEFT);
    m_pCCSManager->getVPOT(i)->setValue(0);
    m_pCCSManager->getVPOT(i)->setBottom(false);
  }
}

MediaTrack *CCSMode::selectedTrack() {
  return Tracks::instance()->getSelectedSingleTrack();
}

Display *CCSMode::getCurrentDisplay() {
		return m_pCCSManager->getDisplayHandler()->getDisplay();
}
