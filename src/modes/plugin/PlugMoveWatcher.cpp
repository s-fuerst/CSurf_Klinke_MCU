/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PlugMoveWatcher.h"
#include "csurf_mcu.h"
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include "Tracks.h"

PlugMoveWatcher *PlugMoveWatcher::s_instance = NULL;

PlugInstanceInfo::PlugInstanceInfo(MediaTrack *pMediaTrack, int iSlot) {
  m_pMediaTrack = pMediaTrack;
  m_slot = iSlot;
  m_plugGUID = GUID2String(TrackFX_GetFXGUID(pMediaTrack, iSlot));
}

bool PlugInstanceInfo::existAndIsSame() {
  String currentGuid = GUID2String(TrackFX_GetFXGUID(m_pMediaTrack, m_slot));
  return m_plugGUID.isNotEmpty() && currentGuid.isNotEmpty() &&
         m_plugGUID == currentGuid;
}

PlugInstanceInfo::tMoveToResult PlugInstanceInfo::movedTo() {
	MediaTrack* master = GetMasterTrack(NULL);
	int iNumFX = TrackFX_GetCount(master);
	for (int iFX = 0; iFX < iNumFX; iFX++) {
		if (compareWith(master, iFX)) {
			return tMoveToResult(true, master, iFX);
		}
	}
		
  for (TrackIterator ti; !ti.end(); ++ti) {
    int iNumFX = TrackFX_GetCount(*ti);
    for (int iFX = 0; iFX < iNumFX; iFX++) {
      if (compareWith(*ti, iFX)) {
        return tMoveToResult(true, *ti, iFX);
      }
    }
  }

  return tMoveToResult(false, NULL, 0);
}

bool PlugInstanceInfo::compareWith(MediaTrack *pMediaTrack, int iSlot) {
  String currentGuid = GUID2String(TrackFX_GetFXGUID(pMediaTrack, iSlot));
  return m_plugGUID.isNotEmpty() && currentGuid.isNotEmpty() &&
         m_plugGUID == currentGuid;
}

PlugMoveWatcher *PlugMoveWatcher::instance() {
  if (s_instance == NULL) {
    s_instance = new PlugMoveWatcher();
  }
  return s_instance;
}

PlugMoveWatcher::PlugMoveWatcher(void) : m_nextConnectionId(0) {
  m_plugInstanceInfos.clear();
  m_activePlugMoveConnections.clear();

  m_trackRemovedConnection = Tracks::instance()->connect2TrackRemovedSignal(
																																						boost::bind(&PlugMoveWatcher::trackRemoved, this, _1));
}

PlugMoveWatcher::~PlugMoveWatcher(void) {
  Tracks::instance()->disconnectTrackRemoved(m_trackRemovedConnection);

  for (tPlugInstances::iterator iter = m_plugInstanceInfos.begin();
       iter != m_plugInstanceInfos.end(); ++iter)
    delete iter->second;
  m_plugInstanceInfos.clear();

  s_instance = NULL;
}

void PlugMoveWatcher::trackRemoved(MediaTrack *pMT) {
  for (tPlugInstances::iterator iterPII = m_plugInstanceInfos.begin();
       iterPII != m_plugInstanceInfos.end();) {
    if ((*iterPII).first.get<0>() == pMT) {
      delete (*iterPII).second;
      m_plugInstanceInfos.erase(iterPII);
      iterPII = m_plugInstanceInfos.begin();
    } else {
      iterPII++;
    }
  }

  m_iterPlugInstances = m_plugInstanceInfos.begin();
}

void PlugMoveWatcher::checkMovementForTrack(MediaTrack *pTrack) {
  if (!pTrack)
    return;
  std::vector<tPlug> toRemove;
  std::vector<tPlug> toAdd;
	
	int numFX = TrackFX_GetCount(pTrack);
	for (int iSlot = 0; iSlot < numFX; iSlot++) {
		tPlug plug(pTrack, iSlot);
		if (m_plugInstanceInfos.find(plug) != m_plugInstanceInfos.end()) {
			if (!m_plugInstanceInfos[plug]->existAndIsSame()) {
				toRemove.push_back(plug);
				PlugInstanceInfo::tMoveToResult res =
					m_plugInstanceInfos[plug]->movedTo();
				if (res.get<0>()) { // plug was moved
					toAdd.push_back(tPlug(res.get<1>(), res.get<2>()));
					signalPlugMove(pTrack, iSlot, res.get<1>(), res.get<2>());
				} else {
					signalPlugMove(pTrack, iSlot, NULL, -1);
				}
			}
		} else { // plug is unknown
			toAdd.push_back(tPlug(pTrack, iSlot));
		}
	}
	// when fx are removed, we don't check the plugInstanceInfos behind numFX,
	// this is done here
	while (m_plugInstanceInfos.find(tPlug(pTrack, numFX)) !=
				 m_plugInstanceInfos.end()) {
		tPlug plug(pTrack, numFX);
		if (!m_plugInstanceInfos[plug]->existAndIsSame()) {
			toRemove.push_back(plug);
			PlugInstanceInfo::tMoveToResult res =
				m_plugInstanceInfos[plug]->movedTo();
			if (res.get<0>()) { // plug was moved
				toAdd.push_back(tPlug(res.get<1>(), res.get<2>()));
				signalPlugMove(pTrack, numFX, res.get<1>(), res.get<2>());
			} else {
				signalPlugMove(pTrack, numFX, NULL, -1);
			}
		}
		numFX++;
	}

  for (std::vector<tPlug>::iterator iterRemove = toRemove.begin();
       iterRemove != toRemove.end(); ++iterRemove) {
    delete (m_plugInstanceInfos[*iterRemove]);
    m_plugInstanceInfos.erase(*iterRemove);
    m_iterPlugInstances = m_plugInstanceInfos.begin();
  }

  for (std::vector<tPlug>::iterator iterAdd = toAdd.begin();
       iterAdd != toAdd.end(); ++iterAdd) {
    m_plugInstanceInfos[*iterAdd] =
			new PlugInstanceInfo((*iterAdd).get<0>(), (*iterAdd).get<1>());
    m_iterPlugInstances = m_plugInstanceInfos.begin();
  }
	}

void PlugMoveWatcher::checkMovement() {
  for (TrackIterator ti; !ti.end(); ++ti) {
		checkMovementForTrack(*ti);
  }

	checkMovementForTrack(GetMasterTrack(NULL));
	
  signalPlugMoveFinished();
}

int PlugMoveWatcher::connectPlugMoveSignal(const tPlugMoveSignalSlot &slot) {
  m_activePlugMoveConnections[++m_nextConnectionId] =
		signalPlugMove.connect(slot);
  return m_nextConnectionId;
}

void PlugMoveWatcher::disconnectPlugMoveSignal(int connectionId) {
  std::map<int, connection>::iterator iter =
      m_activePlugMoveConnections.find(connectionId);
  if (iter == m_activePlugMoveConnections.end())
    return;
  iter->second.disconnect();
  m_activePlugMoveConnections.erase(iter);
}

int PlugMoveWatcher::connectPlugMoveFinishedSignal(
																									 const tPlugMoveFinishedSignalSlot &slot) {
  m_activePlugMoveConnections[++m_nextConnectionId] =
		signalPlugMoveFinished.connect(slot);
  return m_nextConnectionId;
}

void PlugMoveWatcher::disconnectPlugMoveFinishedSignal(int connectionId) {
  std::map<int, connection>::iterator iter =
      m_activePlugMoveConnections.find(connectionId);
  if (iter == m_activePlugMoveConnections.end())
    return;
  iter->second.disconnect();
  m_activePlugMoveConnections.erase(iter);
}
