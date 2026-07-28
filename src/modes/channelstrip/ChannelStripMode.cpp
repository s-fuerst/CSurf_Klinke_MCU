/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Channel Strip Mode — skeleton (Steps A1/A2).
 *
 * Reachable via the TRACK assign button. Full parameter mapping, the on-screen
 * editor and persistence are added in later steps (see
 * ai-docs/channelstrip-mode-plan.md). For now the mode activates and shows a
 * placeholder on the display so the wiring can be verified end-to-end.
 */
#include "ChannelStripMode.h"

#include "csurf_mcu.h"
#include "Display.h"
#include "ChannelStripAccess.h"
#include "editor/ChannelStripComponent.h"

ChannelStripMode::ChannelStripMode(CCSManager *pManager)
    : CCSMode(pManager), m_pDisplay(NULL), m_pAccess(NULL),
      m_pEditor(NULL), m_pCurrentMaps(NULL), m_lastShiftState(false) {
  m_pDisplay = pManager->createDisplay(2);
  m_pAccess = new ChannelStripAccess(this);
}

int ChannelStripMode::numUnits() const {
  return m_pCCSManager ? m_pCCSManager->getMCU()->numUnits() : 1;
}

MediaTrack *ChannelStripMode::getSelectedTrack() { return selectedTrack(); }

void ChannelStripMode::trackChanged(MediaTrack *pTrack) {
  m_pAccess->trackChanged(pTrack);
  if (!pTrack) {
    m_pCurrentMaps = NULL;
    m_currentTrackGUID = String();
    return;
  }
  GUID *g = GetTrackGUID(pTrack);
  String guid = g ? GUID2String(g) : String();
  if (guid != m_currentTrackGUID || !m_pCurrentMaps) {
    m_currentTrackGUID = guid;
    // ensure this track has a map per unit (empty maps default-constructed)
    std::vector<ChannelStripMap> &v = m_mapsByTrack[guid];
    if ((int)v.size() < numUnits())
      v.resize(numUnits());
    m_pCurrentMaps = &v;
  }
}

ChannelStripMode::~ChannelStripMode() {
  safe_delete(m_pDisplay);
  safe_delete(m_pAccess);
}

void ChannelStripMode::activate() {
  trackChanged(selectedTrack());
  CCSMode::activate(); // -> updateEverything() -> updateDisplay()/updateVPOTs()
}

void ChannelStripMode::deactivate() {}

void ChannelStripMode::resolveChannel(int channel, int &unit, int &localCh) {
  // channel is 1-based global; unit 0-based; localCh 0-based 0..7
  if (channel > 0) {
    unit = (channel - 1) / 8;
    localCh = (channel - 1) % 8;
  } else {
    unit = 0;
    localCh = 0;
  }
}

int ChannelStripMode::slotFor(int localCh) {
  // slots 0..7 normal, 8..15 with Shift
  return localCh + (isModifierPressed(VK_SHIFT) ? 8 : 0);
}

ChannelStripMap *ChannelStripMode::getMapForUnit(int unit) {
  if (!m_pCurrentMaps || unit < 0 || unit >= (int)m_pCurrentMaps->size())
    return NULL;
  return &(*m_pCurrentMaps)[unit];
}

bool ChannelStripMode::vpotMoved(int channel, int numSteps) {
  int unit, localCh;
  resolveChannel(channel, unit, localCh);
  int slot = slotFor(localCh);
  ChannelStripMap *map = getMapForUnit(unit);
  MediaTrack *tr = selectedTrack();
  if (!map || !tr)
    return false;
  ChannelStripBinding *b = map->getSlot(slot);
  if (!b || !b->isAssigned())
    return false;
  int fxSlot = ChannelStripAccess::resolveBinding(tr, *b);
  if (fxSlot < 0)
    return false; // dangling / not on track (the "+" flow, Step D)
  ChannelStripAccess::nudgeParam(tr, fxSlot, b->getParamIndex(), numSteps);
  updateChannel(channel);
  return true;
}

bool ChannelStripMode::vpotPressed(int channel, bool pressed) {
  if (!pressed)
    return false;
  int unit, localCh;
  resolveChannel(channel, unit, localCh);
  int slot = slotFor(localCh);
  ChannelStripMap *map = getMapForUnit(unit);
  MediaTrack *tr = selectedTrack();
  if (!map || !tr)
    return false;
  ChannelStripBinding *b = map->getSlot(slot);
  if (!b || !b->isAssigned())
    return false;
  int fxSlot = ChannelStripAccess::resolveBinding(tr, *b);
  if (fxSlot < 0)
    return false;
  ChannelStripAccess::toggleParam(tr, fxSlot, b->getParamIndex());
  updateChannel(channel);
  return true;
}

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
    ChannelStripMap *map = getMapForUnit(unit);
    VPOT_LED *v = m_pCCSManager->getVPOT(ch);
    if (!v)
      continue;
    ChannelStripBinding *b =
        (map) ? map->getSlot(slotFor(localCh)) : NULL;
    int fxSlot = (b && b->isAssigned() && tr)
                     ? ChannelStripAccess::resolveBinding(tr, *b)
                     : -1;
    if (b && b->isAssigned() && fxSlot >= 0) {
      double norm = ChannelStripAccess::getParamValue(tr, fxSlot, b->getParamIndex());
      v->setMode(VPOT_LED::FROM_LEFT);
      v->setValue(1 + (int)(norm * 11.0 + 0.5));
    } else {
      v->setMode(VPOT_LED::OFF);
      v->setValue(0);
    }
  }
}

void ChannelStripMode::updateChannel(int globalChannel) {
  // refresh one channel's VPOT ring + display field
  updateVPOTs(); // VPOT_LED::setValue is a no-op unless changed (guarded)
  if (!m_pDisplay)
    return;
  int unit, localCh;
  resolveChannel(globalChannel, unit, localCh);
  ChannelStripMap *map = getMapForUnit(unit);
  ChannelStripBinding *b = (map) ? map->getSlot(slotFor(localCh)) : NULL;
  MediaTrack *tr = selectedTrack();
  String name = (b && b->isAssigned()) ? b->getShortName() : String();
  String value;
  if (b && b->isAssigned() && tr) {
    int fxSlot = ChannelStripAccess::resolveBinding(tr, *b);
    if (fxSlot >= 0) {
      double norm =
          ChannelStripAccess::getParamValue(tr, fxSlot, b->getParamIndex());
      value = String((int)(norm * 99.0 + 0.5));
    } else {
      value = "+"; // not on the track yet (Step D adds it)
    }
  }
  m_pDisplay->changeField(0, globalChannel, name.toRawUTF8());
  m_pDisplay->changeField(1, globalChannel, value.toRawUTF8());
}

void ChannelStripMode::bindingChanged() {
  // editor changed a binding -> refresh hardware for the active track
  updateEverything();
}

void ChannelStripMode::updateAssignmentDisplay() {
  m_pCCSManager->setAssignmentDisplay(this, "cs");
}

void ChannelStripMode::frameUpdate() {
  // Refresh rings for live value feedback (no-op unless a value changed) and
  // rebuild the display when Shift toggles (slots 1..8 <-> 9..16).
  updateVPOTs();
  bool shift = isModifierPressed(VK_SHIFT);
  if (shift != m_lastShiftState) {
    m_lastShiftState = shift;
    updateDisplay();
  }
}

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
