/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "csurf_mcu.h"
#include "SendReceiveModeBase.h"

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

  int calcSendIdxGet(int sendNr);
  int calcSendIdxSet(int sendNr);

	void getTrackUIVol(MediaTrack *track, int idx, double *volumeOut,
										 double *panOut);
	int getTrackUIOffset() { return 0; };
	
  const char *stringForESendInfo(ESendInfo sendInfo);
};
