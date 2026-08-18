/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once

#include "boost/signals2.hpp"
#include<list>
#include<set>
#include<vector>
#include<map>
#include<unordered_map>
#include<memory>
#include <cassert>
#include "JuceHeader.h"
#include "ProjectConfig.h"
#include "csurf_mcu.h"
#include "McuAssert.h"

using boost::signals2::connection;

class MediaTrack;
class Tracks;
class Options;
class DisplayHandler;
class CSurf_MCU;

// Track Structure Node
//======================================================== TSNode

class TSNode {
  friend class Tracks;

public:
  enum EFilter { OFF = 0, TCP, MCP, MCU };

  TSNode(MediaTrack *pMT, TSNode *pParent);

  void addChild(TSNode *pChild);
  bool isRoot() { return m_pMediaTrack == NULL; }

  MediaTrack *getMediaTrack() { return m_pMediaTrack; }
  TSNode *getParentNode() { return m_pParent; }
	std::vector<TSNode *> getChildren() { return m_children; }
  TSNode *getNextNodeOnSameLevel(EFilter fitler);

  int getDepth();

  static bool showTrack(MediaTrack *pMT, EFilter filter);

private:
  bool hasChilds(EFilter filter);
  int numChilds(EFilter filter);
  TSNode *getNthChild(
      EFilter filter,
      int n); // returns NULL if not n childs are shown, counting starts by 1
  int getChildNumber(EFilter filter); // inverse function to getNthChild

  MediaTrack *m_pMediaTrack;
  TSNode *m_pParent;
  std::vector<TSNode *> m_children;
};

//======================================================== TSGraph

// Anchor channels are not part of the graph
class TSGraph {
public:
  void buildGraph(bool flat);

  TSNode *getRootNode();
  TSNode *nodeOfTrack(MediaTrack *pTM);

  bool trackExists(MediaTrack *pMT);

private:
  typedef std::map<MediaTrack *, TSNode *> tTrack2Node;
  tTrack2Node m_mapTrack;
};

//======================================================== Tracks

class TrackState {
public:
  TrackState();
  TrackState(MediaTrack *pMT);
  //      TrackState(TrackState& other);
  //
  //      TrackState& operator=(TrackState& other);
  bool operator==(TrackState &other);

  MediaTrack *getMediaTrack() { return m_pMediaTrack; }
  void setMediaTrack(MediaTrack *pMT) {
    assert(pMT != NULL);
    m_pMediaTrack = pMT;
  }

  bool isOnMCU() { return m_isOnMCU; }
  int getOnMCUChannel() { return m_onMCUChannel; }

  void setIsOnMCUChannel(int channel) {
    m_isOnMCU = (channel > 0) ? true : false;
    m_onMCUChannel = channel;
  }

  bool isInSet() { return m_isInSet; }
  void setIsInSet(bool inSet) { m_isInSet = inSet; }

  int getAnchorChannel() { return m_anchorChannel; }
  void setAnchorChannel(int channel);

  int getQuickJumpChannel() { return m_quickJumpChannel; }
  void setQuickJumpChannel(int channel) { m_quickJumpChannel = channel; }

  String getQuickJumpName() { return m_quickJumpName; }
  void setQuickJumpName(String &name) { m_quickJumpName = name; }
  String showQuickNameInDisplay();

  bool useAsRootInQuick() { return m_quickRoot; }
  void setUseAsRootInQuick(bool quickRoot) { m_quickRoot = quickRoot; }

  bool isShownInTCP() { return m_isShownInTCP; }
  void setIsShownInTCP(bool isShown) { m_isShownInTCP = isShown; }

  bool isShownInMCP() { return m_isShownInMCP; }
  void setIsShownInMCP(bool isShown) { m_isShownInMCP = isShown; }

	bool getVUactive() { return m_vu; }
	void setVUactive(bool active) { m_vu = active; }

  int getTCPHeight() { return m_tcpHeight; }
  void setTCPHeight(int height) { m_tcpHeight = height; }

  String getDisplayName() { return m_displayName; }
  void setDisplayName(String &displayName) { m_displayName = displayName; }
  String showInDisplay();

  String getGuidAsString() { return m_guidAsString; }

  void writeTrackStatesToProjectConfig(XmlElement *pNode);
  void readTrackStatesFromProjectConfig(XmlElement *pNode);

private:
  String m_guidAsString;
  MediaTrack *m_pMediaTrack;
  bool m_isOnMCU;
  int m_onMCUChannel;
  bool m_isInSet;
  int m_anchorChannel;    // 0 means: isn't an anchor
  int m_quickJumpChannel; // 0 means: isn't a quick jump target
  String m_quickJumpName;
  bool m_quickRoot; // if true, the track is the new base track (its child are
                    // shown, not the track itself)
  bool m_isShownInTCP; // if tcp isn't modified from plugin
  bool m_isShownInMCP; // if mcp isn't modified from plugin
	bool m_vu; // is true if vu should be shown on meterbridge
  int m_tcpHeight;
  String m_displayName;
};

//======================================================== MediaTrackInfo

class MediaTrackInfo {
public:
  static bool isShownInTCP(MediaTrack *pMT);
  static void showInTCP(MediaTrack *pMT, bool bShow);

  static bool isShownInMCP(MediaTrack *pMT);
  static void showInMCP(MediaTrack *pMT, bool bShow);

  static int getHeight(MediaTrack *pMT);
  static void setHeight(MediaTrack *pMT, int height);

  static int getTrackNr(MediaTrack *pMT);

  static bool testPtr(char *pName);
  static String getTrackName(MediaTrack *pMT, bool showTrackNumberIfEmpty);
  static void setTrackName(MediaTrack *pMT, String strTrackname);

  static Colour getTrackColor(MediaTrack *pMT);
};

//======================================================== Tracks

class Tracks {
public:
  typedef boost::signals2::signal<void(MediaTrack *)> tTrackSignal;
  typedef tTrackSignal::slot_type tTrackSignalSlot;

  static Tracks *instance();
  virtual ~Tracks(void);

  void selectionChanged();
  void updateSelection(MediaTrack *pMT, bool selected);
  bool tracksStatesChanged(bool checkProjectChange = true);
  MediaTrack *getSelectedSingleTrack(bool includeMaster = false);

  int getChannelForMediaTrack(
      MediaTrack *pMT); // returns -1 if MediaTrack isn't mapped to a channel
  MediaTrack *getMediaTrackForChannel(int channel);
  MediaTrack *getMediaTrackForGUID(String guid);

	MediaTrack *getParentForMediaTrack(MediaTrack *pMT);
	std::vector<MediaTrack *> getChildredForMediaTrack(MediaTrack *pMT);
	
  int getNumMediaTracksOnMCU();

  int getNumMediaTracksTotal();

	 // can return NULL while adding multiple tracks at once
  TrackState *getTrackStateForMediaTrack(MediaTrack *pMediaTrack);

  int connect2TrackAddedSignal(
      const tTrackSignalSlot &slot); // {signalTrackAdded.connect(slot);}
  void disconnectTrackAdded(int connectionId);

  int connect2TrackRemovedSignal(
      const tTrackSignalSlot &slot); // {signalTrackRemoved.connect(slot);}
  void disconnectTrackRemoved(int connectionId);

  void setDisplayHandler(DisplayHandler *pDH);

  Options *getOptions() {
    ASSERT(m_pOptions1);
    return m_pOptions1;
  }
  Options *get2ndOptions() {
    ASSERT(m_pOptions2);
    return m_pOptions2;
  }

  bool moveBaseTrack(MediaTrack *pMT);
  bool moveBaseTrackToParent();

  bool moveTrackToLeftMostChannel(MediaTrack *pMT);

  bool hasChilds(MediaTrack *pMT);
  bool baseTrackHasParent();

  MediaTrack *getBaseTrack() { return m_pCurrentBaseTrack; }

  TSNode::EFilter getFilter();

  void adjust(int numMCUChannels);

  void setTCP2TrackStates();
  void setMCP2TrackStates();

  int getNumberOfAnchors();
  int getNumberOfActiveAnchors(int maxChannel = -1);

  void setMCU(CSurf_MCU *pMCU) { m_pMCU = pMCU; }
  // extreme ugly hack for the TrackStatesTableComponent. This can't work with
  // the extender (like all the other m_pMCU access in the Tracks singleton).
  CSurf_MCU *getMCU() { return m_pMCU; }

  void buildGraph();

  void projectChanged(XmlElement *pXmlElement, ProjectConfig::EAction action);

  int getGlobalOffset() { return m_globalOffset; }
  bool setGlobalOffset(int globalOffset);

  int getNumberOfChannelStrips();

  // banking helpers
  int getEffectiveBankStep();
  int getLegacyPageStep();
  int getMaxUsefulGlobalOffset();
  int clampGlobalOffset(int offset);
  bool clampCurrentGlobalOffset();
  void moveSelectedTrack2MCU();

  void dumpMappingState();

  bool isTrackInFilter(MediaTrack *pMT);

	void updateVUactive();

	MediaTrack *getMasterTrack() { return getMediaTrackForChannel(0); }
	
private:
  Tracks(void);
  static Tracks *s_instance;

  void updateTrackStates(int numMCUChannels);
  int calcNumShownTracks();
  bool shouldTrackInTCP(TrackState *pMediaTrack);
  int calcTCPSizeInPixel();

  void createChannelTrackVector();
  MediaTrack *findMediaTrackForChannel(int channel);
  // like findMediaTrackForChannel(), but also resolves channels beyond
  // m_numMCUChannels (needed for QuickJump targets outside the visible range)
  MediaTrack *findMediaTrackForChannelUnlimited(int channel);
  std::set<MediaTrack *> m_selectedTracks;

	bool oneChildrenIsSoloed(MediaTrack *pMT);
	void activeVUallChildren(MediaTrack *pMT);
	
  typedef std::map<String, TrackState *> tTrackStates;
  tTrackStates m_trackStates;

  // O(1) secondary index: MediaTrack* -> TrackState*. Kept in lockstep with
  // m_trackStates at every insert/erase so getTrackStateForMediaTrack() is
  // constant-time. It used to linearly scan m_trackStates, which made every
  // call O(n); since TSGraph::buildGraph() calls it once per track, the graph
  // rebuild was O(n^2) and adding a single track got progressively slower as
  // the project grew. The GUID map remains the source of truth for the rare
  // project-restore case where REAPER changes the pointer but keeps the GUID.
  typedef std::unordered_map<MediaTrack *, TrackState *> tTracksByPointer;
  tTracksByPointer m_tracksByPointer;

  typedef ::std::vector<MediaTrack *> tTracks;
  tTracks m_channelTracks;

  // Vector (not set) so element order reflects REAPER track order. This lets
  // operator== detect adds, removes, and reorders in one O(n) comparison,
  // which is used to skip the O(n^2) buildGraph() on unchanged frames.
  typedef std::vector<MediaTrack *> tTrackSet;
  tTrackSet *m_pAllTracksBefore;
  tTrackSet *m_pAllTracksNow;

  tTrackSignal signalTrackAdded;
  tTrackSignal signalTrackRemoved;

  std::map<int, connection> m_trackAddedConnections;
  std::map<int, connection> m_trackRemovedConnections;
  int m_nextConnectionId;

  int m_projectChangedConnectionId;

  int m_globalOffset;
  int m_numMCUChannels;

  TSGraph m_structure;
	TSGraph m_structureVU;
	
  MediaTrack *m_pCurrentBaseTrack;

  MediaTrack *m_pLastSelectedSingleTrack;

  Options *m_pOptions1;
  Options *m_pOptions2;

  CSurf_MCU *m_pMCU;
};
