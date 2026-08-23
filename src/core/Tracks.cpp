/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "Tracks.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include "McuAssert.h"
#include "McuDebugLog.h"
#include "MultiTrackOptions.h"
#include "MultiTrackOptions2.h"

Tracks* Tracks::s_instance = NULL;

//======================================================== TSNode

TSNode::TSNode(MediaTrack *pMT, TSNode *pParent)
  : m_pMediaTrack(pMT), m_pParent(pParent) {}

void TSNode::addChild(TSNode *pChild) { m_children.push_back(pChild); }

bool TSNode::hasChilds(EFilter filter) {
  if (m_children.size() == 0)
    return false;

  BOOST_FOREACH (TSNode *&pNode, m_children) {
    if (showTrack(pNode->getMediaTrack(), filter))
      return true;
  }

  return false;
}

int TSNode::numChilds(EFilter filter) {
  if (m_children.size() == 0)
    return 0;

  int num = 0;
  BOOST_FOREACH (TSNode *&pNode, m_children) {
    if (showTrack(pNode->getMediaTrack(), filter))
      num++;
  }

  return num;
}

bool TSNode::showTrack(MediaTrack *pMT, EFilter filter) {
  switch (filter) {
  case OFF:
    return true;
  case TCP:
    {
      bool *shown = (bool *)GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", NULL);
      return shown && *shown;
    }
  case MCP:
    {
      bool *shown = (bool *)GetSetMediaTrackInfo(pMT, "B_SHOWINMIXER", NULL);
      return shown && *shown;
    }
  case MCU:
    return Tracks::instance()->getTrackStateForMediaTrack(pMT) &&
      Tracks::instance()->getTrackStateForMediaTrack(pMT)->isInSet();
  }

  return true;
}

TSNode *TSNode::getNthChild(EFilter filter, int n) {
  if (hasChilds(filter)) {
    int i = 0;
    BOOST_FOREACH (TSNode *&pNode, m_children) {
      if (showTrack(pNode->getMediaTrack(), filter)) {
        if (++i == n)
          return pNode;
      }
    }
  }

  return NULL;
}

// returns the position of the child regarding the other nodes with the same
// parent (it's the inverse function to getNthChild) 1 means it's the
// "firstborn". 0 is returned if pNode is the Root
int TSNode::getChildNumber(EFilter filter) {
  if (m_pMediaTrack == NULL) {
    return 0;
  }

  int numChilds = m_pParent->numChilds(filter);
  for (int i = 1; i <= numChilds; i++) {
    if (m_pParent->getNthChild(filter, i) == this)
      return i;
  }

  return 0;
}

// Anchor are not part graph, so this doesn't work for them
TSNode *TSNode::getNextNodeOnSameLevel(EFilter filter) {
  int nr = getChildNumber(filter);
  return (nr > 0) ? getNthChild(filter, getChildNumber(filter) + 1) : NULL;
}

int TSNode::getDepth() {
  int d = 0;
  TSNode *pNode = this;
  while (pNode != NULL) {
    pNode = pNode->getParentNode();
    d++;
  }
  return d;
}

//======================================================== TSGraph

void TSGraph::buildGraph(bool flat) {
  BOOST_FOREACH (tTrack2Node::value_type &t2n, m_mapTrack)
    delete (t2n.second);
  m_mapTrack.clear();

  m_mapTrack[0] = new TSNode(NULL, NULL);
  TSNode *pParentNode = m_mapTrack[0];

  bool anchors = Tracks::instance()->getOptions()->isOptionSetTo(MTO_DISABLE_ANCHORS,
								 MTOA_ANCHORS_YES);

  for (TrackIterator ti; !ti.end(); ++ti) {
    TSNode *pNewNode = new TSNode(*ti, pParentNode);
    m_mapTrack[*ti] = pNewNode;

    if (Tracks::instance()->getTrackStateForMediaTrack(*ti) && !(anchors &&
								 Tracks::instance()->getTrackStateForMediaTrack(*ti)->getAnchorChannel() > 0)) 
      pParentNode->addChild(pNewNode);

    if (flat) // ignore tree structure and add everything to the root (NULL)
      continue;

    int *pFD = (int *)GetSetMediaTrackInfo(*ti, "I_FOLDERDEPTH", NULL);
    if (!pFD)
      continue;
    if (*pFD == 1) {
      pParentNode = pNewNode;
    } else if (*pFD < 0) {
      for (int i = 0; i < -(*pFD); i++) {
        // it's possible to set Reaper in a state, where a track in a
        // root-folder returns a negative folder depth. That must be ignored.
        if (pParentNode->getParentNode() != NULL) {
          pParentNode = pParentNode->getParentNode();
        }
      }
    }
  }
}

TSNode *TSGraph::getRootNode() { return m_mapTrack[NULL]; }

bool TSGraph::trackExists(MediaTrack *pMT) {
  if (m_mapTrack.size() == 0)
    return false;

  return (m_mapTrack.find(pMT) != m_mapTrack.end());
}

TSNode *TSGraph::nodeOfTrack(MediaTrack *pMT) {
  if (pMT == NULL)
    return getRootNode();

  if (trackExists(pMT))
    return m_mapTrack[pMT];

  return NULL;
}

//======================================================== TracksState

TrackState::TrackState()
  : m_pMediaTrack(NULL), m_isInSet(true), m_isOnMCU(false), m_onMCUChannel(0),
    m_anchorChannel(0), m_quickJumpChannel(0), m_isShownInMCP(true),
    m_isShownInTCP(true), m_tcpHeight(16), m_quickJumpName(String()),
    m_quickRoot(false), m_displayName(String()), m_vu(true),
    m_guidAsString(String()) {}

TrackState::TrackState(MediaTrack *pMT)
  : m_pMediaTrack(NULL), m_isInSet(true), m_isOnMCU(false), m_onMCUChannel(0),
    m_anchorChannel(0), m_quickJumpChannel(0), m_isShownInMCP(true),
    m_isShownInTCP(true), m_tcpHeight(16), m_quickJumpName(String()),
    m_quickRoot(false), m_vu(true), m_displayName(String()) {
  m_pMediaTrack = pMT;
  if (pMT)
    m_guidAsString = GUID2String(GetTrackGUID(pMT));
  else
    m_guidAsString = String();
}

bool TrackState::operator==(TrackState &other) {
  if (m_pMediaTrack != other.getMediaTrack())
    return false;
  if (m_isInSet != other.isInSet())
    return false;
  if (m_isOnMCU != other.isOnMCU())
    return false;
  if (m_onMCUChannel != other.getOnMCUChannel())
    return false;
  if (m_anchorChannel != other.getAnchorChannel())
    return false;
  if (m_quickJumpChannel != other.getQuickJumpChannel())
    return false;
  if (m_quickJumpName != other.getQuickJumpName())
    return false;
  if (m_displayName != other.getDisplayName())
    return false;
  if (m_isShownInMCP != other.isShownInMCP())
    return false;
  if (m_isShownInTCP != other.isShownInTCP())
    return false;
  if (m_quickRoot != other.useAsRootInQuick())
    return false;
  if (m_tcpHeight != other.getTCPHeight())
    return false;

  return true;
}

String TrackState::showInDisplay() {
  if (m_displayName.isNotEmpty()) {
    return m_displayName;
  } else {
    String fullTrackName = MediaTrackInfo::getTrackName(m_pMediaTrack, true);
    if (fullTrackName.contains(String("|"))) {
      return fullTrackName.fromFirstOccurrenceOf(String("|"), false, false)
	.trimStart()
	.substring(0, 6);
    }
    return fullTrackName.substring(0, 6);
  }
}

String TrackState::showQuickNameInDisplay() {
  if (m_quickJumpName.isNotEmpty())
    return m_quickJumpName;

  return showInDisplay();
}

#define TRACKSTATE_NODE_SINGLE_STATE String("STATE")
#define TRACKSTATE_ATT_TRACK String("track") // GUID as String
#define TRACKSTATE_ATT_NAME String("name")
#define TRACKSTATE_ATT_TCP String("tcp")
#define TRACKSTATE_ATT_MCP String("mcp")
#define TRACKSTATE_ATT_MCU String("mcu")
#define TRACKSTATE_ATT_ANCHOR String("anchor")
#define TRACKSTATE_ATT_QUICK_JUMP String("q_channel")
#define TRACKSTATE_ATT_QUICK_NAME String("q_name")
#define TRACKSTATE_ATT_QUICK_ROOT String("q_root")

void TrackState::writeTrackStatesToProjectConfig(XmlElement *pNode) {
  XmlElement *pStateNode = new XmlElement(TRACKSTATE_NODE_SINGLE_STATE);

  pStateNode->setAttribute(TRACKSTATE_ATT_TRACK, m_guidAsString);
  pStateNode->setAttribute(TRACKSTATE_ATT_NAME, m_displayName);
  pStateNode->setAttribute(TRACKSTATE_ATT_TCP, m_isShownInTCP);
  pStateNode->setAttribute(TRACKSTATE_ATT_MCP, m_isShownInMCP);
  pStateNode->setAttribute(TRACKSTATE_ATT_MCU, m_isInSet);
  pStateNode->setAttribute(TRACKSTATE_ATT_ANCHOR, m_anchorChannel);
  pStateNode->setAttribute(TRACKSTATE_ATT_QUICK_JUMP, m_quickJumpChannel);
  pStateNode->setAttribute(TRACKSTATE_ATT_QUICK_NAME, m_quickJumpName);
  pStateNode->setAttribute(TRACKSTATE_ATT_QUICK_ROOT, m_quickRoot);

  pNode->addChildElement(pStateNode);
}

void TrackState::readTrackStatesFromProjectConfig(XmlElement *pNode) {
  m_guidAsString = pNode->getStringAttribute(TRACKSTATE_ATT_TRACK);
  m_pMediaTrack = Tracks::instance()->getMediaTrackForGUID(m_guidAsString);
  m_displayName = pNode->getStringAttribute(TRACKSTATE_ATT_NAME);
  m_isShownInTCP = pNode->getBoolAttribute(TRACKSTATE_ATT_TCP);
  m_isShownInMCP = pNode->getBoolAttribute(TRACKSTATE_ATT_MCP);
  m_isInSet = pNode->getBoolAttribute(TRACKSTATE_ATT_MCU);
  m_anchorChannel = pNode->getIntAttribute(TRACKSTATE_ATT_ANCHOR);
  m_quickJumpChannel = pNode->getIntAttribute(TRACKSTATE_ATT_QUICK_JUMP);
  m_quickJumpName = pNode->getStringAttribute(TRACKSTATE_ATT_QUICK_NAME);
  m_quickRoot = pNode->getBoolAttribute(TRACKSTATE_ATT_QUICK_ROOT);
}

void TrackState::setAnchorChannel(int channel) {
  if (m_anchorChannel != channel) {
    m_anchorChannel = channel;
  }
}

//======================================================== MediaTrackInfo
 
bool MediaTrackInfo::isShownInTCP(MediaTrack *pMT) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return false;
  bool *bShown = (bool *)GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", NULL);
  return bShown ? *bShown : false;
}

bool MediaTrackInfo::isShownInMCP(MediaTrack *pMT) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return false;
  bool *bShown = (bool *)GetSetMediaTrackInfo(pMT, "B_SHOWINMIXER", NULL);
  return bShown ? *bShown : false;
}

void MediaTrackInfo::showInTCP(MediaTrack *pMT, bool bShow) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return;
  GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", &bShow);
  // workaround to update tcp
  TrackList_AdjustWindows(true);
  UpdateTimeline();
}

void MediaTrackInfo::showInMCP(MediaTrack *pMT, bool bShow) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return;
  GetSetMediaTrackInfo(pMT, "B_SHOWINMIXER", &bShow);
}

int MediaTrackInfo::getHeight(MediaTrack *pMT) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return 0;
  int *pHeight = (int *)GetSetMediaTrackInfo(pMT, "I_WNDH", NULL);
  return pHeight ? *pHeight : 0;
}

void MediaTrackInfo::setHeight(MediaTrack *pMT, int height) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return;
  GetSetMediaTrackInfo(pMT, "I_HEIGHTOVERRIDE", &height);
}

int MediaTrackInfo::getTrackNr(MediaTrack *pMT) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return 0;
  return (int)(intptr_t)GetSetMediaTrackInfo(pMT, "IP_TRACKNUMBER", NULL);
}

bool MediaTrackInfo::testPtr(char *pName) {
  return pName != NULL;
}

String MediaTrackInfo::getTrackName(MediaTrack *pMT,
                                    bool showTrackNumberIfEmpty) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return String();
  char *pName = (char *)GetSetMediaTrackInfo(pMT, "P_NAME", NULL);
  if (pName == NULL)
    return String();
  if (testPtr(pName) && pName && *pName != 0) {
    return String(pName);
  }

  if (!showTrackNumberIfEmpty) {
    return String();
  }

  int nr = (int)(intptr_t)GetSetMediaTrackInfo(pMT, "IP_TRACKNUMBER", NULL);
  return String(nr);
}

void MediaTrackInfo::setTrackName(MediaTrack *pMT, String strTrackname) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return;
  const char *pName = strTrackname.toRawUTF8();
  GetSetMediaTrackInfo(pMT, "P_NAME", (void *)pName);
}

Colour MediaTrackInfo::getTrackColor(MediaTrack *pMT) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return Colours::black;
  int *pColor = (int *)GetSetMediaTrackInfo(pMT, "I_CUSTOMCOLOR", NULL);
  if (!pColor)
    return Colours::white;
  if (!(*pColor & 0x1000000))
    return Colours::white;

  return Colour::fromRGBA(*pColor & 0xFF, (*pColor & 0xFF00) >> 8,
                          (*pColor & 0xFF0000) >> 16, 0x9F);
}

//======================================================== Tracks

Tracks *Tracks::instance() {
  if (s_instance == NULL) {
    s_instance = new Tracks();
  }
  return s_instance;
}

Tracks::Tracks(void)
  : m_pCurrentBaseTrack(NULL), m_pOptions1(NULL), m_pOptions2(NULL),
    m_globalOffset(0), m_numMCUChannels(8) {
  m_selectedTracks.clear();
  m_pAllTracksBefore = new tTrackSet();
  m_pAllTracksNow = new tTrackSet();

  m_projectChangedConnectionId =
    ProjectConfig::instance()->connect2ProjectChangeSignal(
							   boost::bind(&Tracks::projectChanged, this, _1, _2));
}

Tracks::~Tracks(void) {
  ProjectConfig::instance()->disconnectProjectChangeSignal(
							   m_projectChangedConnectionId);

  safe_delete(m_pAllTracksNow);
  safe_delete(m_pAllTracksBefore);
  safe_delete(m_pOptions1);
  safe_delete(m_pOptions2);

  s_instance = NULL;
}

void Tracks::selectionChanged() {
  m_selectedTracks.clear();

  for (TrackIterator ti; !ti.end(); ++ti) {
    int *pSelected = (int *)GetSetMediaTrackInfo(*ti, "I_SELECTED", NULL);
    if (pSelected && *pSelected) {
      m_selectedTracks.insert(*ti);
    }
  }

  if (m_pLastSelectedSingleTrack != getSelectedSingleTrack()) {
    m_pLastSelectedSingleTrack = getSelectedSingleTrack();
    moveSelectedTrack2MCU();
  }
}

void Tracks::updateSelection(MediaTrack *pMT, bool selected) {
  // REAPER calls SetSurfaceSelected() once per track when the selection
  // changes (e.g. Ctrl+A across 256 tracks). selectionChanged() rebuilds
  // m_selectedTracks from scratch via TrackIterator — O(n) per call, O(n^2)
  // total for a full-selection operation. REAPER already passes exactly what
  // changed via the trackid/selected params, so update the set directly in
  // O(log n) per call and only re-evaluate the single-selected track.
  //
  // m_selectedTracks models NORMAL tracks only: TrackIterator skips the
  // master (index 0, csurf_mcu.h:217) and getSelectedSingleTrack(true)
  // handles the master via a separate I_SELECTED probe. REAPER DOES notify
  // master selection through SetSurfaceSelected, so without this guard a
  // master-only selection would insert the master here (size==1 &&
  // masterSelected==1), defeat both branches in getSelectedSingleTrack,
  // and return NULL — surfacing as "You must select a single track".
  if (pMT == GetMasterTrack(NULL))
    return; // keep the normal-tracks-only invariant; master handled elsewhere
  if (selected)
    m_selectedTracks.insert(pMT);
  else
    m_selectedTracks.erase(pMT);

  if (m_pLastSelectedSingleTrack != getSelectedSingleTrack()) {
    m_pLastSelectedSingleTrack = getSelectedSingleTrack();
    moveSelectedTrack2MCU();
  }
}

void Tracks::moveSelectedTrack2MCU() {
  // without this check we will never leave the while loop below
  if (getNumberOfActiveAnchors() == m_numMCUChannels)
    return;

  MediaTrack *trackid = getSelectedSingleTrack();
  if (trackid && Tracks::instance()->get2ndOptions()->isOptionSetTo(
								    MTO2_FOLLOW_REAPER, MTO2A_FOLLOW_REAPER_ON)) {
    tracksStatesChanged();
    // trackid was captured before tracksStatesChanged() ran. If the track was
    // deleted, it has been removed from m_trackStates (and, after P7, from
    // m_tracksByPointer) but trackid still holds the (now freed) pointer. Guard
    // before calling getTrackStateForMediaTrack, whose slow path would call
    // GetTrackGUID on freed memory.
    if (!m_structure.trackExists(trackid))
      return;
    TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(trackid);
    if (pTS && pTS->getAnchorChannel() == 0 && !pTS->isOnMCU()) {
      MediaTrack *newParent = Tracks::instance()->getParentForMediaTrack(trackid);
      Tracks::instance()->moveBaseTrack(newParent);
      int tracknr = MediaTrackInfo::getTrackNr(trackid);
      int numChannels = m_numMCUChannels - Tracks::instance()->getNumberOfActiveAnchors();

      if (numChannels <= 0)
        return;

      int originalOffset = Tracks::instance()->getGlobalOffset();
      Tracks::instance()->setGlobalOffset(0);
      while (!pTS->isOnMCU() &&
             Tracks::instance()->getGlobalOffset() < tracknr) {
        // setGlobalOffset() clamps the offset to [0, getMaxUsefulGlobalOffset()].
        // With a (near-)empty project that maximum is 0, so the offset can never
        // advance and the selected track never lands on the MCU. Without this
        // progress check the loop would spin forever (the GUI freeze observed
        // when adding a track to an empty project with FOLLOW_REAPER enabled).
        int offsetBefore = Tracks::instance()->getGlobalOffset();
        Tracks::instance()->setGlobalOffset(offsetBefore + numChannels);
        if (Tracks::instance()->getGlobalOffset() == offsetBefore)
          break; // offset is clamped and cannot grow any further
      }

      // track wasn't found (because it is not in the set of shown tracks, or
      // on a different level
      if (Tracks::instance()->getGlobalOffset() >= tracknr) {
        Tracks::instance()->setGlobalOffset(originalOffset);
      }
    }
  }
}

bool Tracks::tracksStatesChanged(bool checkProjectChange) {
  if (checkProjectChange)
    ProjectConfig::instance()->checkReaProjectChange();

  m_pAllTracksNow->clear();

  for (TrackIterator ti; !ti.end(); ++ti)
    m_pAllTracksNow->push_back(*ti);

  // O(n) comparison catches adds, removes, and reorders. Skip the O(n^2)
  // buildGraph() on the vast majority of frames where nothing has changed.
  // This early-exit is what makes it safe to call tracksStatesChanged() every
  // frame (see CSurf_MCU::Run()): it costs ~0.001ms on stable frames.
  if (*m_pAllTracksNow == *m_pAllTracksBefore)
    return false;

  // set_difference requires sorted ranges, so build temporary sets from the
  // ordered vectors to identify which tracks were added or removed.
  std::set<MediaTrack *> nowSet(m_pAllTracksNow->begin(), m_pAllTracksNow->end());
  std::set<MediaTrack *> beforeSet(m_pAllTracksBefore->begin(),
                                   m_pAllTracksBefore->end());
  std::set<MediaTrack *> dif;

  // new - old: tracks added
  set_difference(nowSet.begin(), nowSet.end(), beforeSet.begin(),
                 beforeSet.end(), inserter(dif, dif.begin()));

  BOOST_FOREACH (MediaTrack *pMT, dif) {
    TrackState *pTS = getTrackStateForMediaTrack(pMT);
    if (!pTS) { // when the project is changed, TrackStates are already read
                // before we get trackStatesChanged is called.
      assert(pMT != NULL);
      TrackState *pNew = new TrackState(pMT);
      m_trackStates[pNew->getGuidAsString()] = pNew;
      m_tracksByPointer[pMT] = pNew;
    }
    signalTrackAdded(pMT);
  }

  // old - new: tracks removed
  dif.clear();
  set_difference(beforeSet.begin(), beforeSet.end(), nowSet.begin(),
                 nowSet.end(), inserter(dif, dif.begin()));

  BOOST_FOREACH (MediaTrack *pMT, dif) {
    TrackState *pTS = getTrackStateForMediaTrack(pMT);
    if (pTS) { // when the project is changed, project-change prepare has
               // deleted the TrackStates already
      String guid = pTS->getGuidAsString();
      // drop the pointer-index entry while pMT is still a valid key, then free
      m_tracksByPointer.erase(pMT);
      delete (m_trackStates[guid]);
      m_trackStates.erase(m_trackStates.find(guid));
    }
    // Keep m_selectedTracks consistent: REAPER does not always fire
    // SetSurfaceSelected(child, false) before deleting a folder track, so
    // stale pointers would remain here and be dereferenced by
    // moveSelectedTrack2MCU()/getSelectedSingleTrack(). erase() of a
    // non-present pointer is a no-op.
    m_selectedTracks.erase(pMT);
    signalTrackRemoved(pMT);
  }

  *m_pAllTracksBefore = *m_pAllTracksNow;

  buildGraph();
  if (!m_structure.trackExists(m_pCurrentBaseTrack))
    m_pCurrentBaseTrack = NULL;

  return true;
}

MediaTrack *Tracks::getSelectedSingleTrack(bool includeMaster) {
  if (includeMaster) {
    int *masterSelection = (int *)GetSetMediaTrackInfo(
						       GetMasterTrack(NULL), "I_SELECTED", NULL);
    int masterSelected = masterSelection ? *masterSelection : 0;

    if (m_selectedTracks.size() == 1 && masterSelected == 0) {
      return *(m_selectedTracks.begin());
    } else if (m_selectedTracks.size() == 0 && masterSelected == 1) {
      return GetMasterTrack(NULL);
    } else {
      return NULL;
    }
  } else {
    if (m_selectedTracks.size() == 1) {
      return *(m_selectedTracks.begin());
    } else {
      return NULL;
    }
  }
}

void Tracks::createChannelTrackVector() {
  m_channelTracks.clear();
  m_channelTracks.resize(m_numMCUChannels + 1);
  m_channelTracks[0] = CSurf_TrackFromID(0, false);

  for (int i = 1; i <= m_numMCUChannels; i++) {
    m_channelTracks[i] = findMediaTrackForChannel(i);
  }
}

MediaTrack *Tracks::getMediaTrackForChannel(int channel) {
  if (channel >= 0 && channel < (int)m_channelTracks.size())
    return m_channelTracks[channel];
  else
    return NULL;
}

int Tracks::getChannelForMediaTrack(MediaTrack *pMT) {
  int n = (int)m_channelTracks.size();
  for (int i = 1; i < n; i++) {
    if (m_channelTracks[i] == pMT) {
      return i;
    }
  }
  return -1;
}

MediaTrack *Tracks::getParentForMediaTrack(MediaTrack *pMT) {
  TSNode *pNode = m_structureVU.nodeOfTrack(pMT);

  if (pNode == NULL)
    return NULL;

  TSNode *pParentNode = pNode->getParentNode();
  if (pParentNode == NULL)
    return NULL;

  return pParentNode->getMediaTrack();
}

std::vector<MediaTrack *> Tracks::getChildredForMediaTrack(MediaTrack * pMT) {
  std::vector<MediaTrack *> mediaTracks;

  TSNode *pNode = m_structureVU.nodeOfTrack(pMT);

  if (pNode == NULL)
    return mediaTracks;
	
  std::vector<TSNode *> children = pNode->getChildren();
  BOOST_FOREACH (TSNode *&pNode, children) {
    mediaTracks.push_back(pNode->getMediaTrack());
  }

  return mediaTracks;
}


MediaTrack *Tracks::findMediaTrackForChannel(int channel) {
  if (channel < 1 || channel > m_numMCUChannels)
    return NULL;

  return findMediaTrackForChannelUnlimited(channel);
}

// Same lookup as findMediaTrackForChannel(), but without the
// m_numMCUChannels cap. Used by moveTrackToLeftMostChannel() to locate
// QuickJump targets that sit beyond the currently visible channel range.
MediaTrack *Tracks::findMediaTrackForChannelUnlimited(int channel) {
  if (channel < 1)
    return NULL;

  // find anchor and count anchors with lower channel
  int numAnchorsWithLowerChannel = 0;
  if (Tracks::instance()->getOptions()->isOptionSetTo(MTO_DISABLE_ANCHORS,
                                                      MTOA_ANCHORS_YES)) {
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      int anchor = v.second->getAnchorChannel();
      // only active anchors (within the current surface range) are direct hits
      if (anchor == channel && anchor <= m_numMCUChannels)
        return v.second->getMediaTrack();
      else if (anchor >= 1 && anchor < channel && anchor <= m_numMCUChannels)
        numAnchorsWithLowerChannel++;
    }
  }

  int channelWithOffset = channel +
    Tracks::instance()->getGlobalOffset() -
    numAnchorsWithLowerChannel;

  TSNode *pNode = m_structure.nodeOfTrack(m_pCurrentBaseTrack);
  if (pNode == NULL)
    return NULL;

  if (Tracks::instance()->getOptions()->isOptionSetTo(MTO_REFLECT_FOLDER,
                                                      MTOA_REFLECT_PLUS) &&
      !pNode->isRoot() &&
      getTrackStateForMediaTrack(m_pCurrentBaseTrack) &&
      getTrackStateForMediaTrack(m_pCurrentBaseTrack)->getAnchorChannel() ==
      0) { // first non anchor channel must be the parent
    if (channelWithOffset == 1) {
      return m_pCurrentBaseTrack;
    }
    channelWithOffset--;
  }

  pNode = pNode->getNthChild(getFilter(), channelWithOffset);
  if (pNode == NULL)
    return NULL;

  return pNode->getMediaTrack();
}

int Tracks::getNumMediaTracksOnMCU() {
  TSNode *pNode = m_structure.nodeOfTrack(m_pCurrentBaseTrack);
  if (pNode == NULL)
    return 0;

  int numChildren = pNode->numChilds(getFilter());
  if (Tracks::instance()->getOptions()->isOptionSetTo(MTO_REFLECT_FOLDER,
                                                      MTOA_REFLECT_PLUS))
    return numChildren + 1;
  return numChildren;
}

bool Tracks::moveBaseTrack(MediaTrack *pMT) {
  if (!hasChilds(pMT))
    return false;

  m_pCurrentBaseTrack = pMT;

  buildGraph();

  return true;
}

bool Tracks::hasChilds(MediaTrack *pMT) {
  TSNode *pNode = m_structure.nodeOfTrack(pMT);
  return (pNode && pNode->hasChilds(getFilter()));
}

bool Tracks::moveBaseTrackToParent() {
  if (!baseTrackHasParent())
    return false;

  m_pCurrentBaseTrack = m_structure.nodeOfTrack(m_pCurrentBaseTrack)
    ->getParentNode()
    ->getMediaTrack();
  // Rebuild the graph for the new base track. moveBaseTrack() does this
  // explicitly; moveBaseTrackToParent() previously relied on tracksStatesChanged()
  // calling buildGraph() unconditionally as a side-effect. After that call was
  // made conditional (early-exit when the track list is unchanged), folder-up
  // navigation left m_structure stale and the controller was not updated.
  buildGraph();
  return true;
}

bool Tracks::baseTrackHasParent() {
  TSNode *pNode = m_structure.nodeOfTrack(m_pCurrentBaseTrack);
  return (pNode && (pNode->getParentNode() != NULL));
}

TSNode::EFilter Tracks::getFilter() {
  if (m_pOptions1->isOptionSetTo(MTO_SHOW, MTOA_SHOW_MCP)) {
    return TSNode::MCP;
  } else if (m_pOptions1->isOptionSetTo(MTO_SHOW, MTOA_SHOW_TCP)) {
    return TSNode::TCP;
  } else if (m_pOptions1->isOptionSetTo(MTO_SHOW, MTOA_SHOW_SET)) {
    return TSNode::MCU;
  }

  return TSNode::OFF;
}

void Tracks::setDisplayHandler(DisplayHandler *pDH) {
  if (m_pOptions1 == NULL) {
    m_pOptions1 = new MultiTrackOptions(pDH);
    m_pOptions2 = new MultiTrackOptions2(pDH);
  }
  createChannelTrackVector();
}

void Tracks::adjust(int numMCUChannels) {
  int clampedChannels = std::max(8, std::min(numMCUChannels, 64));
  if (m_numMCUChannels != clampedChannels) {
    m_numMCUChannels = clampedChannels;
    setGlobalOffset(m_globalOffset);  // clamp to new valid range
    createChannelTrackVector();        // keep vector in sync with m_numMCUChannels
  }
  updateTrackStates(m_numMCUChannels);

  if (m_pLastSelectedSingleTrack != getSelectedSingleTrack()) {
    m_pLastSelectedSingleTrack = getSelectedSingleTrack();
    selectionChanged();
  }

  // adjust Mixer
  bool updateMixer = false;
  if (m_pOptions2->isOptionSetTo(MTO2_MCP_ADJUCT, MTO2A_MCP_BANK)) {
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      TrackState *pTS = v.second;

      bool *pShowInMixer = (bool *)GetSetMediaTrackInfo(pTS->getMediaTrack(),
                                                        "B_SHOWINMIXER", NULL);
      if (pShowInMixer && *pShowInMixer != pTS->isOnMCU()) {
        *pShowInMixer = !*pShowInMixer;
        GetSetMediaTrackInfo(pTS->getMediaTrack(), "B_SHOWINMIXER",
                             pShowInMixer);
        updateMixer = true;
      }
    }
  }

  if (m_pOptions2->isOptionSetTo(MTO2_MCP_ADJUCT, MTO2A_MCP_ALL)) {
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      TrackState *pTS = v.second;

      if (pTS->getMediaTrack()) {
        bool *pShowInMixer = (bool *)GetSetMediaTrackInfo(
							  pTS->getMediaTrack(), "B_SHOWINMIXER", NULL);
        if (pShowInMixer && *pShowInMixer == false) {
          *pShowInMixer = true;
          GetSetMediaTrackInfo(pTS->getMediaTrack(), "B_SHOWINMIXER",
                               pShowInMixer);
          updateMixer = true;
        }
      }
    }
  }

  if (updateMixer) {
    TrackList_AdjustWindows(false);
    UpdateTimeline();
  }

  // adjust TCP
  bool updateTCP = false;
  if (m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_BANK) ||
      m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_SELECTED)) {
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      TrackState *pTS = v.second;

      bool *pShowInTCP = (bool *)GetSetMediaTrackInfo(pTS->getMediaTrack(),
                                                      "B_SHOWINTCP", NULL);
      if (!pShowInTCP)
        continue;
      if (m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_BANK) &&
	  (*pShowInTCP != pTS->isOnMCU()) ||
          m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_SELECTED) &&
	  (*pShowInTCP != (m_selectedTracks.find(pTS->getMediaTrack()) !=
			   m_selectedTracks.end()))) {
        updateTCP = true;
        break;
      }
    }
  }

  if (m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_ALL)) {
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      TrackState *pTS = v.second;

      bool *pShowInTCP = (bool *)GetSetMediaTrackInfo(pTS->getMediaTrack(),
                                                      "B_SHOWINTCP", NULL);
      if (pShowInTCP && *pShowInTCP == false) {
        updateTCP = true;
        break;
      }
    }
  }

  if (updateTCP) {
    // first hide all tracks (or we can get problems with the scroll-bar)
    /*
      bool bShow = false;
      BOOST_FOREACH(tTrackStates::value_type& v, m_trackStates) {
      TrackState* pTS = v.second;
      GetSetMediaTrackInfo(pTS->getMediaTrack(), "B_SHOWINTCP", &bShow);
      }

      int numShownTracks = calcNumShownTracks();
      if (numShownTracks > 0) {
      int tcpSizeInPixel = calcTCPSizeInPixel();
    */
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      TrackState *pTS = v.second;

      //        int wndHeight = tcpSizeInPixel / numShownTracks;
      //        GetSetMediaTrackInfo(pTS->getMediaTrack(), "I_HEIGHTOVERRIDE",
      //        &wndHeight);

      bool bShow = shouldTrackInTCP(pTS);
      bool *shown = (bool *)GetSetMediaTrackInfo(pTS->getMediaTrack(),
                                                 "B_SHOWINTCP", NULL);
      if (shown && bShow != *shown) {
        GetSetMediaTrackInfo(pTS->getMediaTrack(), "B_SHOWINTCP", &bShow);
      }
    }

    TrackList_AdjustWindows(true);
    UpdateTimeline();
  }
}

int Tracks::calcNumShownTracks() {
  int numShownTracks = 0;
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    if (shouldTrackInTCP(v.second)) {
      numShownTracks++;
    }
  }
  return numShownTracks;
}

bool Tracks::shouldTrackInTCP(TrackState *pTrackState) {
  return (m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_ALL) ||
          m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_BANK) &&
	  (pTrackState->isOnMCU()) ||
          m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_SELECTED) &&
	  (m_selectedTracks.find(pTrackState->getMediaTrack()) !=
	   m_selectedTracks.end()));
}

int Tracks::calcTCPSizeInPixel() {
  int maxSize = -1;

  bool *pMasterShown =
    (bool *)GetSetMediaTrackInfo(GetMasterTrack(NULL), "B_SHOWINTCP", NULL);
  bool masterShown = pMasterShown && *pMasterShown;
  if (masterShown)
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_MASTER_TRACK_TCP, 0);

  if (!m_trackStates.empty()) {
    MediaTrack *pMT = (*m_trackStates.begin()).second->getMediaTrack();
    bool bShow = true;
    GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", &bShow);

    int *pWndHeight = (int *)GetSetMediaTrackInfo(pMT, "I_WNDH", NULL);
    if (pWndHeight && *pWndHeight > maxSize) {
      maxSize = *pWndHeight;
    }

    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_TRACK_ZOOM_MAX_HEIGHT, 0);

    pWndHeight = (int *)GetSetMediaTrackInfo(pMT, "I_WNDH", NULL);
    if (pWndHeight && *pWndHeight > maxSize) {
      maxSize = *pWndHeight;
    }

    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_TRACK_ZOOM_MAX_HEIGHT, 0);
    bShow = false;
    GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", &bShow);
  }

  if (masterShown) {
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_MASTER_TRACK_TCP, 0);
    int *pMasterHeight =
      (int *)GetSetMediaTrackInfo(GetMasterTrack(NULL), "I_WNDH", NULL);
    if (pMasterHeight)
      maxSize -= *pMasterHeight;
    maxSize -= 10; // space below the master track
  }

  return maxSize;
}

void Tracks::updateTrackStates(int numMCUChannels) {
  if (numMCUChannels <= 0)
    return;

  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    if (!v.second->getMediaTrack())
      continue;

    v.second->setIsOnMCUChannel(0);

    if (m_pOptions2->isOptionSetTo(MTO2_TCP_ADJUCT, MTO2A_TCP_NO)) {
      v.second->setIsShownInTCP(
				MediaTrackInfo::isShownInTCP(v.second->getMediaTrack()));
      v.second->setTCPHeight(
			     MediaTrackInfo::getHeight(v.second->getMediaTrack()));
    }

    if (m_pOptions2->isOptionSetTo(MTO2_MCP_ADJUCT, MTO2A_MCP_NO)) {
      v.second->setIsShownInMCP(
				MediaTrackInfo::isShownInMCP(v.second->getMediaTrack()));
    }
  }

  for (int c = 1; c <= numMCUChannels; c++) {
    MediaTrack *pMT = getMediaTrackForChannel(c);
    if (pMT == NULL)
      continue;

    TrackState *pTS = getTrackStateForMediaTrack(pMT);
    safe_call(pTS, setIsOnMCUChannel(c));
  }
}

void Tracks::setTCP2TrackStates() {
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    MediaTrackInfo::showInTCP(v.second->getMediaTrack(),
                              v.second->isShownInTCP());
    MediaTrackInfo::setHeight(v.second->getMediaTrack(),
                              v.second->getTCPHeight());
  }

  TrackList_AdjustWindows(true);
  UpdateTimeline();
}

void Tracks::setMCP2TrackStates() {
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    MediaTrackInfo::showInMCP(v.second->getMediaTrack(),
                              v.second->isShownInMCP());
  }

  TrackList_AdjustWindows(false);
  UpdateTimeline();
}

int Tracks::getNumberOfAnchors() {
  if (Tracks::instance()->getOptions()->isOptionSetTo(MTO_DISABLE_ANCHORS,
						      MTOA_ANCHORS_NO))
    return 0;
						
  int numAnchors = 0;
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    if (v.second->getAnchorChannel() > 0)
      numAnchors++;
  }

  return numAnchors;
}

int Tracks::getNumberOfActiveAnchors(int maxChannel /* = -1 */) {
  if (Tracks::instance()->getOptions()->isOptionSetTo(MTO_DISABLE_ANCHORS,
						      MTOA_ANCHORS_NO))
    return 0;

  int effectiveMax = (maxChannel < 0) ? m_numMCUChannels : maxChannel;
  int numAnchors = 0;
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    int anchor = v.second->getAnchorChannel();
    if (anchor >= 1 && anchor <= effectiveMax)
      numAnchors++;
  }

  return numAnchors;
}

TrackState *Tracks::getTrackStateForMediaTrack(MediaTrack *pMediaTrack) {
  if (pMediaTrack == NULL)
    return NULL;

  // O(1) normal path: the pointer index. This is the hot lookup used once per
  // track inside TSGraph::buildGraph() and repeatedly in meter/display/editor
  // updates, so it must stay constant-time.
  tTracksByPointer::const_iterator it = m_tracksByPointer.find(pMediaTrack);
  if (it != m_tracksByPointer.end())
    return it->second;

  // Fallback: REAPER may replace a MediaTrack* while preserving its GUID (e.g.
  // project restore, undo). Look the GUID up directly in the GUID map (do not
  // linearly scan it), refresh the stored pointer, and (re)build the
  // pointer-index entry so subsequent lookups hit the O(1) path again.
  String strGUID = GUID2String(GetTrackGUID(pMediaTrack));
  tTrackStates::iterator itTS = m_trackStates.find(strGUID);
  if (itTS != m_trackStates.end()) {
    TrackState *pTS = itTS->second;
    MediaTrack *pStored = pTS->getMediaTrack();
    if (pStored != pMediaTrack) {
      if (pStored != NULL)
        m_tracksByPointer.erase(pStored);
      pTS->setMediaTrack(pMediaTrack);
    }
    m_tracksByPointer[pMediaTrack] = pTS;
    return pTS;
  }

  return NULL;
}

void Tracks::buildGraph() {
  m_structure.buildGraph(m_pOptions1->isOptionSetTo(MTO_REFLECT_FOLDER,
						    MTOA_REFLECT_NO));
  m_structureVU.buildGraph(false);
	
  createChannelTrackVector();
  updateVUactive();
}

bool Tracks::oneChildrenIsSoloed(MediaTrack * pMT) {
  std::vector<MediaTrack *> children = getChildredForMediaTrack(pMT);

  int *soloState;
  for(MediaTrack *pMediaTrack : children) {
    soloState = (int *)GetSetMediaTrackInfo(pMediaTrack, "I_SOLO", NULL);
    if (soloState && *soloState > 0)
      return true;
    if (oneChildrenIsSoloed(pMediaTrack))
      return true;
  }

  return false;
}

void Tracks::activeVUallChildren(MediaTrack *pMT) {
  std::vector<MediaTrack *> children = getChildredForMediaTrack(pMT);

  for (MediaTrack *pMediaTrack : children) {
    safe_call(getTrackStateForMediaTrack(pMediaTrack), setVUactive(true));
    activeVUallChildren(pMediaTrack);
  }
}

void Tracks::updateVUactive() {
  if (!m_pMCU->SomethingSoloed()) {
    for (auto &e : m_trackStates) {
      TrackState &ts = *e.second;
      bool *muteState = (bool *)GetSetMediaTrackInfo(ts.getMediaTrack(),
						     "B_MUTE", NULL);
      e.second->setVUactive(!muteState || !*muteState);
    }
    return;
  }
	
  for(auto &e : m_trackStates) {
    e.second->setVUactive(false);
  }

  for(auto &e : m_trackStates) {
    TrackState &ts = *e.second;
    int *soloState = (int *)GetSetMediaTrackInfo(ts.getMediaTrack(),
						 "I_SOLO", NULL);
    if (soloState && *soloState > 0) {
      ts.setVUactive(true);
      MediaTrack *p = getParentForMediaTrack(ts.getMediaTrack());
      // solo all parents
      while (p) {
	safe_call(getTrackStateForMediaTrack(p), setVUactive(true));
	p = getParentForMediaTrack(p);
      }
      // solo children
      if (!oneChildrenIsSoloed(ts.getMediaTrack())) {
	activeVUallChildren(ts.getMediaTrack());
      }
    }
  }

  // check sends
  for (auto &e : m_trackStates) {
    TrackState &ts = *e.second;
    int i = 0;
    MediaTrack *s = (MediaTrack *)
      GetSetTrackSendInfo(ts.getMediaTrack(), -1, i, "P_SRCTRACK", NULL);
    while (s) {
      i++;
      if (getTrackStateForMediaTrack(s) && getTrackStateForMediaTrack(s)->getVUactive())
	ts.setVUactive(true);
      s = (MediaTrack *)
	GetSetTrackSendInfo(ts.getMediaTrack(), -1, i, "P_SRCTRACK", NULL);
    }
  }
}

int Tracks::connect2TrackAddedSignal(const tTrackSignalSlot &slot) {
  m_trackAddedConnections[++m_nextConnectionId] =
    signalTrackAdded.connect(slot);
  return m_nextConnectionId;
}

void Tracks::disconnectTrackAdded(int connectionId) {
  m_trackAddedConnections[connectionId].disconnect();
  m_trackAddedConnections.erase(m_trackAddedConnections.find(connectionId));
}

int Tracks::connect2TrackRemovedSignal(const tTrackSignalSlot &slot) {
  m_trackRemovedConnections[++m_nextConnectionId] =
    signalTrackRemoved.connect(slot);
  return m_nextConnectionId;
}

void Tracks::disconnectTrackRemoved(int connectionId) {
  m_trackRemovedConnections[connectionId].disconnect();
  m_trackRemovedConnections.erase(m_trackRemovedConnections.find(connectionId));
}

#define TRACKSTATE_NODE_ROOT String("TRACKSTATES")

void Tracks::projectChanged(XmlElement *pXmlElement,
                            ProjectConfig::EAction action) {
  XmlElement *pStatesNode;

  switch (action) {
  case ProjectConfig::WRITE:
    pStatesNode = new XmlElement(TRACKSTATE_NODE_ROOT);
    pXmlElement->addChildElement(pStatesNode);
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      v.second->writeTrackStatesToProjectConfig(pStatesNode);
    }
    break;
  case ProjectConfig::FREE:
    // there is no need to clear the m_trackStates here,
    // this is done in trackStatesChanged()
    break;
  case ProjectConfig::READ:
    tracksStatesChanged(false);
    pStatesNode = pXmlElement->getChildByName(TRACKSTATE_NODE_ROOT);
    if (pStatesNode) {
      for (auto* pChild : pStatesNode->getChildIterator()) {
        if (pChild->getTagName() == TRACKSTATE_NODE_SINGLE_STATE) {
          TrackState *pTS = new TrackState();
          pTS->readTrackStatesFromProjectConfig(pChild);
          if (pTS->getMediaTrack()) {
            TrackState *pOldTS =
	      getTrackStateForMediaTrack(pTS->getMediaTrack());
            if (pOldTS) { // overwrite existing track state, so first delete old
                          // one
              m_tracksByPointer.erase(pTS->getMediaTrack());
              delete (m_trackStates[pTS->getGuidAsString()]);
              m_trackStates.erase(m_trackStates.find(pTS->getGuidAsString()));
            }
            m_trackStates[pTS->getGuidAsString()] = pTS;
            m_tracksByPointer[pTS->getMediaTrack()] = pTS;
          }
        }
      }
    }
    break;
  }
}

MediaTrack *Tracks::getMediaTrackForGUID(String guid) {
  BOOST_FOREACH (MediaTrack *pMT, *m_pAllTracksNow) {
    if (guid == GUID2String(GetTrackGUID(pMT))) {
      return pMT;
    }
  }
  return NULL;
}

bool Tracks::moveTrackToLeftMostChannel(MediaTrack *pMT) {
  buildGraph();
  assert(pMT != NULL);
  // if the pMT MediaTrack is an anchor, we need the next MediaTrack
  // that isn't an anchor
  if (getTrackStateForMediaTrack(pMT) &&
      getTrackStateForMediaTrack(pMT)->getAnchorChannel() > 0 &&
      m_pOptions1->isOptionSetTo(MTO_DISABLE_ANCHORS, MTOA_ANCHORS_YES)) {
    int depth = m_structure.nodeOfTrack(pMT)->getDepth();
    int channelNr = MediaTrackInfo::getTrackNr(pMT);
    bool trackFound = false;
    for (TrackIterator ti; !ti.end(); ++ti) {
      if (trackFound &&
	  getTrackStateForMediaTrack(*ti) &&
          getTrackStateForMediaTrack(*ti)->getAnchorChannel() == 0 &&
          depth == m_structure.nodeOfTrack(*ti)->getDepth()) {
        pMT = *ti;
        break;
      }
      if (channelNr == MediaTrackInfo::getTrackNr(*ti)) {
        trackFound = true;
      }
    }
  }

  TSNode *pMTNode = m_structure.nodeOfTrack(pMT);
  MediaTrack *pOriginalMT = m_pCurrentBaseTrack;
  m_pCurrentBaseTrack = pMTNode->getParentNode()->getMediaTrack();

  TSNode *pNode = m_structure.nodeOfTrack(m_pCurrentBaseTrack);
  int numChilds = pNode->numChilds(getFilter());

  int childWithTrack = 0;
  MediaTrack *pMTForChannel;
  Tracks::instance()->setGlobalOffset(0);
  do {
    pMTForChannel = findMediaTrackForChannelUnlimited(++childWithTrack);
    if (pMTForChannel == pMT) {
      int numAnchors = 0;
      for (int j = 1; j < childWithTrack && j <= m_numMCUChannels; j++) {
        MediaTrack *pAnchorMT =
            findMediaTrackForChannelUnlimited(j);
        TrackState *pAnchorTS = getTrackStateForMediaTrack(pAnchorMT);
        if (pAnchorTS && pAnchorTS->getAnchorChannel() > 0 &&
            pAnchorTS->getAnchorChannel() <= m_numMCUChannels &&
            m_pOptions1->isOptionSetTo(MTO_DISABLE_ANCHORS, MTOA_ANCHORS_YES)) {
          numAnchors++;
        }
      }
      Tracks::instance()->setGlobalOffset(childWithTrack - numAnchors - 1);

      return true;
    }
  } while (NULL != pMTForChannel);

  m_pCurrentBaseTrack = pOriginalMT;

  return false;
}

bool Tracks::setGlobalOffset(int globalOffset) {
  int clamped = clampGlobalOffset(globalOffset);
  if (m_globalOffset == clamped)
    return false;

  m_globalOffset = clamped;
  createChannelTrackVector();
  updateTrackStates(getNumberOfChannelStrips());
  return true;
}

int Tracks::getEffectiveBankStep() {
  int activeAnchors = getNumberOfActiveAnchors();
  return std::max(1, m_numMCUChannels - activeAnchors);
}

int Tracks::getLegacyPageStep() {
  int activeAnchors = getNumberOfActiveAnchors(8);
  return std::max(1, 8 - activeAnchors);
}

int Tracks::getMaxUsefulGlobalOffset() {
  int activeAnchors = getNumberOfActiveAnchors();
  int freeSlots = std::max(1, m_numMCUChannels - activeAnchors);
  return std::max(0, getNumMediaTracksOnMCU() - freeSlots);
}

int Tracks::clampGlobalOffset(int offset) {
  if (offset < 0)
    return 0;

  int maxOffset = getMaxUsefulGlobalOffset();
  if (offset > maxOffset)
    return maxOffset;

  return offset;
}

bool Tracks::clampCurrentGlobalOffset() {
  int clamped = clampGlobalOffset(m_globalOffset);
  if (clamped != m_globalOffset) {
    return setGlobalOffset(clamped);
  }
  return false;
}

int Tracks::getNumMediaTracksTotal() { return CSurf_NumTracks(false); }

int Tracks::getNumberOfChannelStrips() {
  return m_numMCUChannels;
}

bool Tracks::isTrackInFilter(MediaTrack *pMT) {
  return TSNode::showTrack(pMT, getFilter());
}

void Tracks::dumpMappingState() {
  MCU_LOG("=== Tracks mapping state ===");
  MCU_LOG("  numMCUChannels = %d", m_numMCUChannels);
  MCU_LOG("  globalOffset = %d", m_globalOffset);
  MCU_LOG("  channelTracks size = %zu", m_channelTracks.size());
  MCU_LOG("  total anchors = %d", getNumberOfAnchors());
  MCU_LOG("  active anchors = %d", getNumberOfActiveAnchors());

  for (int c = 0; c <= m_numMCUChannels; c++) {
    MediaTrack *pMT = getMediaTrackForChannel(c);
    if (pMT) {
      int trackNr = MediaTrackInfo::getTrackNr(pMT);
      String name = MediaTrackInfo::getTrackName(pMT, true);
      MCU_LOG("  ch[%d] = track %d \"%s\"", c, trackNr, name.toRawUTF8());
    } else {
      MCU_LOG("  ch[%d] = (empty)", c);
    }
  }
}
