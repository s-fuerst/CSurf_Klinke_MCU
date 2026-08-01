/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

//#include "SnM/stdafx.h"
#include "boost/foreach.hpp"
#include "PlugAccess.h"
#include "PlugMode.h"
#include "PlugMap.h"
#include "PluginWatcher.h"
#include "PlugModeComponent.h"
#include "csurf_mcu.h"
#include "boost/bind.hpp"
#include "PlugMapManager.h"
#include "PlugWindowManager.h"
#include "std_helper.h"
#include "Tracks.h"
#include <memory>
//#include "SnM/SnM_Actions.h"

//#define TRACK_CHANGE_TRACK_UNKNOWN -1
//#define TRACK_CHANGE_TRACK_NOT_CHANGED -2

PlugAccess::PlugAccess(PlugMode *pMode)
    : m_pMode(pMode), m_pPlugTrack(NULL), m_iSlot(-1),
      m_pMapManager(NULL), m_plugName(String()),
      m_GUIDplugTrack(GUID_NOT_ACTIVE) {
  // init per-unit state to bank 0 / page 0 for all units
  m_selectedBankPerUnit.assign(0);
  for (int u = 0; u < MAX_SURFACE_UNITS; u++)
    m_selectedPagePerUnit[u].assign(0);
  m_unitEmpty.assign(false);

  m_pMapManager = new PlugMapManager(pMode);
  m_pWindowManager = new PlugWindowManager(pMode);

  Tracks::instance()->connect2TrackRemovedSignal(
      boost::bind(&PlugAccess::trackRemoved, this, _1));

  m_pPlugWatcher = new PluginWatcher(pMode->getCCSManager()->getMCU());
  m_nameChangedConnectionId = m_pPlugWatcher->connect2NameChanged(
      boost::bind(&PlugAccess::watchedNameParameterChanged, this, _1, _2, _3));

  m_projectChangedConnectionId =
      ProjectConfig::instance()->connect2ProjectChangeSignal(
          boost::bind(&PlugAccess::projectChanged, this, _1, _2));
}

PlugAccess::~PlugAccess(void) {
  ProjectConfig::instance()->disconnectProjectChangeSignal(
      m_projectChangedConnectionId);

  m_pPlugWatcher->disconnectNameChange(m_nameChangedConnectionId);
  safe_delete(m_pPlugWatcher);
  safe_delete(m_pMapManager);
  safe_delete(m_pWindowManager);
}

void PlugAccess::trackChanged(MediaTrack *pMediaTrack) {
  if (pMediaTrack == m_pPlugTrack)
    return;

  m_track2Slot.erase((unsigned long)m_pPlugTrack);
  m_track2Slot.insert(
      std::pair<unsigned long, int>((unsigned long)m_pPlugTrack, m_iSlot));

  if (!pMediaTrack) {
    accessPlugin(NULL, -1);
    return;
  }

  tTrack2Plug::iterator iterT2P = m_track2Slot.find((unsigned long)pMediaTrack);
  if (iterT2P != m_track2Slot.end()) {
    accessPlugin(pMediaTrack, (*iterT2P).second);
  } else {
    // No prior mapping: prefer whichever slot has a floating window open,
    // otherwise default to slot 0.  This makes "open a plugin then switch
    // to PlugMode" Just Work without requiring the user to manually select
    // a slot via the MCU SELECT buttons.
    int numFX = TrackFX_GetCount(pMediaTrack);
    if (numFX > 0) {
      int slotToUse = 0;
      for (int i = 0; i < numFX; i++) {
        if (TrackFX_GetFloatingWindow(pMediaTrack, i)) {
          slotToUse = i;
          break;
        }
      }
      accessPlugin(pMediaTrack, slotToUse);
    } else {
      accessPlugin(NULL, -1);
    }
  }
}

void PlugAccess::accessPlugin(MediaTrack *pMediaTrack, int iSlot,
                              bool changeTriggeredFromGUI,
                              bool changeTriggeredFromProjectChange) {
  if (changeTriggeredFromGUI) {
    if (pMediaTrack == m_pPlugTrack && iSlot == m_iSlot)
      return;
  }

  storeActualSlotState();

  m_pPlugTrack = pMediaTrack;
  if (pMediaTrack != NULL) {
    m_GUIDplugTrack = *(CSurf_MCU::GUIDfromTrack(m_pPlugTrack));
  } else {
    m_GUIDplugTrack = GUID_NOT_ACTIVE;
  }
  m_iSlot = iSlot;

  // reset ALL units to bank 0 / page 0
  m_selectedBankPerUnit.assign(0);
  for (int u = 0; u < MAX_SURFACE_UNITS; u++)
    m_selectedPagePerUnit[u].assign(0);
  m_unitEmpty.assign(false);

  if (changeTriggeredFromProjectChange || !plugExist()) {
    m_pMapManager->deselectMap();
    m_pMode->updateEverything();
    m_pPlugWatcher->setPlugin(NULL, -1);
    m_iSlot = -1;
    return;
  }

  m_plugName = getPlugNameLong();
  m_pMapManager->loadMapForPlug(m_pPlugTrack, m_iSlot);

  // restore stored per-unit state (or default page-spread for N>1)
  tSlotStatesMap::iterator iterStoredStates = m_knownSlotStates.find(
      tSlotLocation(GUID2String(CSurf_MCU::GUIDfromTrack(pMediaTrack)), iSlot));
  bool restoredFromStored = false;
  if (iterStoredStates != m_knownSlotStates.end()) {
    tSlotState storedState = (*iterStoredStates).second;
    String storedPlugName = storedState.get<0>();
    if (storedPlugName.equalsIgnoreCase(getPlugNameLong())) {
      // restore per-unit arrays
      m_selectedBankPerUnit = storedState.get<1>();
      m_selectedPagePerUnit = storedState.get<2>();
      m_unitEmpty = storedState.get<3>();
      restoredFromStored = true;
    }
  }

  if (!restoredFromStored) {
    // Default page spread across used pages. Unit u gets the page at
    // used-page offset u; units beyond the number of used pages stay empty
    // (no page) so no two units ever display the same page. All units on
    // bank 0. The used-page sequence lives on the RESOLVED bank (bank 0
    // may `refer` to another bank), mirroring the transport/cascade math.
    int nUnits = m_pMode->getCCSManager()->getMCU()->numUnits();
    int resolved0 = resolveBankReference(0);
    int count = usedPageCount(resolved0);
    for (int u = 0; u < nUnits; u++) {
      m_selectedBankPerUnit[u] = 0;
      if (u < count)
        m_selectedPagePerUnit[u][0] = pageAtUsedOffset(resolved0, u);
      else
        m_unitEmpty[u] = true;
    }
  } else {
    // Legacy/foreign states may contain duplicate (bank, page) assignments;
    // normalize them so every page is shown at most once.
    enforceUniquePages(-1);
  }

  m_pPlugWatcher->setPlugin(pMediaTrack, iSlot);
  m_pMode->plugChanged();

  if (!changeTriggeredFromGUI)
    m_pWindowManager->switchedTo(pMediaTrack, iSlot);
  else if (m_pMode->isFollowTrack()) {
    m_pMode->getCCSManager()->getMCU()->UnselectAllTracks();
    CSurf_OnSelectedChange(pMediaTrack, 1);
  }

  m_pMode->updateEverything();
}

String PlugAccess::getPlugName(bool shortName, MediaTrack *pMediaTrack,
                               int slot) {
  if (!plugExist()) {
    return String();
  }

  char paramName[80] = {};
  bool valid = TrackFX_GetFXName(pMediaTrack, slot, paramName, 79);
  if (!valid) {
    return String();
  }

  if (shortName) {
    return m_pMode->shortPlugName(paramName);
  }

  return m_pMode->longPlugName(paramName);
}

String PlugAccess::getFullPlugName(MediaTrack *pMediaTrack, int slot) {
  if (!plugExist()) {
    return String();
  }

  char paramName[80] = {};
  bool valid = TrackFX_GetFXName(pMediaTrack, slot, paramName, 79);
  if (!valid) {
    return String();
  }

  return String(paramName);
}

void PlugAccess::createDefaultMap() {
  int numParamsExist = getNumParams();
  int numParamsMapped = 0;

  char paramName[80] = {};

  ElementDesc desc(this, PlugAccess::ElementDesc::FADER, 0);
  for (desc.m_bank = 0; desc.m_bank < 8; desc.m_bank++) {
    getMap()
        ->getBank(desc.m_bank)
        ->setNameLong(String::formatted(String("Bank %d"), desc.m_bank + 1));
    getMap()
        ->getBank(desc.m_bank)
        ->setNameShort(String::formatted(String("Bank %d"), desc.m_bank + 1));
    for (desc.m_page = 0; desc.m_page < 8; desc.m_page++) {
      getMap()
          ->getBank(desc.m_bank)
          ->getPage(desc.m_page)
          ->setNameLong(String::formatted(String("Page %d"), desc.m_page + 1));
      getMap()
          ->getBank(desc.m_bank)
          ->getPage(desc.m_page)
          ->setNameShort(String::formatted(String("Page %d"), desc.m_page + 1));
      for (desc.m_channel = 0; desc.m_channel < 8; desc.m_channel++) {
        if (numParamsMapped >= numParamsExist)
          return;

        PMParam *pParam = getPMParam(&desc);
        if (!pParam)
          continue;
        pParam->setParamID(numParamsMapped);

        bool valid = TrackFX_GetParamName(m_pPlugTrack, m_iSlot,
                                          numParamsMapped, paramName, 79);
        if (valid) {
          pParam->setNameShort(shortNameFromCString(paramName));
          pParam->setNameLong(longNameFromCString(paramName));
        }

        numParamsMapped++;
      }
    }
  }
}

String PlugAccess::shortNameFromCString(const char *pName) {
  String name = pName;
  return name.substring(0, 6);
}

String PlugAccess::longNameFromCString(const char *pName) {
  String name = pName;
  return name.substring(0, 17);
}

bool PlugAccess::plugExist() {
  return (m_pPlugTrack && TrackFX_GetCount(m_pPlugTrack) > m_iSlot);
}

/**
 * Return the number of params this plugin has.
 *
 * @param includeReaper If true then the returned value will include the params for bypass, dry/wet and delta solo. If
 *                      false then the returned value will only include the params implemented for the plugin itself.
 * @return int
 */
int PlugAccess::getNumParams(bool includeReaper) {
  if (!plugExist()) {
    return 0;
  }

  int numParams = TrackFX_GetNumParams(m_pPlugTrack, m_iSlot);
  if (includeReaper)
      return numParams;

  return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":bypass");
}

PMParam *PlugAccess::getPMParam(ElementDesc *pElement) {
  if (!pElement || !pElement->isValid() || !plugExist()) {
    return NULL;
  }

  // A page of -1 marks a unit that shows no page (empty unit); there is no
  // valid PMPage for it, so every param lookup must fail cleanly.
  if (pElement->m_page < 0) {
    return NULL;
  }

  if (!resolveIndirection(pElement)) {
    return NULL;
  }

  ASSERT(pElement->isValid());

  switch (pElement->m_type) {
  case ElementDesc::FADER:
    return getMap()
        ->getBank(pElement->m_bank)
        ->getPage(pElement->m_page)
        ->getFader(pElement->m_channel);
  case ElementDesc::VPOT:
    return getMap()
        ->getBank(pElement->m_bank)
        ->getPage(pElement->m_page)
        ->getVPot(pElement->m_channel);
  case ElementDesc::DRYWET:
  case ElementDesc::BYPASS:
  case ElementDesc::UNKNOWN:
  case ElementDesc::DELTA:
    break;
  }
  ASSERT_M(false, "type is unknown");
  return NULL;
}

bool PlugAccess::resolveIndirection(ElementDesc *pDesc) {
  if (!pDesc || !pDesc->isValid())
    return false;
  pDesc->m_offset = 0;

  PMBank *pBank = getMap()->getBank(pDesc->m_bank);
  if (pBank->doesRefer()) {
    pDesc->m_bank = pBank->referTo();
    pDesc->m_offset = pBank->getParamIDOffset();
  }

  PMPage *pPage = getMap()->getBank(pDesc->m_bank)->getPage(pDesc->m_page);
  if (pPage->doesRefer()) {
    pDesc->m_page = pPage->referTo();
    pDesc->m_offset += pPage->getParamIDOffset();
  }

  PMParam *pParam;
  switch (pDesc->m_type) {
  case ElementDesc::FADER:
    pParam = getMap()
                 ->getBank(pDesc->m_bank)
                 ->getPage(pDesc->m_page)
                 ->getFader(pDesc->m_channel);
    break;
  case ElementDesc::VPOT:
    pParam = getMap()
                 ->getBank(pDesc->m_bank)
                 ->getPage(pDesc->m_page)
                 ->getVPot(pDesc->m_channel);
    break;
  default:
    ASSERT_M(false, "type is unknown");
    return false;
  }

  if (pParam->getParamID() == NOT_ASSIGNED) {
    pDesc->m_offset = 0;
  }

  return true;
}

// ---- Explicit bank/page parameter overloads ----

String PlugAccess::getParamNameShort(int bank, int page,
                                     ElementDesc::eType type, int channel) {
  ElementDesc desc(bank, page, type, channel);
  return getPMParam(&desc) ? getPMParam(&desc)->getNameShort() : String();
}

String PlugAccess::getParamNameLong(int bank, int page,
                                    ElementDesc::eType type, int channel) {
  ElementDesc desc(bank, page, type, channel);
  return getPMParam(&desc) ? getPMParam(&desc)->getNameLong() : String();
}

int PlugAccess::getParamID(ElementDesc *pElement) {
  if (!pElement || !pElement->isValid() || !plugExist())
    return NOT_ASSIGNED;
  if (pElement->m_type == ElementDesc::DRYWET) {
    return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":wet");
  } else if (pElement->m_type == ElementDesc::BYPASS) {
    return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":bypass");
  } else if (pElement->m_type == ElementDesc::DELTA) {
    return TrackFX_GetParamFromIdent(m_pPlugTrack, m_iSlot, ":delta");
  }

  PMParam *pParam = getPMParam(pElement);
  if (pParam == NULL)
    return NOT_ASSIGNED;

  return pParam->getParamID() + pElement->m_offset;
}

int PlugAccess::getParamID(int bank, int page, ElementDesc::eType type,
                           int channel) {
  ElementDesc desc(bank, page, type, channel);
  return getParamID(&desc);
}

void PlugAccess::setParamValueInt(int bank, int page,
                                  ElementDesc::eType type, int channel,
                                  int value) {
  if (!plugExist())
    return;

  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    TrackFX_SetParam(m_pPlugTrack, m_iSlot, id,
                     convertMCU2R(id, std::min(value, MAX_FADER_VALUE_INT)));
  }
}

void PlugAccess::setParamValueDouble(int bank, int page,
                                     ElementDesc::eType type, int channel,
                                     double value) {
  if (!plugExist())
    return;

  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    TrackFX_SetParam(m_pPlugTrack, m_iSlot, id, value);
  }
}

int PlugAccess::getParamValueInt(int bank, int page, ElementDesc::eType type,
                                 int channel) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return convertR2MCU(
        id, TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max));
  }

  return 0;
}

double PlugAccess::getParamValueDouble(int bank, int page,
                                       ElementDesc::eType type, int channel) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  }

  return 0;
}

double PlugAccess::getParamValueDouble(ElementDesc *desc) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(desc);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  }

  return 0;
}

PMVPot::tSteps *PlugAccess::getParamSteps(int bank, int page, int vpot) {
  if (!plugExist())
    return NULL;

  ElementDesc desc(bank, page, ElementDesc::VPOT, vpot);
  PMVPot *pVPot = static_cast<PMVPot *>(getPMParam(&desc));
  if (!pVPot)
    return NULL;

  return pVPot->getStepsMap();
}

String PlugAccess::getParamValueShort(int bank, int page,
                                      ElementDesc::eType type, int channel) {
  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams()) {
    double val = TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);

    if (type == ElementDesc::VPOT) {
      PMVPot::tSteps *steps = getParamSteps(bank, page, channel);
      int index = findIndexFromKeyInMap(val, steps);
      if (index >= 0) {
        return getNthValueFromMap(index, steps).get<0>();
      } else
        return String();
    }

    char valueString[80] = {};
    bool valid = TrackFX_FormatParamValue(m_pPlugTrack, m_iSlot, id, val,
                                          valueString, 79);
    if (valid) {
      return shortNameFromCString(valueString);
    } else {
      return String::formatted(String("%1.3f"),
                               getParamValueDouble(bank, page, type, channel));
    }
  }

  return String();
}

String PlugAccess::getParamValueLong(int bank, int page,
                                     ElementDesc::eType type, int channel) {
  double min, max;
  int id = getParamID(bank, page, type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams()) {
    double val = TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);

    if (type == ElementDesc::VPOT) {
      PMVPot::tSteps *steps = getParamSteps(bank, page, channel);
      int index = findIndexFromKeyInMap(val, steps);
      if (index >= 0) {
        return getNthValueFromMap(index, steps).get<1>();
      }
    }

    char valueString[80] = {};
    bool valid = TrackFX_FormatParamValue(m_pPlugTrack, m_iSlot, id, val,
                                          valueString, 79);
    if (valid) {
      return longNameFromCString(valueString);
    } else {
      return String::formatted(String("%1.3f"),
                               getParamValueDouble(bank, page, type, channel));
    }
  }

  return String();
}

double PlugAccess::convertMCU2R(int id, int value) {
  ASSERT(plugExist());

  double min, max;
  TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  double valueAsD = ((double)value) / MAX_FADER_VALUE;
  return min + (max - min) * valueAsD;
}

int PlugAccess::convertR2MCU(int id, double value) {
  ASSERT(plugExist());

  double min, max;
  TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  double normed = (value - min) / (max - min);
  return (int)(normed * MAX_FADER_VALUE);
}

// ---- per-unit setters ----

void PlugAccess::setSelectedBank(int bank, int unit) {
  if (unit < 0 || unit >= MAX_SURFACE_UNITS || bank < 0 || bank >= 8)
    return;
  m_selectedBankPerUnit[unit] = bank;
  // Always update LEDs/faders — affects all units (N>1)
  m_pMode->updateSoloLEDs();
  m_pMode->updateMuteLEDs();
  m_pMode->updateFaders();
  m_pMode->updateSelectLEDs();
  m_pMode->updateRecLEDs();
}

void PlugAccess::setSelectedPage(int bank, int page, int unit) {
  if (unit < 0 || unit >= MAX_SURFACE_UNITS || bank < 0 || bank >= 8 ||
      page < 0 || page >= 8)
    return;
  m_selectedPagePerUnit[unit][bank] = page;
  // Selecting a page makes the unit non-empty.
  m_unitEmpty[unit] = false;
  // Always update LEDs/faders — affects all units (N>1)
  m_pMode->updateMuteLEDs();
  m_pMode->updateFaders();
  m_pMode->updateSelectLEDs();
  m_pMode->updateRecLEDs();
}

void PlugAccess::setSelectedPageInSelectedBank(int page, int unit) {
  if (unit < 0 || unit >= MAX_SURFACE_UNITS)
    return;
  setSelectedPage(m_selectedBankPerUnit[unit], page, unit);
}

void PlugAccess::trackRemoved(MediaTrack *pMT) {
  Tracks::instance()->selectionChanged();

  if (pMT == m_pPlugTrack) {
    accessPlugin(NULL, -1);
  }
}

// resolveBankReference uses the active unit's bank
int PlugAccess::resolveBankReference() {
  int activeUnit = m_pMode->getActiveUnit();
  if (activeUnit < 0 || activeUnit >= MAX_SURFACE_UNITS)
    return 0;
  int activeBank = m_selectedBankPerUnit[activeUnit];
  return resolveBankReference(activeBank);
}

int PlugAccess::resolveBankReference(int bank) {
  if (bank < 0 || bank >= 8)
    return 0;
  return getMap()->getBank(bank)->doesRefer()
             ? getMap()->getBank(bank)->referTo()
             : bank;
}

bool PlugAccess::isPageUsedInSelectedBank(int page) {
  return getMap()->getBank(resolveBankReference())->getPage(page)->isUsed();
}

PlugMap *PlugAccess::getMap() { return m_pMapManager->getActiveMap(); }

// ---- Used-page sequence helpers ----

std::vector<int> PlugAccess::usedPages(int bank) {
  std::vector<int> result;
  PlugMap *map = getMap();
  if (!map)
    return result;
  for (int p = 0; p < 8; p++) {
    if (map->getBank(bank)->getPage(p)->isUsed())
      result.push_back(p);
  }
  return result;
}

int PlugAccess::usedPageCount(int bank) {
  return (int)usedPages(bank).size();
}

int PlugAccess::pageAtUsedOffset(int bank, int offset) {
  std::vector<int> pages = usedPages(bank);
  if (pages.empty())
    return 0;
  if (offset < 0)
    return pages.front();
  if (offset >= (int)pages.size())
    return pages.back();
  return pages[offset];
}

int PlugAccess::pageUsedOffsetForPage(int bank, int page) {
  std::vector<int> pages = usedPages(bank);
  for (int i = 0; i < (int)pages.size(); i++) {
    if (pages[i] == page)
      return i;
  }
  return -1;
}

// ---- Page uniqueness ----

int PlugAccess::firstFreePageForUnit(int bank, int unit) {
  std::vector<int> pages = usedPages(bank);
  int nUnits = m_pMode->getCCSManager()->getMCU()->numUnits();
  for (size_t i = 0; i < pages.size(); i++) {
    int p = pages[i];
    bool taken = false;
    for (int v = 0; v < nUnits; v++) {
      if (v == unit || isUnitEmpty(v))
        continue;
      if (selectedBankForUnit(v) == bank && selectedPageForUnit(v) == p) {
        taken = true;
        break;
      }
    }
    if (!taken)
      return p;
  }
  return -1;
}

void PlugAccess::enforceUniquePages(int changedUnit) {
  int nUnits = m_pMode->getCCSManager()->getMCU()->numUnits();
  // Iterate from the highest unit index so that, when no changedUnit is
  // given (-1), the lowest-index unit of a duplicate group keeps its page.
  for (int v = nUnits - 1; v >= 0; v--) {
    if (v == changedUnit || isUnitEmpty(v))
      continue;
    int bankV = selectedBankForUnit(v);
    int pageV = selectedPageForUnit(v);
    if (pageV < 0)
      continue;
    for (int w = 0; w < nUnits; w++) {
      if (w == v || isUnitEmpty(w))
        continue;
      if (selectedBankForUnit(w) == bankV && selectedPageForUnit(w) == pageV) {
        clearUnitPage(v);
        break;
      }
    }
  }
}

// ---- Persistence ----

#define PLUGACCESS_NODE_ROOT String("PLUGACCESS")
#define PLUGACCESS_NODE_SLOTSTATE String("SLOTSTATES")
#define PLUGACCESS_ATT_SLOTSTATE_TRACK String("track")
#define PLUGACCESS_ATT_SLOTSTATE_SLOT String("slot")
#define PLUGACCESS_ATT_SLOTSTATE_PLUGNAME String("plugname")
#define PLUGACCESS_ATT_SLOTSTATE_BANK String("bank")
#define PLUGACCESS_NODE_SLOTSTATE_PAGE String("PAGE")
#define PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX String("nr")

// versioned UNIT_STATES block
#define PLUGACCESS_NODE_UNIT_STATES String("UNIT_STATES")
#define PLUGACCESS_ATT_VERSION String("version")
#define PLUGACCESS_NODE_UNIT String("UNIT")
#define PLUGACCESS_ATT_UNIT_INDEX String("nr")
#define PLUGACCESS_ATT_UNIT_BANK String("bank")
#define PLUGACCESS_ATT_UNIT_EMPTY String("empty")

#define PLUGACCESS_NODE_SELECTED_PLUG String("SELECTED_PLUG")
#define PLUGACCESS_ATT_SELECTED_PLUG_TRACK String("track")
#define PLUGACCESS_ATT_SELECTED_PLUG_SLOT String("slot")

void PlugAccess::projectChanged(XmlElement *pXmlElement,
                                ProjectConfig::EAction action) {
  XmlElement *pPlugAccessNode;

  switch (action) {
  case ProjectConfig::WRITE:
    storeActualSlotState();
    pPlugAccessNode = new XmlElement(PLUGACCESS_NODE_ROOT);
    writeSlotStatesToProjectConfig(pPlugAccessNode);
    writeSelectedPlugToProjectConfig(pPlugAccessNode);
    pXmlElement->addChildElement(pPlugAccessNode);
    break;

  case ProjectConfig::FREE:
    m_knownSlotStates.clear();
    break;

  case ProjectConfig::READ:
    pPlugAccessNode = pXmlElement->getChildByName(PLUGACCESS_NODE_ROOT);
    if (pPlugAccessNode) {
      readSlotStatesFromProjectConfig(pPlugAccessNode);
      readSelectedPlugFromProjectConfig(pPlugAccessNode, true);
    }
    break;
  }
}

void PlugAccess::writeSlotStatesToProjectConfig(XmlElement *pNode) {
  BOOST_FOREACH (tSlotStatePair &entry, m_knownSlotStates) {
    XmlElement *pSlotState = new XmlElement(PLUGACCESS_NODE_SLOTSTATE);
    tSlotLocation loc = entry.first;
    pSlotState->setAttribute(PLUGACCESS_ATT_SLOTSTATE_TRACK, loc.get<0>());
    pSlotState->setAttribute(PLUGACCESS_ATT_SLOTSTATE_SLOT, loc.get<1>());

    tSlotState state = entry.second;
    pSlotState->setAttribute(PLUGACCESS_ATT_SLOTSTATE_PLUGNAME, state.get<0>());

    // write versioned UNIT_STATES block (v2 adds the per-unit empty flag)
    XmlElement *pUnitStates =
        new XmlElement(PLUGACCESS_NODE_UNIT_STATES);
    pUnitStates->setAttribute(PLUGACCESS_ATT_VERSION, 2);

    boost::array<int, MAX_SURFACE_UNITS> banksPerUnit = state.get<1>();
    boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS> pagesPerUnit =
        state.get<2>();
    boost::array<bool, MAX_SURFACE_UNITS> emptyPerUnit = state.get<3>();

    for (int u = 0; u < MAX_SURFACE_UNITS; u++) {
      XmlElement *pUnit = new XmlElement(PLUGACCESS_NODE_UNIT);
      pUnit->setAttribute(PLUGACCESS_ATT_UNIT_INDEX, u);
      pUnit->setAttribute(PLUGACCESS_ATT_UNIT_BANK, banksPerUnit[u]);
      pUnit->setAttribute(PLUGACCESS_ATT_UNIT_EMPTY, emptyPerUnit[u]);

      for (int p = 0; p < 8; p++) {
        XmlElement *pPage = new XmlElement(PLUGACCESS_NODE_SLOTSTATE_PAGE);
        pPage->setAttribute(PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX,
                            pagesPerUnit[u][p]);
        pUnit->addChildElement(pPage);
      }
      pUnitStates->addChildElement(pUnit);
    }
    pSlotState->addChildElement(pUnitStates);
    pNode->addChildElement(pSlotState);
  }
}

void PlugAccess::readSlotStatesFromProjectConfig(XmlElement *pNode) {
  forEachXmlChildElement(*pNode, pChild) {
    if (pChild->getTagName() == PLUGACCESS_NODE_SLOTSTATE) {
      tSlotLocation loc(
          pChild->getStringAttribute(PLUGACCESS_ATT_SLOTSTATE_TRACK),
          pChild->getIntAttribute(PLUGACCESS_ATT_SLOTSTATE_SLOT));

      String plugName =
          pChild->getStringAttribute(PLUGACCESS_ATT_SLOTSTATE_PLUGNAME);

      boost::array<int, MAX_SURFACE_UNITS> banksPerUnit;
      boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS> pagesPerUnit;
      boost::array<bool, MAX_SURFACE_UNITS> emptyPerUnit;
      banksPerUnit.assign(0);
      for (int u = 0; u < MAX_SURFACE_UNITS; u++)
        pagesPerUnit[u].assign(0);
      emptyPerUnit.assign(false);

      // try versioned UNIT_STATES block first
      XmlElement *pUnitStates =
          pChild->getChildByName(PLUGACCESS_NODE_UNIT_STATES);
      if (pUnitStates && pUnitStates->getIntAttribute(PLUGACCESS_ATT_VERSION) >= 1) {
        int unitCount = 0;
        forEachXmlChildElement(*pUnitStates, pUnit) {
          if (pUnit->getTagName() == PLUGACCESS_NODE_UNIT) {
            int u = pUnit->getIntAttribute(PLUGACCESS_ATT_UNIT_INDEX);
            if (u >= 0 && u < MAX_SURFACE_UNITS) {
              int bank = pUnit->getIntAttribute(PLUGACCESS_ATT_UNIT_BANK);
              banksPerUnit[u] = bank >= 0 && bank < 8 ? bank : 0;
              // v1 states have no empty attribute; the flag defaults to
              // false (unit shows its stored page). Duplicate pages in
              // legacy states are normalized by enforceUniquePages on
              // restore.
              emptyPerUnit[u] =
                  pUnit->getBoolAttribute(PLUGACCESS_ATT_UNIT_EMPTY, false);
              int page = 0;
              forEachXmlChildElement(*pUnit, pPage) {
                if (page < 8) {
                  int pageIndex = pPage->getIntAttribute(
                      PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX);
                  pagesPerUnit[u][page++] =
                      pageIndex >= 0 && pageIndex < 8 ? pageIndex : 0;
                }
              }
              unitCount++;
            }
          }
        }
      } else {
        // Legacy format: single bank + 8 pages → map to unit 0
        int bank = pChild->getIntAttribute(PLUGACCESS_ATT_SLOTSTATE_BANK);
        banksPerUnit[0] = bank >= 0 && bank < 8 ? bank : 0;
        int page = 0;
        forEachXmlChildElement(*pChild, pPage) {
          if (pPage->getTagName() == PLUGACCESS_NODE_SLOTSTATE_PAGE &&
              page < 8) {
            int pageIndex = pPage->getIntAttribute(
                PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX);
            pagesPerUnit[0][page++] =
                pageIndex >= 0 && pageIndex < 8 ? pageIndex : 0;
          }
        }
        // Units 1..N−1: defaults from default page-spread logic
        // (handled by accessPlugin when restoredFromStored is false for those)
      }

      tSlotState state(plugName, banksPerUnit, pagesPerUnit, emptyPerUnit);
      m_knownSlotStates[loc] = state;
    }
  }
}

void PlugAccess::storeActualSlotState() {
  // store per-unit arrays
  if (m_iSlot >= 0 && m_pPlugTrack != NULL) {
    tSlotLocation loc(GUID2String(&m_GUIDplugTrack), m_iSlot);
    tSlotState state(m_plugName, m_selectedBankPerUnit, m_selectedPagePerUnit,
                     m_unitEmpty);
    m_knownSlotStates.erase(loc);
    m_knownSlotStates.insert(std::pair<tSlotLocation, tSlotState>(loc, state));
  }
}

void PlugAccess::writeSelectedPlugToProjectConfig(XmlElement *pPlugAccessNode) {
  XmlElement *pSelectedPlug = new XmlElement(PLUGACCESS_NODE_SELECTED_PLUG);
  pSelectedPlug->setAttribute(PLUGACCESS_ATT_SELECTED_PLUG_TRACK,
                              GUID2String(&m_GUIDplugTrack));
  pSelectedPlug->setAttribute(PLUGACCESS_ATT_SELECTED_PLUG_SLOT, m_iSlot);
  pPlugAccessNode->addChildElement(pSelectedPlug);
}

void PlugAccess::readSelectedPlugFromProjectConfig(
    XmlElement *pPlugAccessNode,
    bool changeTriggeredFromProjectChange) {
  forEachXmlChildElement(*pPlugAccessNode, pChild) {
    if (pChild->getTagName() == PLUGACCESS_NODE_SELECTED_PLUG) {
      String guidString =
          pChild->getStringAttribute(PLUGACCESS_ATT_SELECTED_PLUG_TRACK);
      GUID guid;
      String2GUID(guidString, &guid);
      MediaTrack *pMediaTrack = CSurf_MCU::TrackFromGUID(guid);
      int iSlot = pChild->getIntAttribute(PLUGACCESS_ATT_SELECTED_PLUG_SLOT);

      if (!pMediaTrack || iSlot < 0 || iSlot >= TrackFX_GetCount(pMediaTrack)) {
        accessPlugin(NULL, -1, false, changeTriggeredFromProjectChange);
        return;
      }

      accessPlugin(pMediaTrack, iSlot, false, changeTriggeredFromProjectChange);
      return;
    }
  }
}

// ---- unchanged below this line ----

void PlugAccess::watchedNameParameterChanged(MediaTrack *pMediaTrack, int iSlot,
                                             String newPlugName) {
  if (pMediaTrack == m_pPlugTrack && iSlot == m_iSlot) {
    accessPlugin(pMediaTrack, iSlot);
  }
}

void PlugAccess::checkFloatWindows() {
  std::unique_ptr<FloatingWindowInfo> pFWI;

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_SAME_TRACK) &&
      m_pMode->isFollowTrack()) {
    pFWI.reset(checkAppearingFloats(m_pPlugTrack));
  }

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
    for (TrackIterator ti; !ti.end(); ++ti) {
      std::unique_ptr<FloatingWindowInfo> pTrackFWI(
          checkAppearingFloats(*ti));
      if (pTrackFWI) {
        pFWI = std::move(pTrackFWI);
      }
    }
    // trigger update when user opens floating plugin on master track
    if (!pFWI)
      pFWI.reset(checkAppearingFloats(GetMasterTrack(NULL)));
  }

  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_CHAIN)) {
    if (pFWI) {
      TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 2); // close float
      TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 1); // open chain
      tWindowStates::iterator iterWS = m_knownWndStates.find(
          boost::tuple<MediaTrack *, int>(pFWI->pMediaTrack, pFWI->iSlot));
      if (iterWS != m_knownWndStates.end())
        (*iterWS).second = NULL;
    } else if (!isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
      // we havn't searched for every track
      for (TrackIterator ti; !ti.end(); ++ti) {
        std::unique_ptr<FloatingWindowInfo> trackFWI(
            checkAppearingFloats(*ti, false));
        if (trackFWI) {
          TrackFX_Show(trackFWI->pMediaTrack, trackFWI->iSlot, 2); // close float
          TrackFX_Show(trackFWI->pMediaTrack, trackFWI->iSlot, 1); // open chain
          tWindowStates::iterator iterWS = m_knownWndStates.find(
              boost::tuple<MediaTrack *, int>(trackFWI->pMediaTrack,
                                               trackFWI->iSlot));
          if (iterWS != m_knownWndStates.end())
            (*iterWS).second = NULL;
        }
      }
    }
  }

  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_ONE_GLOBAL)) {
    m_pWindowManager->allowOnlySelectedFloat(m_pPlugTrack, m_iSlot);
  }
}

void PlugAccess::syncKnownStates() {
  // Sync chain visible states for all tracks so the next frame update
  // doesn't detect false changes from deactivate/activate cycles.
  for (TrackIterator ti; !ti.end(); ++ti) {
    int chainVisible = TrackFX_GetChainVisible(*ti);
    if (chainVisible >= 0) {
      m_knownChainStates[*ti] = chainVisible;
    } else {
      m_knownChainStates.erase(*ti);
    }
  }
  // Also check master track
  MediaTrack *master = GetMasterTrack(NULL);
  int masterChain = TrackFX_GetChainVisible(master);
  if (masterChain >= 0) {
    m_knownChainStates[master] = masterChain;
  } else {
    m_knownChainStates.erase(master);
  }

  // Sync known floating window states
  for (TrackIterator ti; !ti.end(); ++ti) {
    int numFX = TrackFX_GetCount(*ti);
    for (int i = 0; i < numFX; i++) {
      m_knownWndStates[boost::tuple<MediaTrack *, int>(*ti, i)] =
          TrackFX_GetFloatingWindow(*ti, i);
    }
  }
}

void PlugAccess::checkChainChanges() {
  static MediaTrack *lastMediaTrack = NULL;
  static int lastSlot = -1;

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_SAME_TRACK) &&
      m_pMode->isFollowTrack()) {
    checkChain(m_pPlugTrack);
  }

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
    for (TrackIterator ti; !ti.end(); ++ti) {
      checkChain(*ti);
    }
    // trigger update when user changes selected plugin on master track
    checkChain(GetMasterTrack(NULL));
  }
}

void PlugAccess::checkChain(MediaTrack *pTrack) {
  int chainVisible = TrackFX_GetChainVisible(pTrack);
  if (chainVisible < 0)
    return;

  tChainSlot::iterator iterCS = m_knownChainStates.find(pTrack);
  if (iterCS != m_knownChainStates.end()) {
    if ((*iterCS).second != chainVisible) {
      accessPlugin(pTrack, chainVisible, true);
    }
    (*iterCS).second = chainVisible;
  } else {
    m_knownChainStates.insert(tChainSlot::value_type(pTrack, chainVisible));
    accessPlugin(pTrack, chainVisible, true);
  }
}

FloatingWindowInfo *PlugAccess::checkAppearingFloats(MediaTrack *pTrack,
                                                     bool accessAppearing) {
  int numFX = TrackFX_GetCount(pTrack);
  for (int i = 0; i < numFX; i++) {
    tWindowStates::iterator iterWS =
        m_knownWndStates.find(boost::tuple<MediaTrack *, int>(pTrack, i));
    HWND actualHWND = TrackFX_GetFloatingWindow(pTrack, i);
    HWND knownHWND = NULL;
    if (iterWS != m_knownWndStates.end()) {
      knownHWND = (*iterWS).second;
      (*iterWS).second = actualHWND;
    } else {
      m_knownWndStates.insert(tWindowStates::value_type(
          boost::tuple<MediaTrack *, int>(pTrack, i), actualHWND));
    }
    if (knownHWND != actualHWND && actualHWND != NULL) {
      if (accessAppearing && (i != m_iSlot || pTrack != m_pPlugTrack))
        accessPlugin(pTrack, i, true);

      return new FloatingWindowInfo(actualHWND, pTrack, i);
    }
  }
  return NULL;
}

void PlugAccess::openFX() {
  if (isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_CHAIN) ||
      isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_CHAIN_CLOSE_FLOAT)) {
    TrackFX_Show(m_pPlugTrack, m_iSlot, 1);
  } else if (isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_FLOATING)) {
    TrackFX_Show(m_pPlugTrack, m_iSlot, 3);
  }
}
