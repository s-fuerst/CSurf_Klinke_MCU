/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once

#include <boost/array.hpp>
#include "csurf_mcu.h"
#include <boost/scoped_ptr.hpp>
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but

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

  // Parameter
  String getParamNameShort(ElementDesc::eType type, int channel) {
    return getPMParam(type, channel) ? getPMParam(type, channel)->getNameShort()
                                     : String::empty;
  }
  String getParamNameLong(ElementDesc::eType type, int channel) {
    return getPMParam(type, channel) ? getPMParam(type, channel)->getNameLong()
                                     : String::empty;
  }
  void setParamValueInt(
      ElementDesc::eType type, int channel,
      int value); // channel is 0 based, value is a 14bit unsigned int
  void setParamValueDouble(ElementDesc::eType type, int channel,
                           double value); // channel is 0 based, value is
                                          // directly forwarded to reaper
  int getParamValueInt(
      ElementDesc::eType type,
      int channel = 0); // channel is 0 based, return value is 14bit unsigned,
                        // returns 0 if element hasn't an assigned id
  double getParamValueDouble(
      ElementDesc::eType type,
      int channel); // channel is 0 based, returns unconverted value, returns 0
                    // if element hasn't an assigned id
  double getParamValueDouble(ElementDesc* desc);
  PMVPot::tSteps *getParamSteps(
      int vpot); // ElementDesc can be only VPOT, so vpot parameter is enough
  String getParamValueShort(ElementDesc::eType type, int channel);
  String getParamValueLong(ElementDesc::eType type, int channel);

  int getParamID(ElementDesc *element);
  int getParamID(ElementDesc::eType type, int channel) {
    boost::scoped_ptr<PlugAccess::ElementDesc> pDesc(
        new ElementDesc(this, type, channel));
    return getParamID(pDesc.get());
  }
  // Bank
  void setSelectedBank(int bank);
  int getSelectedBank() { return m_selectedBank; }
  String getBankNameLong(int bank) {
    return getMap()->getBank(bank)->getNameLong();
  }
  String getBankNameShort(int bank) {
    return getMap()->getBank(bank)->getNameShort();
  }
  bool isBankUsed(int bank) {
    return getMap()->getBank(bank)->isUsed();
  } // 0 based

  // Page
  void setSelectedPage(int bank, int page);
  void setSelectedPageInSelectedBank(int page);
  int getSelectedPageInSelectedBank() { return m_selectedPage[m_selectedBank]; }
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

  bool isOptionSetTo(wchar_t *optionName, wchar_t *attribute) {
    return m_pMode->getOptions()->isOptionSetTo(optionName, attribute);
  }
  void checkChain(MediaTrack *pTrack);
  String m_plugName; // used in accessPlugin so that the name must not accessed
                     // for saving the slotState (it's possible that the track
                     // doesn't exist anymore)

  int m_selectedBank;
  boost::array<int, 8>
      m_selectedPage; // for each bank we store the last selected page

  MediaTrack *m_pPlugTrack; // can be NULL
  GUID m_GUIDplugTrack; // GetTrackGUID only works as long as the track is in
                        // the actual project. As a workaround a store the GUID
                        // when i assign a new track to m_pPlugTrack

  int m_iSlot; // 0 based
  void writeSelectedPlugToProjectConfig(XmlElement *pPlugAccessNode);
  void readSelectedPlugFromProjectConfig(XmlElement *pPlugAccessNode, bool changeTriggeredFromProjectChange = false);

  PlugMode *m_pMode;
  PlugMapManager *m_pMapManager;

  typedef boost::tuple<String, int, boost::array<int, 8>> tSlotState;
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
