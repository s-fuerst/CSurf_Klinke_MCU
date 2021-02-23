/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "SendMode.h"
#include "Display.h"
#include "ccsmanager.h"
#include "csurf_mcu.h"
#include "UndoEnd.h"

SendMode::SendMode(CCSManager *pManager) : SendReceiveModeBase(pManager) {
  m_pSendOrReceiveText = "Sends Track ";
}

SendMode::~SendMode(void) {}

void SendMode::getSendInfos(std::vector<void *> *pR_SendInfos,
                            ESendInfo sendInfo) {
  //  REAPER_PLUGIN_DECLARE_APIFUNCS void *(*GetSetTrackSendInfo)(MediaTrack
  //  *tr, int category, int sendidx, const char *parmname, void *setNewValue);
  pR_SendInfos->clear();

  const char *pName = stringForESendInfo(sendInfo);
  int iSendNum = 0;
  void *pSendInfo = NULL;
  do {
    pSendInfo =
        GetSetTrackSendInfo(selectedTrack(), SEND, iSendNum++, pName, NULL);
    if (pSendInfo != NULL) {
      pR_SendInfos->push_back(pSendInfo);
    }
  } while (pSendInfo != NULL);
}

void *SendMode::getSendInfo(ESendInfo sendInfo, int iTrack) {
  return GetSetTrackSendInfo(selectedTrack(), SEND, iTrack,
                             stringForESendInfo(sendInfo), NULL);
}

void SendMode::setSendInfo(ESendInfo sendInfo, int iTrack, void *pValue,
                           int wait) {
  UndoEnd::instance()->callUndoBegin();
  GetSetTrackSendInfo(selectedTrack(), SEND, iTrack,
                      stringForESendInfo(sendInfo), pValue);
  UndoEnd::instance()->callUndoEnd("Send Status changed (via Surface)",
                                   UNDO_STATE_TRACKCFG, wait);
}

int SendMode::calcSendIdx(int sendNr) {
  return sendNr + GetTrackNumSends(selectedTrack(), 1);
}

const char *SendMode::stringForESendInfo(ESendInfo sendInfo) {
  switch (sendInfo) {
  case TRACK:
    return "P_DESTTRACK";
  default:
    return SendReceiveModeBase::stringForESendInfo(sendInfo);
  }
}

void SendMode::activate() { SendReceiveModeBase::activate(); }

void SendMode::updateAssignmentDisplay() {
  m_pCCSManager->setAssignmentDisplay(this, "SE");
}

bool SendMode::buttonSelect(int channel, bool pressed) {
  if (pressed) {
    MediaTrack *pTrack =
        (MediaTrack *)getSendInfo(TRACK, m_startWithSend + channel - 1);
    if (pTrack) {
      m_pCCSManager->getMCU()->UnselectAllTracks();
      CSurf_OnSelectedChange(pTrack, 1);
      m_pCCSManager->setMode(CCSManager::RECEIVE);
    }
  }

  return true;
}
