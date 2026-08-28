/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripMode.h"

#include "csurf_mcu.h"
#include "Display.h"
#include "ChannelStripAccess.h"
#include "editor/ChannelStripComponent.h"
#include "ChannelStripMeterBridge.h"
#include "Tracks.h"
#include "McuAssert.h"
#include "McuDebugLog.h"
#include "ProjectConfig.h"
#include <boost/bind.hpp>

using boost::placeholders::_1;
using boost::placeholders::_2;

#define CSM_TAG_ROOT String("CHANNELSTRIPS")
#define CSM_ATT_TRACK String("track")
#define CSM_ATT_UNIT String("unit")
#define CSM_ATT_STRIP String("strip")

ChannelStripMode::PerTrackAssignments::PerTrackAssignments() {
  for (int i = 0; i < MAX_SURFACE_UNITS; i++)
    stripIndexForUnit[i] = -1;
}

ChannelStripMode::ChannelStripMode(CCSManager *pManager)
    : MultiTrackMode(pManager), m_pAccess(NULL), m_pEditor(NULL),
      m_selectionMode(false), m_lastShiftState(false),
      m_lastAltState(false), m_projectChangedConnectionId(-1) {
  m_pAccess = new ChannelStripAccess(this);
  // Replace MultiTrackMode's meter bridge with one that does NOT draw meter
  // bars on the LCD — row 1 carries our strip-name / parameter text.
  safe_delete(m_pMeterBridge);
  m_pMeterBridge = new ChannelStripMeterBridge();
  for (int i = 0; i < kNumStrips; i++)
    m_strips[i].initEmpty();
  // Load the 16 global strips from the user file (survives REAPER restarts).
  loadStripsFromFile();
  // Persist per-(track, unit) assignments inside the Reaper project.
  m_projectChangedConnectionId =
      ProjectConfig::instance()->connect2ProjectChangeSignal(
          boost::bind(&ChannelStripMode::projectChanged, this, _1, _2));
}

ChannelStripMode::~ChannelStripMode() {
  if (m_projectChangedConnectionId >= 0)
    ProjectConfig::instance()->disconnectProjectChangeSignal(
        m_projectChangedConnectionId);
  safe_delete(m_pAccess);
}

// --- 16 global strips ---

ChannelStripMap *ChannelStripMode::getStrip(int index) {
  jassert(index >= 0 && index < kNumStrips);
  return &m_strips[index];
}

// --- selected track ---

MediaTrack *ChannelStripMode::getSelectedTrack() {
  return Tracks::instance()->getSelectedSingleTrack();
}

// --- per-(track, unit) assignment ---

int ChannelStripMode::getAssignedStripIndex(MediaTrack *tr, int unit) {
  if (!tr || unit < 0 || unit >= MAX_SURFACE_UNITS)
    return -1;
  GUID *g = GetTrackGUID(tr);
  if (!g) return -1;
  String guid = GUID2String(g);
  auto it = m_assignments.find(guid);
  if (it == m_assignments.end())
    return -1;
  return it->second.stripIndexForUnit[unit];
}

void ChannelStripMode::setAssignedStripIndex(MediaTrack *tr, int unit,
                                             int stripIndex,
                                             bool notifyHardware) {
  if (!tr || unit < 0 || unit >= MAX_SURFACE_UNITS)
    return;
  if (stripIndex >= kNumStrips)
    stripIndex = -1;
  GUID *g = GetTrackGUID(tr);
  if (!g) return;
  String guid = GUID2String(g);
  if (m_assignments.find(guid) == m_assignments.end())
    m_assignments.emplace(guid, PerTrackAssignments());
  m_assignments[guid].stripIndexForUnit[unit] = stripIndex;
  if (notifyHardware)
    updateEverything();
}

ChannelStripMap *ChannelStripMode::getStripForChannel(int globalChannel) {
  if (globalChannel <= 0)
    return NULL;
  MediaTrack *tr = getSelectedTrack();
  if (!tr)
    return NULL;
  int unit = (globalChannel - 1) / 8;
  int idx = getAssignedStripIndex(tr, unit);
  return (idx >= 0) ? &m_strips[idx] : NULL;
}

// --- CCSMode / MultiTrackMode overrides ---

void ChannelStripMode::activate() {
  // MultiTrackMode::activate() clears the display, switches to it, and
  // handles MultiDisplay routing.
  MultiTrackMode::activate();
}

int ChannelStripMode::slotFor(int localCh) {
  return localCh + (isModifierPressed(VK_SHIFT) ? 8 : 0);
}

// --- hardware events ---

bool ChannelStripMode::vpotMoved(int channel, int numSteps) {
  MediaTrack *tr = getSelectedTrack();
  if (!tr)
    return false;

  int localCh = (channel - 1) % 8;
  int unit = (channel - 1) / 8;
  int stripIdx = getAssignedStripIndex(tr, unit);

  // No strip assigned, or in selection mode: turns do nothing (only press
  // picks). Selection is via VPOT press, not turn.
  if (stripIdx < 0 || m_selectionMode)
    return false;

  ChannelStripMap *strip = &m_strips[stripIdx];
  if (!strip->isAssigned())
    return false;
  int vpot = slotFor(localCh);
  int param = strip->getParamForVPOT(vpot);
  if (param < 0)
    return false;

  int fxSlot = m_pAccess->resolveSlot(tr, stripIdx, *strip);
  if (fxSlot < 0)
    return false; // plugin missing — needs press (+), not turn

  ChannelStripAccess::nudgeParam(tr, fxSlot, param, numSteps);
  m_lastVPOTChangeTime[channel - 1] = Time::getCurrentTime();
  updateEverything();
  return true;
}

bool ChannelStripMode::vpotPressed(int channel, bool pressed) {
  if (!pressed)
    return false;
  MediaTrack *tr = getSelectedTrack();
  if (!tr)
    return false;

  int localCh = (channel - 1) % 8;
  int unit = (channel - 1) / 8;
  int vpot = slotFor(localCh); // 0..7 normal, 8..15 with Shift

  // ALT commands (Step G), unshifted VPOT range only. NOTE: `vpot` here is
  // 0-based — hardware VPOT N is vpot N-1, so the commands live at vpot
  // 0 (VPOT-1, open floating window), 1 (VPOT-2, open chain), 6 (VPOT-7)
  // and 7 (VPOT-8, move FX up/down). They are only active on units whose
  // strip is assigned AND whose plugin is present on the selected track —
  // only then is the target FX known. Other VPOTs fall through to the
  // normal behaviour below.
  if (isModifierPressed(VK_ALT) && vpot < 8 &&
      (vpot == 0 || vpot == 1 || vpot == 6 || vpot == 7)) {
    int stripIdx = getAssignedStripIndex(tr, unit);
    int fxSlot = (stripIdx >= 0 && m_strips[stripIdx].isAssigned())
        ? m_pAccess->resolveSlot(tr, stripIdx, m_strips[stripIdx])
        : -1;
    if (fxSlot < 0)
      return false; // no active strip on this unit: command inactive
    MCU_LOG("CSM ALT cmd ch=%d unit=%d vpot=%d fxSlot=%d", channel, unit,
            vpot, fxSlot);
    if (vpot == 0 || vpot == 1) {
      openFxWindow(tr, fxSlot, /*floating=*/vpot == 0);
      return true;
    }
    if (moveFx(tr, fxSlot, (vpot == 6) ? -1 : +1))
      return true;
    return false; // move refused (chain edge): do nothing else
  }

  int stripIdx = getAssignedStripIndex(tr, unit);
  // A strip whose plugin is missing on the selected track behaves like
  // "no strip": the unit shows the picker and a press (re)picks a strip.
  int fxSlot = (stripIdx >= 0)
      ? m_pAccess->resolveSlot(tr, stripIdx, m_strips[stripIdx])
      : -1;
  MCU_LOG("CSM vpotPressed ch=%d unit=%d vpot=%d stripIdx=%d selMode=%d fxSlot=%d",
          channel, unit, vpot, stripIdx, (int)m_selectionMode, fxSlot);

  // Pick / re-pick: a unit without a strip, OR any unit while selection mode
  // (TRACK held) is on, OR the assigned strip's plugin is missing. VPOT
  // position selects which of the 16 global strips to assign to this unit
  // for the selected track.
  if (stripIdx < 0 || m_selectionMode || fxSlot < 0) {
    if (vpot >= kNumStrips)
      return false;
    ChannelStripMap *candidate = &m_strips[vpot];
    if (!candidate->isAssigned())
      return false; // empty strip slot — nothing to assign
    setAssignedStripIndex(tr, unit, vpot, false);
    // Auto-add the plugin to the selected track if it is not present yet.
    if (m_pAccess->resolveSlot(tr, vpot, *candidate) < 0)
      m_pAccess->addPlugin(tr, vpot, *candidate);
    updateEverything();
    return true;
  }

  // Active state: toggle the parameter 0/1.
  ChannelStripMap *strip = &m_strips[stripIdx];
  if (!strip->isAssigned())
    return false;
  int param = strip->getParamForVPOT(vpot);
  if (param < 0)
    return false;

  ChannelStripAccess::toggleParam(tr, fxSlot, param);
  m_lastVPOTChangeTime[channel - 1] = Time::getCurrentTime();
  updateEverything();
  return true;
}

// --- display / VPOT / assignment ---

void ChannelStripMode::updateDisplay() {
  // MultiTrackMode writes track names to row 0. Then we overlay row 1
  // with our parameter names/values (or the strip-name selection list).
  MultiTrackMode::updateDisplay();
  m_pCCSManager->switchToDisplay(this, m_pDisplay);

  // Row 1 is fully ours. Clear the ENTIRE line (all units) first. This
  // must be a full-line clear, NOT a per-field one: row-1 fields are 7
  // columns wide ((field-1)*7) but changeField only writes pad=6 columns,
  // so the 7th (separator) column of every field is never touched by
  // field writes. The centered "select a single track" hint covers those
  // separator columns, and only a full-line clear removes those leftovers.
  m_pDisplay->changeTextFullLine(1, "");

  MediaTrack *tr = getSelectedTrack();
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();

  if (!tr) {
    // No single selected track: show the hint on row 1 of every unit.
    m_pDisplay->changeTextFullLine(1, "You must select a single track.", true);
    return;
  }

  for (int ch = 1; ch <= nStrips; ch++)
    updateChannel(ch);
}

void ChannelStripMode::updateVPOTs() {
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  MediaTrack *tr = getSelectedTrack();
  for (int ch = 1; ch <= nStrips; ch++) {
    VPOT_LED *v = m_pCCSManager->getVPOT(ch);
    if (!v) continue;

    if (!tr) {
      v->setMode(VPOT_LED::OFF);
      v->setValue(0);
      continue;
    }

    int localCh = (ch - 1) % 8;
    int unit = (ch - 1) / 8;
    int vpot = slotFor(localCh);
    int stripIdx = getAssignedStripIndex(tr, unit);

    // Selection list (no strip, or selection mode): no LED ring — the names
    // are the selection surface, pressing picks.
    if (stripIdx < 0 || m_selectionMode) {
      v->setMode(VPOT_LED::OFF);
      v->setValue(0);
      continue;
    }

    ChannelStripMap *strip = &m_strips[stripIdx];
    int param = strip->isAssigned() ? strip->getParamForVPOT(vpot) : -1;
    int fxSlot = (param >= 0) ? m_pAccess->resolveSlot(tr, stripIdx, *strip) : -1;
    if (param >= 0 && fxSlot >= 0) {
      double norm = ChannelStripAccess::getParamValue(tr, fxSlot, param);
      v->setMode(VPOT_LED::FROM_LEFT);
      v->setValue(1 + (int)(norm * 10.0 + 0.5));
    } else {
      v->setMode(VPOT_LED::OFF);
      v->setValue(0);
    }
  }
}

void ChannelStripMode::updateChannel(int globalChannel) {
  if (!m_pDisplay)
    return;
  MediaTrack *tr = getSelectedTrack();
  if (!tr)
    return; // hint handled in updateDisplay()

  int localCh = (globalChannel - 1) % 8;
  int unit = (globalChannel - 1) / 8;
  int vpot = slotFor(localCh);
  int stripIdx = getAssignedStripIndex(tr, unit);
  ChannelStripMap *strip = (stripIdx >= 0) ? &m_strips[stripIdx] : NULL;
  int fxSlot = (strip && strip->isAssigned())
      ? m_pAccess->resolveSlot(tr, stripIdx, *strip)
      : -1;

  // ALT held: per-unit command legend on row 1, directly in the VPOT fields
  // (6 chars each). Only units with an ACTIVE strip (assigned + plugin
  // present) get labels — only there is the target FX known; the other
  // fields (and all fields of inactive units) stay empty.
  if (isModifierPressed(VK_ALT)) {
    String label;
    if (fxSlot >= 0) {
      if (localCh == 0) // hardware VPOT 1
        label = "Float";
      else if (localCh == 1) // hardware VPOT 2
        label = "Chain";
      else if (localCh == 6) // hardware VPOT 7
        label = "FXup";
      else if (localCh == 7) // hardware VPOT 8
        label = "FXdown";
    }
    m_pDisplay->changeField(1, globalChannel, label.toRawUTF8());
    return;
  }

  String text;

  // Pick / re-pick: no strip assigned, selection mode (TRACK held), or the
  // assigned strip's plugin is missing on the selected track. Show the 16
  // global strip names so the user can choose; a leading "+" marks strips
  // whose plugin is not present on the track yet (it will be auto-added
  // when the strip is picked).
  if (stripIdx < 0 || m_selectionMode || fxSlot < 0) {
    if (vpot < kNumStrips) {
      ChannelStripMap *candidate = &m_strips[vpot];
      if (candidate->isAssigned()) {
        text = candidate->getShortName();
        if (text.isEmpty())
          text = candidate->getFxIdent();
        if (m_pAccess->resolveSlot(tr, vpot, *candidate) < 0)
          text = "+" + text;
      }
    }
    m_pDisplay->changeField(1, globalChannel, text.toRawUTF8());
    return;
  }

  // Active state for this unit's strip.
  int param = strip->getParamForVPOT(vpot);
  if (param < 0) {
    // No parameter bound to this VPOT position: show nothing (not the
    // plugin name).
    m_pDisplay->changeField(1, globalChannel, "");
    return;
  }

  // Plugin present: show formatted value shortly after a turn, else the
  // per-VPOT display name (idle).
  bool changing = (Time::getCurrentTime() - m_lastVPOTChangeTime[globalChannel - 1])
                      .inMilliseconds() < 1000;
  if (changing)
    text = ChannelStripAccess::getFormattedParamValue(tr, fxSlot, param);
  if (text.isEmpty()) {
    text = strip->getVPOTName(vpot);
    if (text.isEmpty())
      text = strip->getShortName();
  }
  m_pDisplay->changeField(1, globalChannel, text.toRawUTF8());
}

void ChannelStripMode::updateAssignmentDisplay() {
  // Show "cs" so the user knows they are in Channel Strip mode.
  m_pCCSManager->setAssignmentDisplay(this, "cs");
}

void ChannelStripMode::frameUpdate() {
  // MultiTrackMode updates all LEDs, faders, meters, and clamps the offset.
  MultiTrackMode::frameUpdate();
  // Refresh display when Shift state changes (1-8 <-> Shift 1-8).
  bool shift = isModifierPressed(VK_SHIFT);
  if (shift != m_lastShiftState) {
    m_lastShiftState = shift;
    updateDisplay();
  }
  // Refresh display when ALT state changes (command legend on row 1).
  bool alt = isModifierPressed(VK_ALT);
  if (alt != m_lastAltState) {
    m_lastAltState = alt;
    updateDisplay();
  }
}

// --- ALT commands (Step G) ---

void ChannelStripMode::openFxWindow(MediaTrack *tr, int fxSlot,
                                    bool floating) {
  // ALT+VPOT-1 (floating) / ALT+VPOT-2 (chain) — deliberate: the PlugMode
  // window settings are IGNORED. Both TOGGLE: if the window/chain for this
  // FX is already open it is closed, else it is opened. showflag 3/2 =
  // show/close floating window, 1/0 = show/close chain.
  int chainVis = TrackFX_GetChainVisible(tr);
  HWND floatHwnd = TrackFX_GetFloatingWindow(tr, fxSlot);
  bool action;
  if (floating)
    action = (floatHwnd == NULL) ? 3 : 2; // open, or close if already open
  else
    action = (chainVis == -1) ? 1 : 0;    // open, or close if already open
  MCU_LOG("CSM openFxWindow slot=%d floating=%d chainVis=%d floatHwnd=%p"
          " action=%d", fxSlot, (int)floating, chainVis, (void *)floatHwnd,
          action);
  TrackFX_Show(tr, fxSlot, action);
  updateEverything();
}

bool ChannelStripMode::moveFx(MediaTrack *tr, int fxSlot, int dir) {
  int n = TrackFX_GetCount(tr);
  int newSlot = fxSlot + dir;
  if (fxSlot < 0 || newSlot < 0 || newSlot >= n)
    return false; // would move out of the chain
  MCU_LOG("CSM moveFx slot=%d dir=%d new=%d chain=%d", fxSlot, dir, newSlot,
          n);
  TrackFX_CopyToTrack(tr, fxSlot, tr, newSlot, true);
  // The slot cache now holds the old index for this track's strips.
  m_pAccess->invalidateTrack(tr);
  updateEverything();
  return true;
}

void ChannelStripMode::bindingChanged() {
  // A global strip changed: the slot cache may now point at the wrong plugin
  // type, and the display/LEDs must refresh.
  m_pAccess->invalidateAll();
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
  // The main editor (BindingTable) edits the per-slot header
  // (fxIdent/Abbrev/InsPos) — persist it now.
  saveStripsToFile();
}

// ===========================================================================
//  Persistence
// ===========================================================================

File ChannelStripMode::getStripsDir() {
#ifdef _WIN32
  File dir = File::getSpecialLocation(File::userDocumentsDirectory)
                     .getFullPathName() +
             String("\\Reaper\\MCU\\ChannelStripMaps\\");
#else
  File dir = File::getSpecialLocation(File::userHomeDirectory)
                     .getFullPathName() +
             String("/.config/REAPER/MCU/ChannelStripMaps/");
#endif
  if (!dir.exists())
    dir.createDirectory();
  return dir;
}

File ChannelStripMode::getGlobalFile() {
  return getStripsDir().getChildFile("channelstrips.xml");
}

void ChannelStripMode::loadStripsFromFile() {
  // One file holds everything: per assigned slot a <STRIP nr=.. fxident=..
  // name=.. inspos=..> with its <VPOT> children.
  File gf = getGlobalFile();
  if (!gf.existsAsFile())
    return;
  XmlDocument doc(gf);
  if (auto root = doc.getDocumentElement()) {
    forEachXmlChildElementWithTagName(*root, pStrip, "STRIP") {
      int nr = pStrip->getIntAttribute(CSB_ATT_NR, 0);
      if (nr >= 1 && nr <= kNumStrips)
        m_strips[nr - 1].readFromXml(pStrip);
    }
  }
}

void ChannelStripMode::saveStripsToFile() {
  XmlElement *root = new XmlElement(CSM_TAG_ROOT);
  for (int i = 0; i < kNumStrips; i++) {
    if (m_strips[i].isAssigned())
      m_strips[i].writeToXml(root, i + 1);
  }
  if (!root->writeToFile(getGlobalFile(), String()))
    MCU_LOG("CSM saveStripsToFile FAILED");
  delete root;
}

void ChannelStripMode::projectChanged(XmlElement *pRootNode,
                                      ProjectConfig::EAction action) {
  switch (action) {
  case ProjectConfig::WRITE: {
    XmlElement *node = new XmlElement(String("CHANNELSTRIP_ASSIGNMENTS"));
    for (const auto &kv : m_assignments) {
      const String &guid = kv.first;
      for (int u = 0; u < MAX_SURFACE_UNITS; u++) {
        int s = kv.second.stripIndexForUnit[u];
        if (s >= 0) {
          XmlElement *a = new XmlElement(String("ASSIGN"));
          a->setAttribute(CSM_ATT_TRACK, guid);
          a->setAttribute(CSM_ATT_UNIT, u);
          a->setAttribute(CSM_ATT_STRIP, s);
          node->addChildElement(a);
        }
      }
    }
    pRootNode->addChildElement(node);
    break;
  }
  case ProjectConfig::READ: {
    m_assignments.clear();
    m_pAccess->invalidateAll();
    XmlElement *node =
        pRootNode ? pRootNode->getChildByName(String("CHANNELSTRIP_ASSIGNMENTS"))
                  : nullptr;
    if (node) {
      forEachXmlChildElementWithTagName(*node, a, String("ASSIGN")) {
        String guid = a->getStringAttribute(CSM_ATT_TRACK);
        int u = a->getIntAttribute(CSM_ATT_UNIT, -1);
        int s = a->getIntAttribute(CSM_ATT_STRIP, -1);
        if (guid.isNotEmpty() && u >= 0 && u < MAX_SURFACE_UNITS &&
            s >= 0 && s < kNumStrips) {
          if (m_assignments.find(guid) == m_assignments.end())
            m_assignments.emplace(guid, PerTrackAssignments());
          m_assignments[guid].stripIndexForUnit[u] = s;
        }
      }
    }
    break;
  }
  case ProjectConfig::FREE:
    m_assignments.clear();
    m_pAccess->invalidateAll();
    break;
  }
}
