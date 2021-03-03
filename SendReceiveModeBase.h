/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "ccsmode.h"
#include "csurf_mcu.h"
#include "vector"

#define WAIT_FOR_MORE_MOVEMENT 200

enum ESendInfo { TRACK = 0, MUTE = 1, PHASE = 2, MONO = 3, VOL = 4, PAN = 5,
	AUTOMODE = 6 };
// "P_DESTTRACK" (read only, returns MediaTrack *, destination track, only
// applies for sends/recvs) "P_SRCTRACK" (read only, returns MediaTrack *,
// source track, only applies for sends/recvs)

// "B_MUTE" (returns bool *, read/write)
// "B_PHASE" (returns bool *, read/write - true to flip phase)
// "B_MONO" (returns bool *, read/write)
// "D_VOL" (returns double *, read/write - 1.0 = +0dB etc)
// "D_PAN" (returns double *, read/write - -1..+1)
// I_AUTOMODE : int * : automation mode (-1=use track automode, 0=trim/off, 1=read, 2=touch, 3=write, 4=latch)

class SendReceiveModeBase : public CCSMode {
public:
  enum ECategory { SEND = 0, RECEIVE = -1 };

  SendReceiveModeBase(CCSManager *pManager);
  virtual ~SendReceiveModeBase(void);

  virtual void activate();

	virtual bool buttonRec(int channel, bool pressed);
	
  virtual bool buttonFaderBanks(int button, bool pressed);
  virtual bool buttonFlip(bool pressed);

  virtual bool buttonMute(int channel, bool pressed);
  virtual bool buttonSolo(int channel, bool pressed);

  virtual bool fader(int channel, int value);
  virtual bool faderTouched(int channel, bool touched);

  virtual bool
  vpotMoved(int channel,
            int numSteps); // numSteps are negativ for left rotation

	virtual void updateRecLEDs();
  virtual void updateSoloLEDs();
  virtual void updateMuteLEDs();
  virtual void updateFlipLED();

  virtual void updateDisplay();

  virtual void updateFaders();
  virtual void updateVPOTs();

  virtual void trackName(MediaTrack *trackid, const char *pName);

  virtual void frameUpdate();

  virtual bool somethingTouched(
      bool touched); // is called when the first fader is touched or the last
                     // fader touch is released (incl. a short delay)

  int getNumSends();

	
protected:
  virtual void getSendInfos(std::vector<void *> *pResult,
                            ESendInfo sendInfo) = 0;
  virtual void *getSendInfo(ESendInfo sendInfo, int iTrack) = 0;
  virtual void setSendInfo(ESendInfo sendInfo, int iTrack, void *pValue,
                           int wait = 0) = 0;

  virtual int calcSendIdxGet(int sendNr) = 0;
  virtual int calcSendIdxSet(int sendNr) = 0;

	virtual void getTrackUIVol(MediaTrack *track, int idx,
														 double *volumeOut, double *panOut) = 0;
	virtual int getTrackUIOffset() = 0;
	
  virtual const char *stringForESendInfo(ESendInfo sendInfo);

  virtual void writeTrackName(int startPos);

  Display *m_pDisplay;

  char *m_pSendOrReceiveText;

  std::vector<void *> m_sendInfos;

  MediaTrack
      *m_pLastSelectedTrack; // in the case that the send mode is activated with
                             // a different track, m_startWithSend is reset to 0
  int m_startWithSend;

  bool m_flip;
};
