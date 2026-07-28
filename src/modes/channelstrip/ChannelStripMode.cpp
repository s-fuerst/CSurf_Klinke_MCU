/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripMode.h"

#include "csurf_mcu.h"
#include "Display.h"
#include "ChannelStripAccess.h"
#include "editor/ChannelStripComponent.h"
#include "Tracks.h"

ChannelStripMode::PerTrackAssignments::PerTrackAssignments() {
  for (int i = 0; i < MAX_SURFACE_UNITS; i++)
    stripIndexForUnit[i] = -1;
}

ChannelStripMode::ChannelStripMode(CCSManager *pManager)
    : CCSMode(pManager), m_pDisplay(NULL), m_pAccess(NULL), m_pEditor(NULL),
      m_lastShiftState(false) {
  m_pDisplay = pManager->createDisplay(2);
  m_pAccess = new ChannelStripAccess(this);
  for (int i = 0; i < kNumStrips; i++)
    m_strips[i].initEmpty();
}

ChannelStripMode::~ChannelStripMode() {
  safe_delete(m_pDisplay);
  safe_delete(m_pAccess);
}

int ChannelStripMode::numUnits() const {
  return m_pCCSManager ? m_pCCSManager->getMCU()->numUnits() : 1;
}

MediaTrack *ChannelStripMode::getSelectedTrack() { return selectedTrack(); }

// --- 16 global strips ---

ChannelStripMap *ChannelStripMode::getStrip(int index) {
  jassert(index >= 0 && index < kNumStrips);
  return &m_strips[index];
}

// --- per-unit assignment on the current track ---

int ChannelStripMode::getAssignedStripIndex(int unit) {
  if (m_currentTrackGUID.isEmpty() || unit < 0 || unit >= numUnits())
    return -1;
  auto it = m_assignments.find(m_currentTrackGUID);
  if (it == m_assignments.end())
    return -1;
  return it->second.stripIndexForUnit[unit];
}

void ChannelStripMode::setAssignedStripIndex(int unit, int stripIndex,
                                             bool notifyHardware) {
  if (m_currentTrackGUID.isEmpty() || unit < 0 || unit >= numUnits())
    return;
  if (stripIndex >= kNumStrips)
    stripIndex = -1;
  m_assignments[m_currentTrackGUID].stripIndexForUnit[unit] = stripIndex;
  if (notifyHardware)
    updateEverything();
}

ChannelStripMap *ChannelStripMode::getStripForUnit(int unit) {
  int idx = getAssignedStripIndex(unit);
  return (idx >= 0) ? &m_strips[idx] : NULL;
}

void ChannelStripMode::trackChanged(MediaTrack *pTrack) {
  m_pAccess->trackChanged(pTrack);
  if (!pTrack) {
    m_currentTrackGUID = String();
    return;
  }
  GUID *g = GetTrackGUID(pTrack);
  String guid = g ? GUID2String(g) : String();
  if (guid != m_currentTrackGUID) {
    m_currentTrackGUID = guid;
    auto it = m_assignments.find(guid);
    if (it == m_assignments.end()) {
      it = m_assignments.emplace(guid, PerTrackAssignments()).first;
      // default for testing: unit 0 → strip 0 (first row in the editor)
      it->second.stripIndexForUnit[0] = 0;
    }
  }
}

// --- CCSMode overrides ---

void ChannelStripMode::activate() {
  trackChanged(selectedTrack());
  CCSMode::activate();
}

void ChannelStripMode::deactivate() {}

void ChannelStripMode::resolveChannel(int channel, int &unit, int &localCh) {
  if (channel > 0) {
    unit = (channel - 1) / 8;
    localCh = (channel - 1) % 8;
  } else {
    unit = 0;
    localCh = 0;
  }
}

int ChannelStripMode::slotFor(int localCh) {
  return localCh + (isModifierPressed(VK_SHIFT) ? 8 : 0);
}

// --- hardware events ---

bool ChannelStripMode::vpotMoved(int channel, int numSteps) {
  int unit, localCh;
  resolveChannel(channel, unit, localCh);
  int vpot = slotFor(localCh);
  ChannelStripMap *strip = getStripForUnit(unit);
  MediaTrack *tr = selectedTrack();
  if (!strip || !tr)
    return false;
  int param = strip->getParamForVPOT(vpot);
  if (param < 0)
    return false;
  int fxSlot = ChannelStripAccess::resolveBinding(tr, *strip);
  if (fxSlot < 0)
    return false; // plugin not on the track ("+" flow, Step D)
  ChannelStripAccess::nudgeParam(tr, fxSlot, param, numSteps);
  updateChannel(channel);
  return true;
}

bool ChannelStripMode::vpotPressed(int channel, bool pressed) {
  if (!pressed)
    return false;
  int unit, localCh;
  resolveChannel(channel, unit, localCh);
  int vpot = slotFor(localCh);
  ChannelStripMap *strip = getStripForUnit(unit);
  MediaTrack *tr = selectedTrack();
  if (!strip || !tr)
    return false;
  int param = strip->getParamForVPOT(vpot);
  if (param < 0)
    return false;
  int fxSlot = ChannelStripAccess::resolveBinding(tr, *strip);
  if (fxSlot < 0)
    return false;
  ChannelStripAccess::toggleParam(tr, fxSlot, param);
  updateChannel(channel);
  return true;
}

// --- display / VPOT / assignment ---

void ChannelStripMode::updateDisplay() {
  if (!m_pDisplay)
    return;
  m_pCCSManager->switchToDisplay(this, m_pDisplay);
  int nStrips = numUnits() * 8;
  for (int ch = 1; ch <= nStrips; ch++)
    updateChannel(ch);
}

void ChannelStripMode::updateVPOTs() {
  MediaTrack *tr = selectedTrack();
  int nStrips = numUnits() * 8;
  for (int ch = 1; ch <= nStrips; ch++) {
    int unit, localCh;
    resolveChannel(ch, unit, localCh);
    ChannelStripMap *strip = getStripForUnit(unit);
    VPOT_LED *v = m_pCCSManager->getVPOT(ch);
    if (!v)
      continue;
    int vpot = slotFor(localCh);
    int param = (strip) ? strip->getParamForVPOT(vpot) : -1;
    int fxSlot = (param >= 0 && tr) ? ChannelStripAccess::resolveBinding(tr, *strip)
                                    : -1;
    if (param >= 0 && fxSlot >= 0) {
      double norm = ChannelStripAccess::getParamValue(tr, fxSlot, param);
      v->setMode(VPOT_LED::FROM_LEFT);
      v->setValue(1 + (int)(norm * 11.0 + 0.5));
    } else {
      v->setMode(VPOT_LED::OFF);
      v->setValue(0);
    }
  }
}

void ChannelStripMode::updateChannel(int globalChannel) {
  updateVPOTs();
  if (!m_pDisplay)
    return;
  int unit, localCh;
  resolveChannel(globalChannel, unit, localCh);
  ChannelStripMap *strip = getStripForUnit(unit);
  int vpot = slotFor(localCh);
  int param = (strip) ? strip->getParamForVPOT(vpot) : -1;
  MediaTrack *tr = selectedTrack();
  String name;
  String value;
  if (strip && strip->isAssigned()) {
    name = strip->getVPOTName(vpot);
    if (name.isEmpty())
      name = strip->getShortName();
    if (param >= 0 && tr) {
      int fxSlot = ChannelStripAccess::resolveBinding(tr, *strip);
      if (fxSlot >= 0) {
        double norm = ChannelStripAccess::getParamValue(tr, fxSlot, param);
        value = String((int)(norm * 99.0 + 0.5));
      } else {
        value = "+";
      }
    }
  }
  m_pDisplay->changeField(0, globalChannel, name.toRawUTF8());
  m_pDisplay->changeField(1, globalChannel, value.toRawUTF8());
}

void ChannelStripMode::updateAssignmentDisplay() {
  m_pCCSManager->setAssignmentDisplay(this, "cs");
}

void ChannelStripMode::frameUpdate() {
  updateVPOTs();
  bool shift = isModifierPressed(VK_SHIFT);
  if (shift != m_lastShiftState) {
    m_lastShiftState = shift;
    updateDisplay();
  }
}

void ChannelStripMode::bindingChanged() {
  updateEverything();
  if (m_pEditor)
    m_pEditor->updateEverything();
}

// --- editor lifecycle ---

Component **ChannelStripMode::createEditorComponent() {
  if (!m_pEditor)
    m_pEditor = new ChannelStripComponent(this);
  m_pEditor->updateEverything();
  return reinterpret_cast<Component **>(&m_pEditor);
}

void ChannelStripMode::deleteEditorComponent() { removeEditor(); }

void ChannelStripMode::removeEditor() {
  m_pCCSManager->closeEditorIfOpen(m_pEditor);
  safe_delete(m_pEditor);
}
