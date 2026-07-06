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
//#include "SnM/SnM_Actions.h"

//#define TRACK_CHANGE_TRACK_UNKNOWN -1
//#define TRACK_CHANGE_TRACK_NOT_CHANGED -2

PlugAccess::PlugAccess(PlugMode *pMode)
    : m_pMode(pMode), m_selectedBank(0), m_pPlugTrack(NULL), m_iSlot(-1),
      m_pMapManager(NULL), m_plugName(String()),
      m_GUIDplugTrack(GUID_NOT_ACTIVE) {
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

  m_selectedBank = 0;
  BOOST_FOREACH (int &page, m_selectedPage)
    page = 0;

  if (changeTriggeredFromProjectChange || !plugExist()) {
    m_pMapManager->deselectMap();
    m_pMode->updateEverything();
    m_pPlugWatcher->setPlugin(NULL, -1);
    m_iSlot = -1;
    return;
  }

  m_plugName = getPlugNameLong();
  m_pMapManager->loadMapForPlug(m_pPlugTrack, m_iSlot);

  // restore the stored state
  tSlotStatesMap::iterator iterStoredStates = m_knownSlotStates.find(
      tSlotLocation(GUID2String(CSurf_MCU::GUIDfromTrack(pMediaTrack)), iSlot));
  if (iterStoredStates != m_knownSlotStates.end()) {
    tSlotState storedState = (*iterStoredStates).second;
    String storedPlugName = storedState.get<0>();
    if (storedPlugName.equalsIgnoreCase(getPlugNameLong())) {
      m_selectedBank = storedState.get<1>();
      m_selectedPage = storedState.get<2>();
    }
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

  char paramName[80];
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

  char paramName[80];
  bool valid = TrackFX_GetFXName(pMediaTrack, slot, paramName, 79);
  if (!valid) {
    return String();
  }

  return String(paramName);
}

void PlugAccess::createDefaultMap() {
  int numParamsExist = getNumParams();
  int numParamsMapped = 0;

  char paramName[80];

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
  if (!plugExist()) {
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
  }
  ASSERT_M(false, "type is unknown");
  return NULL;
}

int PlugAccess::getParamID(ElementDesc *pElement) {
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

bool PlugAccess::resolveIndirection(ElementDesc *pDesc) {
  ASSERT(pDesc->isValid());
  ASSERT(pDesc->m_offset == 0);

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
  }

  if (pParam->getParamID() == NOT_ASSIGNED) {
    pDesc->m_offset = 0;
  }

  return true;
}

void PlugAccess::setParamValueInt(ElementDesc::eType type, int channel,
                                  int value) {
  if (!plugExist())
    return;

  int id = getParamID(type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    TrackFX_SetParam(m_pPlugTrack, m_iSlot, id,
                     convertMCU2R(id, std::min(value, MAX_FADER_VALUE_INT)));
  }
}

void PlugAccess::setParamValueDouble(ElementDesc::eType type, int channel,
                                     double value) {
  if (!plugExist())
    return;

  int id = getParamID(type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    TrackFX_SetParam(m_pPlugTrack, m_iSlot, id, value);
  }
}

int PlugAccess::getParamValueInt(ElementDesc::eType type, int channel) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return convertR2MCU(
        id, TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max));
  }

  return 0;
}

double PlugAccess::getParamValueDouble(ElementDesc::eType type, int channel) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  }

  return 0;
}

double PlugAccess::getParamValueDouble(ElementDesc* desc) {
  if (!plugExist())
    return 0;

  double min, max;
  int id = getParamID(desc);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams(true)) {
    return TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);
  }

  return 0;
}


PMVPot::tSteps *PlugAccess::getParamSteps(int vpot) {
  if (!plugExist())
    return NULL;

  PMVPot *pVPot = static_cast<PMVPot *>(getPMParam(ElementDesc::VPOT, vpot));

  return pVPot->getStepsMap();
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

void PlugAccess::setSelectedBank(int bank) {
  m_selectedBank = bank;
  m_pMode->updateSoloLEDs();
  m_pMode->updateMuteLEDs();
  m_pMode->updateFaders();
}

void PlugAccess::setSelectedPage(int bank, int page) {
  m_selectedPage[bank] = page;
  m_pMode->updateMuteLEDs();
  m_pMode->updateFaders();
}

void PlugAccess::setSelectedPageInSelectedBank(int page) {
  setSelectedPage(m_selectedBank, page);
}

String PlugAccess::getParamValueShort(ElementDesc::eType type, int channel) {
  double min, max;
  int id = getParamID(type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams()) {
    double val = TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);

    if (type == ElementDesc::VPOT) {
      PMVPot::tSteps *steps = getParamSteps(channel);
      int index = findIndexFromKeyInMap(val, steps);
      if (index >= 0) {
        return getNthValueFromMap(index, steps).get<0>();
      } else
        return String();
    }

    char valueString[80];
    bool valid = TrackFX_FormatParamValue(m_pPlugTrack, m_iSlot, id, val,
                                          valueString, 79);
    if (valid) {
      return shortNameFromCString(valueString);
    } else {
      return String::formatted(String("%1.3f"),
                               getParamValueDouble(type, channel));
    }
  }

  return String();
}

String PlugAccess::getParamValueLong(ElementDesc::eType type, int channel) {
  double min, max;
  int id = getParamID(type, channel);
  if (id != NOT_ASSIGNED && id >= 0 && id < getNumParams()) {
    double val = TrackFX_GetParam(m_pPlugTrack, m_iSlot, id, &min, &max);

    if (type == ElementDesc::VPOT) {
      PMVPot::tSteps *steps = getParamSteps(channel);
      int index = findIndexFromKeyInMap(val, steps);
      if (index >= 0) {
        return getNthValueFromMap(index, steps).get<1>();
      }
    }

    char valueString[80];
    bool valid = TrackFX_FormatParamValue(m_pPlugTrack, m_iSlot, id, val,
                                          valueString, 79);
    if (valid) {
      return longNameFromCString(valueString);
    } else {
      return String::formatted(String("%1.3f"),
                               getParamValueDouble(type, channel));
    }
  }

  return String();
}

void PlugAccess::trackRemoved(MediaTrack *pMT) {
  Tracks::instance()->selectionChanged();

  if (pMT == m_pPlugTrack) {
    accessPlugin(NULL, -1);
    //    m_pPlugTrack = NULL;
    //    m_pMode->updateEverything();
  }
}

int PlugAccess::resolveBankReference() {
  return getMap()->getBank(m_selectedBank)->doesRefer()
             ? getMap()->getBank(m_selectedBank)->referTo()
             : m_selectedBank;
}

bool PlugAccess::isPageUsedInSelectedBank(int page) {
  return getMap()->getBank(resolveBankReference())->getPage(page)->isUsed();
}

PlugMap *PlugAccess::getMap() { return m_pMapManager->getActiveMap(); }

void PlugAccess::watchedNameParameterChanged(MediaTrack *pMediaTrack, int iSlot,
                                             String newPlugName) {
  if (pMediaTrack == m_pPlugTrack && iSlot == m_iSlot) {
    accessPlugin(pMediaTrack, iSlot);
  }
}

void PlugAccess::checkFloatWindows() {
  FloatingWindowInfo *pFWI = NULL;

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_SAME_TRACK) &&
      m_pMode->isFollowTrack()) {
    pFWI = checkAppearingFloats(m_pPlugTrack);
  }

  if (isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
    for (TrackIterator ti; !ti.end(); ++ti) {
      FloatingWindowInfo *pTrackFWI = checkAppearingFloats(*ti);
      if (pTrackFWI) {
        pFWI = pTrackFWI;
      }
    }
		// trigger update when user opens floating plugin on master track
    if (!pFWI)
      pFWI = checkAppearingFloats(GetMasterTrack(NULL));
  }

  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_CHAIN)) {
    if (pFWI) {
      TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 2); // close float
      TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 1); // open chain
      tWindowStates::iterator iterWS = m_knownWndStates.find(
          boost::tuple<MediaTrack *, int>(pFWI->pMediaTrack, pFWI->iSlot));
      (*iterWS).second = NULL;
    } else if (!isOptionSetTo(PMO_MCU_FOLLOW, PMOA_ALWAYS)) {
      // we havn't searched for every track
      for (TrackIterator ti; !ti.end(); ++ti) {
        FloatingWindowInfo *pFWI = checkAppearingFloats(*ti, false);
        if (pFWI) {
          TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 2); // close float
          TrackFX_Show(pFWI->pMediaTrack, pFWI->iSlot, 1); // open chain
          tWindowStates::iterator iterWS = m_knownWndStates.find(
              boost::tuple<MediaTrack *, int>(pFWI->pMediaTrack, pFWI->iSlot));
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

#define PLUGACCESS_NODE_ROOT String("PLUGACCESS")
#define PLUGACCESS_NODE_SLOTSTATE String("SLOTSTATES")
#define PLUGACCESS_ATT_SLOTSTATE_TRACK String("track")
#define PLUGACCESS_ATT_SLOTSTATE_SLOT String("slot")
#define PLUGACCESS_ATT_SLOTSTATE_PLUGNAME String("plugname")
#define PLUGACCESS_ATT_SLOTSTATE_BANK String("bank")
#define PLUGACCESS_NODE_SLOTSTATE_PAGE String("PAGE")
#define PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX String("nr")
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
    pSlotState->setAttribute(PLUGACCESS_ATT_SLOTSTATE_BANK, state.get<1>());
    boost::array<int, 8> pages = state.get<2>();
    BOOST_FOREACH (int &page, pages) {
      XmlElement *pPage = new XmlElement(PLUGACCESS_NODE_SLOTSTATE_PAGE);
      pPage->setAttribute(PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX, page);
      pSlotState->addChildElement(pPage);
    }
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
      int bank = pChild->getIntAttribute(PLUGACCESS_ATT_SLOTSTATE_BANK);
      boost::array<int, 8> pages;
      int page = 0;
      forEachXmlChildElement(*pChild, pPage) {
        pages[page++] =
            pPage->getIntAttribute(PLUGACCESS_ATT_SLOTSTATE_PAGE_INDEX);
      }
      tSlotState state(plugName, bank, pages);

      m_knownSlotStates[loc] = state;
    }
  }
}

void PlugAccess::storeActualSlotState() {
  // store the actual state (selectedBank and Pages) in the knownSlotStates map
  if (m_iSlot >= 0 && m_pPlugTrack != NULL) {
    tSlotLocation loc(GUID2String(&m_GUIDplugTrack), m_iSlot);
    tSlotState state(m_plugName, m_selectedBank, m_selectedPage);
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

      accessPlugin(pMediaTrack, iSlot, false, changeTriggeredFromProjectChange);
      return;
    }
  }
}
