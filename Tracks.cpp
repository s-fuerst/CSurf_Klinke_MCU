/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "Tracks.h"
#include "csurf.h"
#include "csurf_mcu.h"
#include <boost\foreach.hpp>
#include <boost\bind.hpp>
#include "Assert.h"
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
  case TCP:
    return (*((bool *)GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", NULL)));
  case MCP:
    return (*((bool *)GetSetMediaTrackInfo(pMT, "B_SHOWINMIXER", NULL)));
  case MCU:
    return Tracks::instance()->getTrackStateForMediaTrack(pMT)->isInSet();
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

  for (TrackIterator ti; !ti.end(); ++ti) {
    TSNode *pNewNode = new TSNode(*ti, pParentNode);
    m_mapTrack[*ti] = pNewNode;

    if (!(Tracks::instance()
                  ->getTrackStateForMediaTrack(*ti)
                  ->getAnchorChannel() > 0 &&
          Tracks::instance()->getOptions()->isOptionSetTo(MTO_DISABLE_ANCHORS,
                                                          MTOA_ANCHORS_YES))) {
      pParentNode->addChild(pNewNode);
    }

    if (flat) // ignore tree structure and add everything to the root (NULL)
      continue;

    int *pFD = (int *)GetSetMediaTrackInfo(*ti, "I_FOLDERDEPTH", NULL);
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
      m_isShownInTCP(true), m_tcpHeight(16), m_quickJumpName(String::empty),
      m_quickRoot(false), m_displayName(String::empty), m_vu(true),
      m_guidAsString(String::empty) {}

TrackState::TrackState(MediaTrack *pMT)
    : m_pMediaTrack(NULL), m_isInSet(true), m_isOnMCU(false), m_onMCUChannel(0),
      m_anchorChannel(0), m_quickJumpChannel(0), m_isShownInMCP(true),
      m_isShownInTCP(true), m_tcpHeight(16), m_quickJumpName(String::empty),
      m_quickRoot(false), m_vu(true), m_displayName(String::empty) {
  m_pMediaTrack = pMT;
  if (pMT)
    m_guidAsString = GUID2String(GetTrackGUID(pMT));
  else
    m_guidAsString = String::empty;
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
    if (fullTrackName.contains(JUCE_T("|"))) {
      return fullTrackName.fromFirstOccurrenceOf(JUCE_T("|"), false, false)
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

#define TRACKSTATE_NODE_SINGLE_STATE JUCE_T("STATE")
#define TRACKSTATE_ATT_TRACK JUCE_T("track") // GUID as String
#define TRACKSTATE_ATT_NAME JUCE_T("name")
#define TRACKSTATE_ATT_TCP JUCE_T("tcp")
#define TRACKSTATE_ATT_MCP JUCE_T("mcp")
#define TRACKSTATE_ATT_MCU JUCE_T("mcu")
#define TRACKSTATE_ATT_ANCHOR JUCE_T("anchor")
#define TRACKSTATE_ATT_QUICK_JUMP JUCE_T("q_channel")
#define TRACKSTATE_ATT_QUICK_NAME JUCE_T("q_name")
#define TRACKSTATE_ATT_QUICK_ROOT JUCE_T("q_root")

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
  return *bShown;
}

bool MediaTrackInfo::isShownInMCP(MediaTrack *pMT) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return false;
  bool *bShown = (bool *)GetSetMediaTrackInfo(pMT, "B_SHOWINMIXER", NULL);
  return *bShown;
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
  return *pHeight;
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
  return (int)GetSetMediaTrackInfo(pMT, "IP_TRACKNUMBER", NULL);
}

bool MediaTrackInfo::testPtr(char *pName) {
  __try {
    char *test = pName;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

String MediaTrackInfo::getTrackName(MediaTrack *pMT,
                                    bool showTrackNumberIfEmpty) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return String::empty;
  char *pName = (char *)GetSetMediaTrackInfo(pMT, "P_NAME", NULL);
  if (IsBadStringPtr(pName, 1) == TRUE)
    return String::empty;
  if (testPtr(pName) && pName && *pName != 0) {
    return String(pName);
  }

  if (!showTrackNumberIfEmpty) {
    return String::empty;
  }

  int nr = (int)GetSetMediaTrackInfo(pMT, "IP_TRACKNUMBER", NULL);
  return String(nr);
}

void MediaTrackInfo::setTrackName(MediaTrack *pMT, String strTrackname) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return;
  const char *pName = strTrackname.toCString();
  GetSetMediaTrackInfo(pMT, "P_NAME", (void *)pName);
}

Colour MediaTrackInfo::getTrackColor(MediaTrack *pMT) {
  assert(pMT != NULL);
  if (pMT == NULL)
    return Colours::black;
  int *pColor = (int *)GetSetMediaTrackInfo(pMT, "I_CUSTOMCOLOR", NULL);
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
      m_globalOffset(0) {
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
    if (*pSelected) {
      m_selectedTracks.insert(*ti);
    }
  }

  if (m_pLastSelectedSingleTrack != getSelectedSingleTrack()) {
    m_pLastSelectedSingleTrack = getSelectedSingleTrack();
    moveSelectedTrack2MCU();
  }
}

void Tracks::moveSelectedTrack2MCU() {
  // without this check we will never leave the while loop below
  if (getNumberOfAnchors() == 8)
    return;

  MediaTrack *trackid = getSelectedSingleTrack();
  if (trackid && Tracks::instance()->get2ndOptions()->isOptionSetTo(
                     MTO2_FOLLOW_REAPER, MTO2A_FOLLOW_REAPER_ON)) {
    tracksStatesChanged();
    TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(trackid);
    if (pTS->getAnchorChannel() == 0 && !pTS->isOnMCU()) {
      int tracknr = MediaTrackInfo::getTrackNr(trackid);
      int numChannels = 8 - Tracks::instance()->getNumberOfAnchors();

      int originalOffset = Tracks::instance()->getGlobalOffset();
      Tracks::instance()->setGlobalOffset(0);
      while (!pTS->isOnMCU() &&
             Tracks::instance()->getGlobalOffset() < tracknr) {
        Tracks::instance()->setGlobalOffset(
            Tracks::instance()->getGlobalOffset() + numChannels);
      }

      // track wasn't found (because it is in the set of shown tracks), so use
      // the original offset
      if (Tracks::instance()->getGlobalOffset() >= tracknr) {
        Tracks::instance()->setGlobalOffset(originalOffset);
      }
    }
  }
}

bool Tracks::tracksStatesChanged(bool checkProjectChange) {
  if (checkProjectChange)
    ProjectConfig::instance()->checkReaProjectChange();

  bool somethingHasChanged = false;
  m_pAllTracksNow->clear();

  for (TrackIterator ti; !ti.end(); ++ti) {
    m_pAllTracksNow->insert(*ti);
  }

  // send signals for all added or removed tracks, therefore we build
  // the difference between the old and new set of tracks
  tTrackSet dif;

  // new - old: tracks added
  set_difference(m_pAllTracksNow->begin(), m_pAllTracksNow->end(),
                 m_pAllTracksBefore->begin(), m_pAllTracksBefore->end(),
                 inserter(dif, dif.begin()));

  BOOST_FOREACH (MediaTrack *pMT, dif) {
    TrackState *pTS = getTrackStateForMediaTrack(pMT);
    if (!pTS) { // when the project is changed, TrackStates are already read
                // before we get trackStatesChanged is called.
      assert(pMT != NULL);
      m_trackStates[GUID2String(GetTrackGUID(pMT))] = new TrackState(pMT);
    }
    signalTrackAdded(pMT);
    somethingHasChanged = true;
  }

  // old - new: tracks removed
  dif.clear();
  set_difference(m_pAllTracksBefore->begin(), m_pAllTracksBefore->end(),
                 m_pAllTracksNow->begin(), m_pAllTracksNow->end(),
                 inserter(dif, dif.begin()));

  BOOST_FOREACH (MediaTrack *pMT, dif) {
    TrackState *pTS = getTrackStateForMediaTrack(pMT);
    if (pTS) { // when the project is changed, project-change prepare has
               // deleted the TrackStates already
      String guid = pTS->getGuidAsString();
      delete (m_trackStates[guid]);
      m_trackStates.erase(m_trackStates.find(guid));
    }
    signalTrackRemoved(pMT);
    somethingHasChanged = true;
  }

  m_pAllTracksBefore->clear();

  for (TrackIterator ti; !ti.end(); ++ti) {
    m_pAllTracksBefore->insert(*ti);
  }

  buildGraph();
  if (!m_structure.trackExists(m_pCurrentBaseTrack)) {
    m_pCurrentBaseTrack = NULL;
  }

  return somethingHasChanged;
}

MediaTrack *Tracks::getSelectedSingleTrack() { // returns null if more then one
                                               // track is selected
  if (m_selectedTracks.size() == 1) {
    return *(m_selectedTracks.begin());
  } else {
    return NULL;
  }
}

void Tracks::createChannelTrackVector() {
  m_channelTracks.clear();
  m_channelTracks.resize(9);
  m_channelTracks[0] = CSurf_TrackFromID(0, false);

  for (int i = 1; i < 9; i++) {
    m_channelTracks[i] = findMediaTrackForChannel(i);
  }
  //  TrackList_UpdateAllExternalSurfaces();
}

MediaTrack *Tracks::getMediaTrackForChannel(int channel) {
  if (channel < 9)
    return m_channelTracks[channel];
  else
    return NULL;
}

int Tracks::getChannelForMediaTrack(MediaTrack *pMT) {
  for (int i = 1; i < 9; i++) {
    if (m_channelTracks[i] = pMT) {
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
  // find anchor and count anchors with lower channel
  int numAnchorsWithLowerChannel = 0;
  if (Tracks::instance()->getOptions()->isOptionSetTo(MTO_DISABLE_ANCHORS,
                                                      MTOA_ANCHORS_YES)) {
    BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
      int anchor = v.second->getAnchorChannel();
      if (anchor == channel)
        return v.second->getMediaTrack();
      else if (anchor != 0 && anchor < channel)
        numAnchorsWithLowerChannel++;
    }
  }

  // TODO: this can't work with extenders, because the m_pMCU offset will be set
  // multiple times (each for every unit)
  int channelWithOffset = channel + m_pMCU->GetOffset() +
                          Tracks::instance()->getGlobalOffset() -
                          numAnchorsWithLowerChannel;

  TSNode *pNode = m_structure.nodeOfTrack(m_pCurrentBaseTrack);
  if (pNode == NULL)
    return NULL;

  if (Tracks::instance()->getOptions()->isOptionSetTo(MTO_REFLECT_FOLDER,
                                                      MTOA_REFLECT_PLUS) &&
      !pNode->isRoot() &&
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
  updateTrackStates(numMCUChannels);

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
        if (*pShowInMixer == false) {
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
      if (*pShowInTCP == false) {
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
      if (bShow) {
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
  if (*pMasterShown)
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_MASTER_TRACK_TCP, 0);

  if (!m_trackStates.empty()) {
    MediaTrack *pMT = (*m_trackStates.begin()).second->getMediaTrack();
    bool bShow = true;
    GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", &bShow);

    int *pWndHeight = (int *)GetSetMediaTrackInfo(pMT, "I_WNDH", NULL);
    if (*pWndHeight > maxSize) {
      maxSize = *pWndHeight;
    }

    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_TRACK_ZOOM_MAX_HEIGHT, 0);

    pWndHeight = (int *)GetSetMediaTrackInfo(pMT, "I_WNDH", NULL);
    if (*pWndHeight > maxSize) {
      maxSize = *pWndHeight;
    }

    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_TRACK_ZOOM_MAX_HEIGHT, 0);
    bShow = false;
    GetSetMediaTrackInfo(pMT, "B_SHOWINTCP", &bShow);
  }

  if (*pMasterShown) {
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_MASTER_TRACK_TCP, 0);
    int *pMasterHeight =
        (int *)GetSetMediaTrackInfo(GetMasterTrack(NULL), "I_WNDH", NULL);
    maxSize -= *pMasterHeight;
    maxSize -= 10; // space below the master track
  }

  return maxSize;
}

void Tracks::updateTrackStates(int numMCUChannels) {
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
  int numAnchors = 0;
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    if (v.second->getAnchorChannel() > 0)
      numAnchors++;
  }

  return numAnchors;
}

TrackState *Tracks::getTrackStateForMediaTrack(MediaTrack *pMediaTrack) {
  // maybe the same track (same GUID) has got a new pointer (e.g. when a project
  // is restored) so search via the GUID and update the MediaTrack* if this has
  // chnaged
  String strGUID = GUID2String(GetTrackGUID(pMediaTrack));
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    if (v.second->getGuidAsString() == strGUID) {
      if (v.second->getMediaTrack() != pMediaTrack && pMediaTrack != NULL) {
        v.second->setMediaTrack(pMediaTrack);
      }
      return v.second;
    }
  }

  // in the case, that the media track is already deleted, GetTrackGUID doens't
  // return the old GUID so search using hte pMediaTrack pointer directly
  BOOST_FOREACH (tTrackStates::value_type &v, m_trackStates) {
    if (v.second->getMediaTrack() == pMediaTrack) {
      return v.second;
    }
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
    if (*soloState > 0)
      return true;
    if (oneChildrenIsSoloed(pMediaTrack))
      return true;
  }

  return false;
}

void Tracks::activeVUallChildren(MediaTrack *pMT) {
	std::vector<MediaTrack *> children = getChildredForMediaTrack(pMT);

	for (MediaTrack *pMediaTrack : children) {
		getTrackStateForMediaTrack(pMediaTrack)->setVUactive(true);
		activeVUallChildren(pMediaTrack);
	}
}

void Tracks::updateVUactive() {
	if (!m_pMCU->SomethingSoloed()) {
		for (auto &e : m_trackStates) {
			e.second->setVUactive(true);
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
		if (*soloState > 0) {
			ts.setVUactive(true);
			MediaTrack *p = getParentForMediaTrack(ts.getMediaTrack());
			// solo all parents
			while (p) {
				getTrackStateForMediaTrack(p)->setVUactive(true);
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
			if (getTrackStateForMediaTrack(s)->getVUactive())
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

#define TRACKSTATE_NODE_ROOT JUCE_T("TRACKSTATES")

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
      forEachXmlChildElement(*pStatesNode, pChild) {
        if (pChild->getTagName() == TRACKSTATE_NODE_SINGLE_STATE) {
          TrackState *pTS = new TrackState();
          pTS->readTrackStatesFromProjectConfig(pChild);
          if (pTS->getMediaTrack()) {
            TrackState *pOldTS =
                getTrackStateForMediaTrack(pTS->getMediaTrack());
            if (pOldTS) { // overwrite existing track state, so first delete old
                          // one
              delete (m_trackStates[pTS->getGuidAsString()]);
              m_trackStates.erase(m_trackStates.find(pTS->getGuidAsString()));
            }
            m_trackStates[pTS->getGuidAsString()] = pTS;
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
  if (getTrackStateForMediaTrack(pMT)->getAnchorChannel() > 0 &&
      m_pOptions1->isOptionSetTo(MTO_DISABLE_ANCHORS, MTOA_ANCHORS_YES)) {
    int depth = m_structure.nodeOfTrack(pMT)->getDepth();
    int channelNr = MediaTrackInfo::getTrackNr(pMT);
    bool trackFound = false;
    for (TrackIterator ti; !ti.end(); ++ti) {
      if (trackFound &&
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
    pMTForChannel = findMediaTrackForChannel(++childWithTrack);
    if (pMTForChannel == pMT) {
      int numAnchors = 0;
      for (int j = 1; j < childWithTrack; j++) {
        if (getTrackStateForMediaTrack(findMediaTrackForChannel(j))
                    ->getAnchorChannel() > 0 &&
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

void Tracks::setGlobalOffset(int globalOffset) {
  m_globalOffset = globalOffset;
  createChannelTrackVector();
  updateTrackStates(getNumberOfChannelStrips());
}

int Tracks::getNumMediaTracksTotal() { return CSurf_NumTracks(false); }

int Tracks::getNumberOfChannelStrips() {
  // TODO extender: implement this
  return 8;
}

bool Tracks::isTrackInFilter(MediaTrack *pMT) {
  return TSNode::showTrack(pMT, getFilter());
}
