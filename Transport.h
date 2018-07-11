/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifndef MCU_TRANSPORT
#define MCU_TRANSPORT

#include "csurf.h"

class CSurf_MCU;

#define STARTING_REPEAT_TIME 300
#define REPEAT_TIME_REDUCING_FACTOR 0.8

class Transport {
 public:
  enum Client {
    POSITION,
    MARKER,
    NUDGE
  };

  enum Direction {
    REWIND,
    NO_REEL,
    FORWARD
  };

 private:
  Client m_client;
  Direction m_direction;
  int m_playStateBeforeReel;
  CSurf_MCU* m_pMCU;
  Client m_clientButtonPressed; // Marker or Nudge button is pressed down
  DWORD m_ffwdPressTime;
  DWORD m_rewindPressTime;
  double m_repeatTime;

 public:
  Transport::Transport(CSurf_MCU* p_mcu);

  void handleButton(Client button, bool buttonDown);
  void updateLeds();
  void rewindButton(bool down);

  Client getActualClient();
  void forwardButton(bool down);
  void stopButton(bool down);
  void playButton(bool down);
  void recordButton(bool down);
  void rewindDC();
  void forwardDC();
  void rewind();
  void forward();
  DWORD getFfwdPressTime() { return m_ffwdPressTime; }
  DWORD getRewindPressTime() { return m_rewindPressTime; }

 private:
  void Transport::setClient(Client new_client);

 private:
  void Transport::startReel(Direction dir);
  void Transport::endReel();
};

#endif
