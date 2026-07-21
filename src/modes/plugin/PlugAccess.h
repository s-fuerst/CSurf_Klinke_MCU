/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once

#include <boost/array.hpp>
#include "csurf_mcu.h"
#include <boost/scoped_ptr.hpp>
#include "JuceHeader.h"

#include "PlugMap.h"
#include "PlugMode.h"
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <map>

#define RESOLVE_ERROR -594875
#define MAX_FADER_VALUE 16368.0
#define MAX_FADER_VALUE_INT 16368

class PluginWatcher;
class PlugMapManager;
class PlugWindowManager;
class FloatingWindowInfo;

class PlugAccess {
public:
  class ElementDesc {
  public:
    enum eType { FADER = 0, VPOT, DRYWET, BYPASS, UNKNOWN, DELTA };

    ElementDesc(int bank, int page, eType type, int channel) {
      m_bank = bank;
      m_page = page;
      m_type = type;
      m_channel = channel;
      m_offset = 0;
    }

    ElementDesc(PlugAccess *pPA, eType type, int channel) {
      // WP-PlugMode guard: getSelectedBank() reads the active unit's state.
      // If m_activeUnit is stale, resolution silently targets the wrong unit.
      ASSERT(pPA->getActiveUnit() >= 0 && pPA->getActiveUnit() < MAX_SURFACE_UNITS);
      m_bank = pPA->getSelectedBank();
      m_page = pPA->getSelectedPageInSelectedBank();
      m_type = type;
      m_channel = channel;
      m_offset = 0;
    }

    bool isValid() {
      return (m_bank >= 0 && m_bank < 9 && m_page >= 0 && m_bank < 9 &&
              m_type != UNKNOWN && m_channel >= 0 && m_channel < 9);
    }

  public:
    int m_bank; // 0 based
    int m_page; // 0 based
    eType m_type;
    int m_channel; // 0 based
    int m_offset;  // this is the paramId offset value derived from
                   // resolveIndirection
  };

  PlugAccess(PlugMode *pMode);

public:
  ~PlugAccess(void);

  void trackChanged(
      MediaTrack *pMediaTrack); // can be also the actual selected track
  void accessPlugin(MediaTrack *, int slot,
                    bool changeTriggeredFromGUI = false,
                    bool changeTriggeredFromProjectChange = false); // slot is 0 based

  void storeActualSlotState();
  int getPlugSlot() { return m_iSlot; }
  String getPlugNameShort() { return getPlugName(true, m_pPlugTrack, m_iSlot); }
  String getPlugNameShort(MediaTrack *pMediaTrack, int slot) {
    return getPlugName(true, pMediaTrack, slot);
  }
  String getPlugNameLong() { return getPlugName(false, m_pPlugTrack, m_iSlot); }
  String getPlugNameLong(MediaTrack *pMediaTrack, int slot) {
    return getPlugName(false, pMediaTrack, slot);
  }
  String getFullPlugName() { return getFullPlugName(m_pPlugTrack, m_iSlot); }
  String getFullPlugName(MediaTrack *pMediaTrack, int slot);
  MediaTrack *getPlugTrack() { return m_pPlugTrack; }
  const GUID &getPlugTrackGUID() { return m_GUIDplugTrack; }

  // Parameter — selected-based (delegates to explicit overloads with active
  // unit's Bank/Page)
  String getParamNameShort(ElementDesc::eType type, int channel) {
    return getParamNameShort(getSelectedBank(), getSelectedPageInSelectedBank(),
                             type, channel);
  }
  String getParamNameLong(ElementDesc::eType type, int channel) {
    return getParamNameLong(getSelectedBank(), getSelectedPageInSelectedBank(),
                            type, channel);
  }
  void setParamValueInt(
      ElementDesc::eType type, int channel, int value) {
    setParamValueInt(getSelectedBank(), getSelectedPageInSelectedBank(), type,
                     channel, value);
  }
  void setParamValueDouble(ElementDesc::eType type, int channel, double value) {
    setParamValueDouble(getSelectedBank(), getSelectedPageInSelectedBank(), type,
                        channel, value);
  }
  int getParamValueInt(
      ElementDesc::eType type,
      int channel = 0) {
    return getParamValueInt(getSelectedBank(), getSelectedPageInSelectedBank(),
                            type, channel);
  }
  double getParamValueDouble(
      ElementDesc::eType type, int channel) {
    return getParamValueDouble(getSelectedBank(), getSelectedPageInSelectedBank(),
                               type, channel);
  }
  double getParamValueDouble(ElementDesc *desc);
  PMVPot::tSteps *getParamSteps(int vpot) {
    return getParamSteps(getSelectedBank(), getSelectedPageInSelectedBank(), vpot);
  }
  String getParamValueShort(ElementDesc::eType type, int channel) {
    return getParamValueShort(getSelectedBank(), getSelectedPageInSelectedBank(),
                              type, channel);
  }
  String getParamValueLong(ElementDesc::eType type, int channel) {
    return getParamValueLong(getSelectedBank(), getSelectedPageInSelectedBank(),
                             type, channel);
  }

  int getParamID(ElementDesc *element);
  int getParamID(ElementDesc::eType type, int channel) {
    return getParamID(getSelectedBank(), getSelectedPageInSelectedBank(), type,
                      channel);
  }

  // ---- WP-PlugMode: explicit Bank/Page overloads (D1 / R1) ----
  String getParamNameShort(int bank, int page, ElementDesc::eType type,
                           int channel);
  String getParamNameLong(int bank, int page, ElementDesc::eType type,
                          int channel);
  void setParamValueInt(int bank, int page, ElementDesc::eType type, int channel,
                        int value);
  void setParamValueDouble(int bank, int page, ElementDesc::eType type,
                           int channel, double value);
  int getParamValueInt(int bank, int page, ElementDesc::eType type,
                       int channel);
  double getParamValueDouble(int bank, int page, ElementDesc::eType type,
                             int channel);
  PMVPot::tSteps *getParamSteps(int bank, int page, int vpot);
  String getParamValueShort(int bank, int page, ElementDesc::eType type,
                            int channel);
  String getParamValueLong(int bank, int page, ElementDesc::eType type,
                           int channel);
  int getParamID(int bank, int page, ElementDesc::eType type, int channel);

  // Bank — active-unit aliases (selected-based, for editor/selector compat)
  void setSelectedBank(int bank) {
    setSelectedBank(bank, m_pMode->getActiveUnit());
  }
  int getSelectedBank() {
    int u = m_pMode->getActiveUnit();
    ASSERT(u >= 0 && u < MAX_SURFACE_UNITS);
    return m_selectedBankPerUnit[u];
  }
  String getBankNameLong(int bank) {
    return getMap()->getBank(bank)->getNameLong();
  }
  String getBankNameShort(int bank) {
    return getMap()->getBank(bank)->getNameShort();
  }
  bool isBankUsed(int bank) {
    return getMap()->getBank(bank)->isUsed();
  } // 0 based

  // Page — active-unit aliases
  void setSelectedPage(int bank, int page) {
    setSelectedPage(bank, page, m_pMode->getActiveUnit());
  }
  void setSelectedPageInSelectedBank(int page) {
    setSelectedPageInSelectedBank(page, m_pMode->getActiveUnit());
  }
  int getSelectedPageInSelectedBank() {
    int u = m_pMode->getActiveUnit();
    int bank = m_selectedBankPerUnit[u];
    return m_selectedPagePerUnit[u][bank];
  }
  String getPageNameLongInSelectedBank(int page) {
    return getMap()
        ->getBank(resolveBankReference())
        ->getPage(page)
        ->getNameLong();
  }
  String getPageNameShortInSelectedBank(int page) {
    return getMap()
        ->getBank(resolveBankReference())
        ->getPage(page)
        ->getNameShort();
  }
  bool isPageUsed(int bank, int page) {
    return getMap()->getBank(bank)->getPage(page)->isUsed();
  }
  bool isPageUsedInSelectedBank(int page); // 0 based

  // ---- WP-PlugMode: per-unit accessors (Phase 0) ----
  int  selectedBankForUnit(int u) const { return m_selectedBankPerUnit[u]; }
  int  selectedPageForUnit(int u) const {
    return m_selectedPagePerUnit[u][m_selectedBankPerUnit[u]];
  }
  int  selectedPageForUnit(int u, int bank) const {
    return m_selectedPagePerUnit[u][bank];
  }
  void setSelectedBank(int bank, int unit);
  void setSelectedPage(int bank, int page, int unit);
  void setSelectedPageInSelectedBank(int page, int unit);
  int  getActiveUnit() const { return m_pMode->getActiveUnit(); }

  // ---- WP-PlugMode: used-page-sequence helpers (R11) ----
  std::vector<int> usedPages(int bank);
  int usedPageCount(int bank);
  int pageAtUsedOffset(int bank, int offset);
  // Inverse of pageAtUsedOffset: sequence position of `page` in bank's used-page
  // list, or -1 if the page is unused. Used by transport page-window math.
  int pageUsedOffsetForPage(int bank, int page);

  // TrackFX_ releated stuff
  bool plugExist();
  int getNumParams(bool includingReaper = false);

  void checkChainChanges();
  void checkFloatWindows();
  void syncKnownStates();

  FloatingWindowInfo *checkAppearingFloats(MediaTrack *pTrack,
                                           bool accessAppearing = true);
  // Slots
  void trackRemoved(MediaTrack *pMT);

  PluginWatcher *getPlugWatcher() { return m_pPlugWatcher; }
  void watchedNameParameterChanged(MediaTrack *pMediaTrack, int iSlot,
                                   String newPlugName);
  int m_nameChangedConnectionId;

  PlugWindowManager *getPlugWindowManager() { return m_pWindowManager; }
  void openFX();

  // the editor needs access to the map, but PlugMode should use only the
  // provided methodes above
  PlugMap *getMap();
  PlugMapManager *getMapManager() { return m_pMapManager; }

  // Helper
  static String shortNameFromCString(const char *pName);
  static String longNameFromCString(const char *pName);

  void projectChanged(XmlElement *pXmlElement, ProjectConfig::EAction action);

  void createDefaultMap();

private:
  //      PMParam* get corresponding parameter to element description, incl
  //      reference resolving. Can be NULL in the case that the resolving fails.
  PMParam *getPMParam(ElementDesc *element);
  PMParam *getPMParam(ElementDesc::eType type, int channel) {
    boost::scoped_ptr<PlugAccess::ElementDesc> pDesc(
        new ElementDesc(this, type, channel));
    return getPMParam(pDesc.get());
  }
  // check the page/bank references and update the given ElementDesc, returns
  // the offset of the parameter id, or RESOLVE_ERROR if a loop is detected
  bool resolveIndirection(ElementDesc *desc);

  String getPlugName(bool shortName, MediaTrack *pMediaTrack, int slot);

  // create a ElementDesc with selectedBank, selectedPage and the give paramters
  ElementDesc *createDesc(ElementDesc::eType type, int channel);

  double convertMCU2R(int id, int value);
  int convertR2MCU(int id, double value);
  int resolveBankReference();
public:
  // Resolve a bank reference for an explicit bank (doesRefer -> referTo),
  // independent of the active unit. Used by page-cascade math which must read
  // the map's used-page sequence on the RESOLVED bank while storing the
  // selection under the RAW bank.
  int resolveBankReference(int bank);
private:

  bool isOptionSetTo(const String &optionName, const String &attribute) {
    return m_pMode->getOptions()->isOptionSetTo(optionName, attribute);
  }
  void checkChain(MediaTrack *pTrack);
  String m_plugName; // used in accessPlugin so that the name must not accessed
                     // for saving the slotState (it's possible that the track
                     // doesn't exist anymore)

  // WP-PlugMode: per-unit state (Phase 0).  Index 0..MAX_SURFACE_UNITS-1.
  // m_selectedBankPerUnit[u] = selected bank for unit u, -1 = unassigned.
  boost::array<int, MAX_SURFACE_UNITS> m_selectedBankPerUnit;
  // m_selectedPagePerUnit[u][bank] = selected page in bank for unit u.
  boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS> m_selectedPagePerUnit;

  MediaTrack *m_pPlugTrack; // can be NULL
  GUID m_GUIDplugTrack; // GetTrackGUID only works as long as the track is in
                        // the actual project. As a workaround a store the GUID
                        // when i assign a new track to m_pPlugTrack

  int m_iSlot; // 0 based
  void writeSelectedPlugToProjectConfig(XmlElement *pPlugAccessNode);
  void readSelectedPlugFromProjectConfig(XmlElement *pPlugAccessNode, bool changeTriggeredFromProjectChange = false);

  PlugMode *m_pMode;
  PlugMapManager *m_pMapManager;

  // WP-PlugMode: widened tSlotState (R9) — per-unit banks + pages.
  // Format: (plugName, banksPerUnit[], pagesPerUnit[][])
  typedef boost::tuple<String,
                       boost::array<int, MAX_SURFACE_UNITS>,
                       boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS>>
      tSlotState;
  typedef boost::tuple<String /* GUID */, int> tSlotLocation;
  typedef std::pair<const tSlotLocation, tSlotState> tSlotStatePair;
  typedef std::map<tSlotLocation, tSlotState> tSlotStatesMap;
  tSlotStatesMap m_knownSlotStates;
  void writeSlotStatesToProjectConfig(XmlElement *pNode);
  void readSlotStatesFromProjectConfig(XmlElement *pNode);
  typedef std::map<unsigned long, int> tTrack2Plug;
  tTrack2Plug m_track2Slot;

  typedef std::map<boost::tuple<MediaTrack *, int>, HWND> tWindowStates;
  tWindowStates m_knownWndStates;

  typedef std::map<MediaTrack *, int> tChainSlot;
  tChainSlot m_knownChainStates;

  PluginWatcher *m_pPlugWatcher;

  PlugWindowManager *m_pWindowManager;

  int m_projectChangedConnectionId;
};
