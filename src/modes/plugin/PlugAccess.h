/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once

#include <boost/array.hpp>
#include <cstdint>
#include "csurf_mcu.h"
#include <boost/scoped_ptr.hpp>
#include "JuceHeader.h"

#include "PlugMap.h"
#include "PlugMode.h"
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <map>
#include <set>
#include <vector>

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
      // guard: getSelectedBank() reads the active unit's state.
      // If m_activeUnit is stale, resolution silently targets the wrong unit.
      ASSERT(pPA->getActiveUnit() >= 0 && pPA->getActiveUnit() < MAX_SURFACE_UNITS);
      m_bank = pPA->getSelectedBank();
      m_page = pPA->getSelectedPageInSelectedBank();
      m_type = type;
      m_channel = channel;
      m_offset = 0;
    }

    bool isValid() {
      if (m_type == UNKNOWN || m_channel < 0 || m_channel >= 8)
        return false;
      // Bank/page only apply to mapped elements (FADER/VPOT). The REAPER
      // pseudo-params (DRYWET/BYPASS/DELTA) resolve via param idents, so
      // they must stay addressable even when the active unit is empty
      // (getSelectedPageInSelectedBank() returns -1 in that case).
      if (m_type == FADER || m_type == VPOT)
        return (m_bank >= 0 && m_bank < 8 && m_page >= 0 && m_page < 8);
      return true;
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

  // ---- explicit Bank/Page overloads ----
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
    if (u < 0 || u >= MAX_SURFACE_UNITS)
      return 0;
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
    if (u < 0 || u >= MAX_SURFACE_UNITS)
      return -1;
    if (m_unitEmpty[u])
      return -1;
    int bank = m_selectedBankPerUnit[u];
    if (bank < 0 || bank >= 8)
      return -1;
    return m_selectedPagePerUnit[u][bank];
  }
  String getPageNameLongInSelectedBank(int page) {
    if (page < 0)
      return String();
    return getMap()
        ->getBank(resolveBankReference())
        ->getPage(page)
        ->getNameLong();
  }
  String getPageNameShortInSelectedBank(int page) {
    if (page < 0)
      return String();
    return getMap()
        ->getBank(resolveBankReference())
        ->getPage(page)
        ->getNameShort();
  }
  bool isPageUsed(int bank, int page) {
    return getMap()->getBank(bank)->getPage(page)->isUsed();
  }
  bool isPageUsedInSelectedBank(int page); // 0 based

  // ---- per-unit accessors ----
  int  selectedBankForUnit(int u) const {
    return u >= 0 && u < MAX_SURFACE_UNITS ? m_selectedBankPerUnit[u] : 0;
  }
  int  selectedPageForUnit(int u) const {
    if (u < 0 || u >= MAX_SURFACE_UNITS)
      return -1;
    // A unit without a page (empty) shows -1 regardless of its bank.
    int bank = m_selectedBankPerUnit[u];
    return m_unitEmpty[u] || bank < 0 || bank >= 8
               ? -1
               : m_selectedPagePerUnit[u][bank];
  }
  int  selectedPageForUnit(int u, int bank) const {
    return u < 0 || u >= MAX_SURFACE_UNITS || bank < 0 || bank >= 8 ||
                   m_unitEmpty[u]
               ? -1
               : m_selectedPagePerUnit[u][bank];
  }
  void setSelectedBank(int bank, int unit);
  void setSelectedPage(int bank, int page, int unit);
  void setSelectedPageInSelectedBank(int page, int unit);
  int  getActiveUnit() const { return m_pMode->getActiveUnit(); }

  // ---- empty-unit state (page uniqueness) ----
  // A unit is "empty" when it shows no page: either the plugin has more
  // units than used pages in the selected bank, or a page was moved to
  // another unit. Empty units stay fully usable for bank/page selection.
  bool isUnitEmpty(int u) const {
    return u >= 0 && u < MAX_SURFACE_UNITS && m_unitEmpty[u];
  }
  void setUnitEmpty(int u, bool empty) {
    if (u < 0 || u >= MAX_SURFACE_UNITS)
      return;
    m_unitEmpty[u] = empty;
  }
  void clearUnitPage(int u) { setUnitEmpty(u, true); }
  // Enforce the page-uniqueness invariant: no two units may display the same
  // (bank, page). After a change on `changedUnit` that unit keeps its page;
  // every other unit showing the same (bank, page) becomes empty. Pass -1 to
  // keep the lowest unit index on collisions (used after state restore).
  void enforceUniquePages(int changedUnit);
  // First used page of `bank` that no other unit currently displays, or -1
  // when every used page of that bank is taken. Used to auto-fill an empty
  // unit when its bank is changed (SOLO).
  int firstFreePageForUnit(int bank, int unit);

  // ---- used-page-sequence helpers ----
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

  // Fills pSteps with the discrete values of a parameter and returns the
  // number of entries added (0 = the parameter was not detected as discrete
  // or the FX provided no usable value names; the map stays empty in that
  // case, and the parameter must not be treated as discrete). Used by the
  // map editor (parameter assignment and Learn) and by the automatic
  // default map creation. Tries two strategies in order:
  // 1. The FX reports a step size: the exact value grid is used (a toggle
  //    uses exactly its two endpoint values instead, never its grid).
  // 2. Heuristic over the displayed value names, with verification
  //    (see fillStepsByValueNameScan).
  // After both strategies a generated three-entry table whose middle entry
  // shows the same name as the first or the last entry is reduced to two
  // entries: the middle position displays one of the two real values and
  // is not a value of its own.
  static int fillDiscreteSteps(MediaTrack *pTrack, int slot, int paramId,
                               PMVPot::tSteps *pSteps);

  // Rewrites the short names of all assigned parameters of the current map
  // so that they can be distinguished on the 6-character display (see
  // disambiguateShortNames). Parameters whose short name was edited (it is
  // no longer the plain truncation of the long name) keep their short name,
  // and their name is reserved against the new abbreviations. Returns true
  // if a short name was changed. Called after a parameter name was taken
  // over from the FX (Learn or parameter selection) and at the end of the
  // automatic map creation.
  bool disambiguateShortNamesInMap();

  void projectChanged(XmlElement *pXmlElement, ProjectConfig::EAction action);

  void createDefaultMap();

private:
  // --- discrete parameter helpers (used by fillDiscreteSteps) ---

  // Display name of a normalized (0..1) parameter value, or an empty string
  // if the FX cannot format its parameter values.
  static String formattedValueName(MediaTrack *pTrack, int slot, int paramId,
                                   double normalizedValue);

  // Number of discrete value segments of the parameter, or 0 if the
  // parameter is continuous or the step size is unknown/unusable.
  // A parameter counts as discrete if the FX reports a step size that
  // divides the parameter range into 2..maxSegments segments. Toggles are
  // handled separately by fillStepsFromStepGrid (a toggle always has
  // exactly two values and no usable step grid), so this function ignores
  // the toggle flag.
  static int discreteStepSegments(MediaTrack *pTrack, int slot, int paramId,
                                  int maxSegments = 100);

  // Fills pSteps from the parameter's step-size grid, converted to raw
  // parameter values. Only values with a display name from the FX are
  // added. A toggle is filled with exactly its two endpoint values
  // instead: the step size that REAPER reports for toggles as well would
  // produce intermediate grid positions that display one of the two real
  // names. Returns the number of entries added.
  static int fillStepsFromStepGrid(MediaTrack *pTrack, int slot, int paramId,
                                   PMVPot::tSteps *pSteps);

  // Heuristic fallback: if the FX displays value names and the names of the
  // values 0.00 and 0.01 do not differ, the normalized range 0.00..1.00 is
  // scanned in 0.01 steps for changing names. The collected values are
  // distributed evenly over the parameter range (first value at the
  // minimum, last at the maximum) and it is verified that the parameter
  // really displays the collected names at these positions. On any
  // mismatch nothing is added. Returns the number of entries added.
  static int fillStepsByValueNameScan(MediaTrack *pTrack, int slot,
                                      int paramId, PMVPot::tSteps *pSteps);

  // Rewrites the short names of the steps in pSteps so that they are unique
  // within the table (see disambiguateShortNames).
  static void disambiguateStepShortNames(PMVPot::tSteps *pSteps);

  // Computes unique short names for the given long names. width is the
  // number of characters the names are built and compared with: 6 for
  // V-Pots and for faders when no QCon Pro X unit is present, 5 for faders
  // when a QCon Pro X is part of the surface (its fader display shows only
  // five characters). Names whose plain truncation is unique (compared
  // case-insensitively and ignoring trailing spaces) keep the truncation.
  // For names whose truncation collides, the names are contracted word by
  // word with the vowels removed ("Drive Level" -> "DrvLvl"); a name that
  // ends with a number keeps that number complete ("Band Transient 12" ->
  // "BndT12"). Returns a map from long name to short name; names that
  // cannot be disambiguated keep the naive truncation.
  static std::map<String, String>
  disambiguateShortNames(const std::vector<String> &longNames, int width);

  // Applies the disambiguated short names of one element group (faders or
  // V-Pots) to the given parameters; manually edited short names are not
  // rewritten but reserved. Returns true if a short name was changed.
  static bool applyDisambiguatedShortNames(std::vector<PMParam *> &params,
                                           std::vector<String> &longNames,
                                           int width);

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

  // per-unit state.  Index 0..MAX_SURFACE_UNITS-1.
  // m_selectedBankPerUnit[u] = selected bank for unit u, -1 = unassigned.
  boost::array<int, MAX_SURFACE_UNITS> m_selectedBankPerUnit;
  // m_selectedPagePerUnit[u][bank] = selected page in bank for unit u.
  boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS> m_selectedPagePerUnit;
  // per-unit empty flag: true while the unit shows no page.
  boost::array<bool, MAX_SURFACE_UNITS> m_unitEmpty;

  MediaTrack *m_pPlugTrack; // can be NULL
  GUID m_GUIDplugTrack; // GetTrackGUID only works as long as the track is in
                        // the actual project. As a workaround a store the GUID
                        // when i assign a new track to m_pPlugTrack

  int m_iSlot; // 0 based
  void writeSelectedPlugToProjectConfig(XmlElement *pPlugAccessNode);
  void readSelectedPlugFromProjectConfig(XmlElement *pPlugAccessNode, bool changeTriggeredFromProjectChange = false);

  PlugMode *m_pMode;
  PlugMapManager *m_pMapManager;

  // widened tSlotState — per-unit banks + pages + empty flags.
  // Format: (plugName, banksPerUnit[], pagesPerUnit[][], emptyPerUnit[])
  typedef boost::tuple<String,
                       boost::array<int, MAX_SURFACE_UNITS>,
                       boost::array<boost::array<int, 8>, MAX_SURFACE_UNITS>,
                       boost::array<bool, MAX_SURFACE_UNITS>>
      tSlotState;
  typedef boost::tuple<String /* GUID */, int> tSlotLocation;
  typedef std::pair<const tSlotLocation, tSlotState> tSlotStatePair;
  typedef std::map<tSlotLocation, tSlotState> tSlotStatesMap;
  tSlotStatesMap m_knownSlotStates;
  void writeSlotStatesToProjectConfig(XmlElement *pNode);
  void readSlotStatesFromProjectConfig(XmlElement *pNode);
  typedef std::map<std::uintptr_t, int> tTrack2Plug;
  tTrack2Plug m_track2Slot;

  typedef std::map<boost::tuple<MediaTrack *, int>, HWND> tWindowStates;
  tWindowStates m_knownWndStates;

  typedef std::map<MediaTrack *, int> tChainSlot;
  tChainSlot m_knownChainStates;

  PluginWatcher *m_pPlugWatcher;

  PlugWindowManager *m_pWindowManager;

  int m_projectChangedConnectionId;
};
