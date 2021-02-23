/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "ReceiveMode.h"
#include "Display.h"
#include "ccsmanager.h"
#include "csurf_mcu.h"
#include "undoend.h"

ReceiveMode::ReceiveMode(CCSManager *pManager) : SendReceiveModeBase(pManager) {
  m_pSendOrReceiveText = "Receive Track ";
}

ReceiveMode::~ReceiveMode(void) {}

void ReceiveMode::getSendInfos(std::vector<void *> *pR_SendInfos,
                               ESendInfo sendInfo) {
  //  REAPER_PLUGIN_DECLARE_APIFUNCS void *(*GetSetTrackSendInfo)(MediaTrack
  //  *tr, int category, int sendidx, const char *parmname, void *setNewValue);
  pR_SendInfos->clear();

  const char *pName = stringForESendInfo(sendInfo);
  int iSendNum = 0;
  void *pSendInfo = NULL;
  do {
    pSendInfo =
        GetSetTrackSendInfo(selectedTrack(), RECEIVE, iSendNum++, pName, NULL);
    if (pSendInfo != NULL) {
      pR_SendInfos->push_back(pSendInfo);
    }
  } while (pSendInfo != NULL);
}

void *ReceiveMode::getSendInfo(ESendInfo sendInfo, int iTrack) {
  return GetSetTrackSendInfo(selectedTrack(), RECEIVE, iTrack,
                             stringForESendInfo(sendInfo), NULL);
}

void ReceiveMode::setSendInfo(ESendInfo sendInfo, int iTrack, void *pValue,
                              int wait) {
  UndoEnd::instance()->callUndoBegin();
  GetSetTrackSendInfo(selectedTrack(), RECEIVE, iTrack,
                      stringForESendInfo(sendInfo), pValue);
  UndoEnd::instance()->callUndoEnd("Receive Status changed (via Surface)",
                                   UNDO_STATE_TRACKCFG, wait);
}

int ReceiveMode::calcSendIdx(int sendNr) { return -sendNr - 1; }

const char *ReceiveMode::stringForESendInfo(ESendInfo sendInfo) {
  switch (sendInfo) {
  case TRACK:
    return "P_SRCTRACK";
  default:
    return SendReceiveModeBase::stringForESendInfo(sendInfo);
  }
}

void ReceiveMode::activate() { SendReceiveModeBase::activate(); }

void ReceiveMode::updateAssignmentDisplay() {
  m_pCCSManager->setAssignmentDisplay(this, "RE");
}

bool ReceiveMode::buttonSelect(int channel, bool pressed) {
  if (pressed) {
    MediaTrack *pTrack =
        (MediaTrack *)getSendInfo(TRACK, m_startWithSend + channel - 1);
    if (pTrack) {
      m_pCCSManager->getMCU()->UnselectAllTracks();
      CSurf_OnSelectedChange(pTrack, 1);
      m_pCCSManager->setMode(CCSManager::SEND);
    }
  }

  return true;
}
