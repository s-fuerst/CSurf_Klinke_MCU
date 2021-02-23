/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "ccsmode.h"
#include "csurf_mcu.h"
#include "vector"

#define WAIT_FOR_MORE_MOVEMENT 200

enum ESendInfo { TRACK = 0, MUTE = 1, PHASE = 2, MONO = 3, VOL = 4, PAN = 5 };
// "P_DESTTRACK" (read only, returns MediaTrack *, destination track, only
// applies for sends/recvs) "P_SRCTRACK" (read only, returns MediaTrack *,
// source track, only applies for sends/recvs)

// "B_MUTE" (returns bool *, read/write)
// "B_PHASE" (returns bool *, read/write - true to flip phase)
// "B_MONO" (returns bool *, read/write)
// "D_VOL" (returns double *, read/write - 1.0 = +0dB etc)
// "D_PAN" (returns double *, read/write - -1..+1)
// "D_PANLAW" (returns double *,read/write - 1.0=+0.0db, 0.5=-6dB, -1.0 =
// projdef etc) "I_SENDMODE" (returns int *, read/write - 0=post-fader,
// 1=pre-fx, 2=post-fx(depr), 3=post-fx) "I_SRCCHAN" (returns int *, read/write
// - index,&1024=mono, -1 for none) "I_DSTCHAN" (returns int *, read/write -
// index, &1024=mono, otherwise stereo pair, hwout:&512=rearoute) "I_MIDIFLAGS"
// (returns int *, read/write - low 5 bits=source channel 0=all, 1-16, next 5
// bits=dest channel, 0=orig, 1-16=chan)

class SendReceiveModeBase : public CCSMode {
public:
  enum ECategory { SEND = 0, RECEIVE = -1 };

  SendReceiveModeBase(CCSManager *pManager);
  virtual ~SendReceiveModeBase(void);

  virtual void activate();

  virtual bool buttonFaderBanks(int button, bool pressed);
  virtual bool buttonFlip(bool pressed);

  virtual bool buttonMute(int channel, bool pressed);
  virtual bool buttonSolo(int channel, bool pressed);

  virtual bool fader(int channel, int value);
  virtual bool faderTouched(int channel, bool touched);

  virtual bool
  vpotMoved(int channel,
            int numSteps); // numSteps are negativ for left rotation

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

  virtual int calcSendIdx(int sendNr) = 0;

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
