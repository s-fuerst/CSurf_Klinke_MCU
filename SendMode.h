/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "csurf_mcu.h"
#include "sendreceivemodebase.h"

class SendMode : public SendReceiveModeBase {
public:
  SendMode(CCSManager *pManager);
  virtual ~SendMode(void);

  void activate();

  void updateAssignmentDisplay();

  bool buttonSelect(int channel, bool pressed);

protected:
  void getSendInfos(std::vector<void *> *pResult, ESendInfo sendInfo);
  void *getSendInfo(ESendInfo sendInfo, int iTrack);
  void setSendInfo(ESendInfo sendInfo, int iTrack, void *pValue, int wait);

  int calcSendIdx(int sendNr);

  const char *stringForESendInfo(ESendInfo sendInfo);
};
