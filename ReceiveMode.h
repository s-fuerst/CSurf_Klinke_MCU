/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "csurf_mcu.h"
#include "sendreceivemodebase.h"

class ReceiveMode : public SendReceiveModeBase {
public:
  ReceiveMode(CCSManager *pManager);
  virtual ~ReceiveMode(void);

  void activate();

  bool buttonSelect(int channel, bool pressed);

  void updateAssignmentDisplay();

protected:
  void getSendInfos(std::vector<void *> *pResult, ESendInfo sendInfo);
  void *getSendInfo(ESendInfo sendInfo, int iTrack);
  void setSendInfo(ESendInfo sendInfo, int iTrack, void *pValue, int wait);

  int calcSendIdx(int sendNr);

  const char *stringForESendInfo(ESendInfo sendInfo);
};
