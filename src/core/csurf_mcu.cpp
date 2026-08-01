/*
** reaper_csurf
** MCU support
** Copyright (C) 2006-2008 Cockos Incorporated
** License: LGPL.
*/

// JUCE 1.52 internal event dispatcher — defined in juce_amalgamated.cpp
// inside namespace juce.  Processes one pending X11/JUCE event and returns
// true, or returns false immediately (no sleep) when the queue is empty.
namespace juce { namespace detail {
    bool dispatchNextMessageOnSystemQueue(bool returnIfNoPendingMessages);
}}

#include "csurf_mcu.h"
#include "McuDebugLog.h"
#include "math.h"
#include "Transport.h"
#include "Region.h"
#include "McuAssert.h"
#include "DisplayHandler.h"
#include "Display.h"
#include "MultiTrackMode.h"
#include "UndoEnd.h"
#include "Tracks.h"
#include "JuceHeader.h"
#include "boost/bind.hpp"
#include "ProjectConfig.h"
#include "PlugMoveWatcher.h"
#include "PlugMode.h"
#include "ButtonManager.h"
#include "ActionsDisplay.h"
#include "ActionsDialogComponent.h"
#include "CCSModesEditor.h"
#include "Actions.h"

/*
	MCU documentation:

	MCU=>PC:
  The MCU seems to send, when it boots (or is reset) F0 00 00 66 14 01 58 59 5A 57 18 61 05 57 18 61 05 F7

  Ex vv vv    :   volume fader move, x=0..7, 8=master, vv vv is int14
  B0 1x vv    :   pan fader move, x=0..7, vv has 40 set if negative, low bits 0-31 are move amount
  B0 3C vv    :   jog wheel move, 01 or 41

  to the extent the buttons below have LEDs, you can set them by sending these messages, with 7f for on, 1 for blink, 0 for off.
  90 0x vv    :   rec arm push x=0..7 (vv:..)
  90 0x vv    :   solo push x=8..F (vv:..)
  90 1x vv    :   mute push x=0..7 (vv:..)
  90 1x vv    :   selected push x=8..F (vv:..)
  90 2x vv    :   pan knob push, x=0..7 (vv:..)
  90 28 vv    :   assignment track
  90 29 vv    :   assignment send
  90 2A vv    :   assignment pan/surround
  90 2B vv    :   assignment plug-in
  90 2C vv    :   assignment EQ
  90 2D vv    :   assignment instrument
  90 2E vv    :   bank down button (vv: 00=release, 7f=push)
  90 2F vv    :   channel down button (vv: ..)
  90 30 vv    :   bank up button (vv:..)
  90 31 vv    :   channel up button (vv:..)
  90 32 vv    :   flip button
  90 33 vv    :   global view button
  90 34 vv    :   name/value display button
  90 35 vv    :   smpte/beats mode switch (vv:..)
  90 36 vv    :   F1
  90 37 vv    :   F2
  90 38 vv    :   F3
  90 39 vv    :   F4
  90 3A vv    :   F5
  90 3B vv    :   F6
  90 3C vv    :   F7
  90 3D vv    :   F8
  90 3E vv    :   Global View : midi tracks
  90 3F vv    :   Global View : inputs
  90 40 vv    :   Global View : audio tracks
  90 41 vv    :   Global View : audio instrument
  90 42 vv    :   Global View : aux
  90 43 vv    :   Global View : busses
  90 44 vv    :   Global View : outputs
  90 45 vv    :   Global View : user
  90 46 vv    :   shift modifier (vv:..)
  90 47 vv    :   option modifier
  90 48 vv    :   control modifier
  90 49 vv    :   alt modifier
  90 4A vv    :   automation read/off
  90 4B vv    :   automation write
  90 4C vv    :   automation trim
  90 4D vv    :   automation touch
  90 4E vv    :   automation latch
  90 4F vv    :   automation group
  90 50 vv    :   utilities save
  90 51 vv    :   utilities undo
  90 52 vv    :   utilities cancel
  90 53 vv    :   utilities enter
  90 54 vv    :   marker
  90 55 vv    :   nudge
  90 56 vv    :   cycle
  90 57 vv    :   drop
  90 58 vv    :   replace
  90 59 vv    :   click
  90 5a vv    :   solo
  90 5b vv    :   transport rewind (vv:..)
  90 5c vv    :   transport ffwd (vv:..)
  90 5d vv    :   transport pause (vv:..)
  90 5e vv    :   transport play (vv:..)
  90 5f vv    :   transport record (vv:..)
  90 60 vv    :   up arrow button  (vv:..)
  90 61 vv    :   down arrow button 1 (vv:..)
  90 62 vv    :   left arrow button 1 (vv:..)
  90 63 vv    :   right arrow button 1 (vv:..)
  90 64 vv    :   zoom button (vv:..)
  90 65 vv    :   scrub button (vv:..)

  90 6x vv    :   fader touch x=8..f
  90 70 vv    :   master fader touch

	PC=>MCU:

  F0 00 00 66 14 12 xx <data> F7   : update LCD. xx=offset (0-112), string. display is 55 chars wide, second line begins at 56, though.
  F0 00 00 66 14 08 00 F7          : reset MCU
  F0 00 00 66 14 20 0x 03 F7       : put track in VU meter mode, x=track  

  90 73 vv : rude solo light (vv: 7f=on, 00=off, 01=blink)

  B0 3x vv : pan display, x=0..7, vv=1..17 (hex) or so
  B0 4x vv : right to left of LEDs. if 0x40 set in vv, dot below char is set (x=0..11)

  D0 yx    : update VU meter, y=track, x=0..d=volume, e=clip on, f=clip off
  Ex vv vv : set volume fader, x=track index, 8=master


*/

#define SPLASH_MESSAGE "REAPER! Initializing... Please wait..."

#define MASTER_GUID 

int CSurf_MCU::s_iNumInstances = 0;

int CSurf_MCU::s_mackie_modifiers = 0;

int CSurf_MCU::s_cfg_flags = 0;

/*
	static unsigned int get_midi_evt_code( MIDI_event_t *evt ) {
  unsigned int code = 0;
  code |= (evt->midi_message[0]<<24);
  code |= (evt->midi_message[1]<<16);
  code |= (evt->midi_message[2]<<8);
  code |= evt->size > 3 ? evt->midi_message[3] : 0;
  return code;
	}
*/
int CSurf_MCU::FindTrackNr(MediaTrack *tr) {
  int iNr = 1;
  for (TrackIterator ti; !ti.end(); ++ti, ++iNr) {
    if (*ti == tr)
      return iNr;
  }
  return 0;
}

const char *CSurf_MCU::GetTrackName(MediaTrack *tr) {
  if (!tr) {
    memset(trackName, ' ', 4);
    return trackName;
  }

	if (tr == GetMasterTrack(NULL))
		return "Master";

  const char *pTrackName = (char *)(*GetSetMediaTrackInfo)(tr, "P_NAME", NULL);
  if (pTrackName != NULL && strnlen(pTrackName, 55) > 0)
    return pTrackName;

  //  char buf[32];
  int trackno = m_pCCSManager->getMCU()->FindTrackNr(tr);
  if (trackno < 100)
    sprintf(trackName, "%02d", trackno);
  else
    sprintf(trackName, "%d", trackno);
  return trackName;
}

MediaTrack *CSurf_MCU::TrackFromGUID(const GUID &guid) {
	if (guid == GUID_MASTER)
		return GetMasterTrack(NULL);
	
  for (TrackIterator ti; !ti.end(); ++ti) {
    MediaTrack *tr = *ti;
    const GUID *tguid = GetTrackGUID(tr);

    if (tr && tguid && !memcmp(tguid, &guid, sizeof(GUID)))
      return tr;
  }
  return NULL;
}

// this wraps GetTrackGUID and also support the MasterTrack
// via an own GUID
GUID *CSurf_MCU::GUIDfromTrack(MediaTrack *tr) {
	if (tr == GetMasterTrack(NULL))
		return &GUID_MASTER;

	return GetTrackGUID(tr);
}

bool CSurf_MCU::SomethingSoloed() {
  for (TrackIterator ti; !ti.end(); ++ti) {
    MediaTrack *tr = *ti;
    int *OriginalState = (int *)GetSetMediaTrackInfo(tr, "I_SOLO", NULL);
    if (*OriginalState > 0)
      return true;
  }

  return false;
}

void CSurf_MCU::ScheduleAction(DWORD time, ScheduleFunc func) {
  ScheduledAction *action = new ScheduledAction(time, func);
  if (m_schedule == NULL) {
    m_schedule = action;
  } else if (action->time < m_schedule->time) {
    action->next = m_schedule;
    m_schedule = action;
  } else {
    ScheduledAction *curr = m_schedule;
    while (curr->next != NULL && curr->next->time < action->time)
      curr = curr->next;
    action->next = curr->next;
    curr->next = action;
  }
}

void CSurf_MCU::MCUReset() {
  m_pButtonManager->reset();
  memset(m_mackie_lasttime, 0, sizeof(m_mackie_lasttime));
  m_mackie_lasttime_mode = -1;
  s_mackie_modifiers = 0;
  m_buttonstate_lastrun = 0;
  m_button_states = 0;
  m_mackie_arrow_states = 0;

  m_dropstate.updateReaper();

  // code from Justin, i don't really understand what this is doing
  int sz;
	// this can be done once, and stored (for speed)
  m_metronom_offset = projectconfig_var_getoffs("projmetroen", &sz); 
  if (sz != 4)
    m_metronom_offset = 0;

  // show splash on all units using per-unit splash displays
  for (size_t ui = 0; ui < m_pSplashDisplays.size() && ui < m_units.size(); ui++) {
    DisplayHandler *dh = m_units[ui]->displayHandler();
    if (dh) {
      m_pSplashDisplays[ui]->changeTextFullLine(0, SPLASH_MESSAGE);
      m_pSplashDisplays[ui]->clearLine(1);
      dh->switchTo(m_pSplashDisplays[ui]);
    }
  }
}

void CSurf_MCU::CallTransportForward() {
  if (timeGetTime() - m_pTransport->getFfwdPressTime() > STARTING_REPEAT_TIME)
    m_pTransport->forward();
}

void CSurf_MCU::CallTransportRewind() {
  if (timeGetTime() - m_pTransport->getRewindPressTime() > STARTING_REPEAT_TIME)
    m_pTransport->rewind();
}

bool CSurf_MCU::OnMCUReset(MIDI_event_t *evt) {
  unsigned char onResetMsg[] = {
		0xf0, 0x00, 0x00, 0x66, 0x14, 0x01, 0x58, 0x59, 0x5a,
  };
  onResetMsg[4] = m_is_mcuex ? 0x15 : 0x14;
  if (evt->midi_message[0] == 0xf0 && evt->size >= sizeof(onResetMsg) &&
      !memcmp(evt->midi_message, onResetMsg, sizeof(onResetMsg))) {
    // on reset
    MCUReset();
    // Republish state after cache invalidation
    if (m_pTransport)
      m_pTransport->updateLeds();
    SetLED(B_DROP, m_dropstate.ledState());
    m_pCCSManager->updateFlipLED();
    m_pCCSManager->updateGlobalViewLED();
    SetLED(B_ZOOM, (m_mackie_arrow_states & ARROW_STATE_ZOOM) ? 0x7f : 0);
    SetLED(B_SCRUB, (m_mackie_arrow_states & ARROW_STATE_SCRUB) ? 0x7f : 0);
    TrackList_UpdateAllExternalSurfaces();
    return true;
  }
  return false;
}

bool CSurf_MCU::OnFaderMove(MIDI_event_t *evt) {
  if ((evt->midi_message[0] & 0xf0) == 0xe0) { // volume fader move
    int tid = evt->midi_message[0] & 0xf;
    if (tid == 8)
      tid = 0;
    else
      tid++;

    // translate local channel → global (master tid=0 is unchanged)
    if (tid != 0)
      tid += m_currentInputOffset;

    return m_pCCSManager->fader(tid,
																msbLsbToInt(evt->midi_message[2],
																						evt->midi_message[1]));
  }
  return false;
}

bool CSurf_MCU::OnRotaryEncoder(MIDI_event_t *evt) {
  if ((evt->midi_message[0] & 0xf0) == 0xb0 && evt->midi_message[1] >= 0x10 &&
      evt->midi_message[1] < 0x18) { // pan
    int tid = evt->midi_message[1] - 0x10;

    // translate local VPOT → global channel
    int channel = tid + 1 + m_currentInputOffset;

    m_pan_lasttouch[Tracks::instance()->getMediaTrackForChannel(channel)] =
			timeGetTime();

    int adj = (evt->midi_message[2] & 0x3f);
    if (evt->midi_message[2] & 0x40)
      adj = -adj;

    return m_pCCSManager->vpotMoved(channel, adj);
  }
  return false;
}

bool CSurf_MCU::OnVPOTAssign(MIDI_event_t *evt) {
  return m_pCCSManager->buttonVPOTassign(evt->midi_message[1],
                                         evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnJogWheel(MIDI_event_t *evt) {
  // jog wheel is global — only accept from primary unit
  if (m_currentInputOffset != 0)
    return false;
  if ((evt->midi_message[0] & 0xf0) == 0xb0 &&
      evt->midi_message[1] == 0x3c) // jog wheel
		{
			// MCU jog sends 0x41-0x7F for backward, 0x01-0x3F for forward.
			// Use the lower 6 bits as signed speed so fast turns move further
			// per event (1..63 steps) than slow turns.
			int dir;
			if (evt->midi_message[2] >= 0x41) {
				dir = -(evt->midi_message[2] & 0x3F);
				if (IsNoModifierPressed()) {
					CSurf_OnRew(m_mackie_arrow_states & ARROW_STATE_SCRUB);
					return true;
				}
			} else if (evt->midi_message[2] >= 0x01) {
				dir = evt->midi_message[2] & 0x3F;
				if (IsNoModifierPressed()) {
					CSurf_OnFwd(m_mackie_arrow_states & ARROW_STATE_SCRUB);
					return true;
				}
			}

			if (IsModifierPressed(VK_SHIFT) || IsModifierPressed(VK_OPTION)) {
				m_region.GetFromActualRegion(false);
				if (!Region::IsActive(Region::TIME)) {
					m_region.SetStart(::GetCursorPosition());
					m_region.SetEnd(::GetCursorPosition());
				}
				if (IsModifierPressed(VK_SHIFT)) {
					m_region.SetStart(CalcMovement(m_region.GetStart(), dir));
				}
				if (IsModifierPressed(VK_OPTION)) {
					m_region.SetEnd(CalcMovement(m_region.GetEnd(), dir));
				}
				m_region.Set(false);
			}

			if (IsModifierPressed(VK_CONTROL) || IsModifierPressed(VK_ALT)) {
				m_region.GetFromActualRegion(true);
				if (!Region::IsActive(Region::LOOP)) {
					m_region.SetStart(::GetCursorPosition());
					m_region.SetEnd(::GetCursorPosition());
				}
				if (IsModifierPressed(VK_CONTROL)) {
					m_region.SetStart(CalcMovement(m_region.GetStart(), dir));
				}
				if (IsModifierPressed(VK_ALT)) {
					m_region.SetEnd(CalcMovement(m_region.GetEnd(), dir));
					if ((GetPlayState() == 1) && (m_region.GetEnd() < GetPlayPosition())) {
						CSurf_OnStop();
						SetEditCurPos(m_region.GetStart(), false, false);
						CSurf_OnPlay();
					}
				}
				m_region.Set(true);
			}

			return true;
		}
  return false;
}

double CSurf_MCU::CalcMovement(double oldPos, int dir) {
  double bpm, bpi;
  GetProjectTimeSignature(&bpm, &bpi);
  double pixelPerBeat = GetHZoomLevel() * 60 / bpm;

  if (m_mackie_arrow_states & ARROW_STATE_SCRUB) {
    return oldPos + (1 / GetHZoomLevel()) * dir * 2;
  } else {
    if (pixelPerBeat < 20) {
      return MoveInBars(oldPos, dir);
    } else {
      return MoveInBeats(oldPos, dir);
    }
  }
}

bool CSurf_MCU::OnAutoMode(MIDI_event_t *evt) {
  AutoMode mode = AutoMode::AUTO_MODE_TRIM;
  int a = evt->midi_message[1] - 0x4a;
  if (!a)
    mode = AUTO_MODE_READ;
  else if (a == 1)
    mode = AUTO_MODE_WRITE;
  else if (a == 2)
    mode = AUTO_MODE_TRIM;
  else if (a == 3)
    mode = AUTO_MODE_TOUCH;
  else if (a == 4)
    mode = AUTO_MODE_LATCH;

  if (mode >= 0) {
		if (!m_pCCSManager->setAutoMode(mode))
			SetAutomationMode(mode, !IsModifierPressed(VK_CONTROL));
	}
	//    SetAutomationMode(mode, !IsModifierPressed(VK_CONTROL));

  return true;
}

bool CSurf_MCU::OnBankChannel(MIDI_event_t *evt) {
  return m_pCCSManager->buttonFaderBanks(evt->midi_message[1],
                                         evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnSMPTEBeats(MIDI_event_t *evt) {
  int *tmodeptr =
		(int *)projectconfig_var_addr(NULL, __g_projectconfig_timemode2);
  if (tmodeptr && *tmodeptr >= 0) {
    (*tmodeptr)++;
    if ((*tmodeptr) > 5)
      (*tmodeptr) = 0;
  } else {
    tmodeptr = (int *)projectconfig_var_addr(NULL, __g_projectconfig_timemode);

    if (tmodeptr) {
      (*tmodeptr)++;
      if ((*tmodeptr) > 5)
        (*tmodeptr) = 0;
    }
  }
  UpdateTimeline();
  Main_UpdateLoopInfo(0);

  return true;
}

bool CSurf_MCU::OnRotaryEncoderPush(MIDI_event_t *evt) {
  int trackid = evt->midi_message[1] - 0x20;
  // translate local VPOT push → global channel
  int channel = trackid + 1 + m_currentInputOffset;
  m_pan_lasttouch[Tracks::instance()->getMediaTrackForChannel(channel)] =
		timeGetTime();

  m_pCCSManager->vpotPressed(channel, evt->midi_message[2] > 0x3f);

  return true;
}

bool CSurf_MCU::OnRecArm(MIDI_event_t *evt) {
  return m_pCCSManager->buttonRec(evt->midi_message[1] + 1 + m_currentInputOffset,
                                  evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnRecArmDC(MIDI_event_t *evt) {
  return m_pCCSManager->buttonRecDC(evt->midi_message[1] + 1 + m_currentInputOffset,
                                    evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnMute(MIDI_event_t *evt) {
  return m_pCCSManager->buttonMute(evt->midi_message[1] - 0x0f + m_currentInputOffset,
                                   evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnSolo(MIDI_event_t *evt) {
  return m_pCCSManager->buttonSolo(evt->midi_message[1] - 0x07 + m_currentInputOffset,
                                   evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnSoloDC(MIDI_event_t *evt) {
  return m_pCCSManager->buttonSoloDC(evt->midi_message[1] - 0x07 + m_currentInputOffset);
}

bool CSurf_MCU::OnChannelSelect(MIDI_event_t *evt) {
  return m_pCCSManager->buttonSelect(evt->midi_message[1] - 0x17 + m_currentInputOffset,
                                     evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnChannelSelectDC(MIDI_event_t *evt) {
  return m_pCCSManager->buttonSelectDC(evt->midi_message[1] - 0x17 + m_currentInputOffset);
}

bool CSurf_MCU::OnChannelSelectLong(int channel) {
  return m_pCCSManager->buttonSelectLong(channel);
}

bool CSurf_MCU::OnTransport(MIDI_event_t *evt) {
  bool down = evt->midi_message[2] > 0x40;
  switch (evt->midi_message[1]) {
  case B_RECORD:
    m_dropstate.updateReaper();
    m_pTransport->recordButton(down);
    break;
  case B_PLAY:
    m_pTransport->playButton(down);
    break;
  case B_STOP:
    m_pTransport->stopButton(down);
    break;
  case B_REWIND:
    m_pTransport->rewindButton(down);
    break;
  case B_FFWD:
    m_pTransport->forwardButton(down);
    break;
  }
  return true;
}

bool CSurf_MCU::OnTransportDC(MIDI_event_t *evt) {
  switch (evt->midi_message[1]) {
  case B_RECORD:

    break;
  case B_PLAY:
    break;
  case B_STOP:
    if (IsModifierPressed(VK_ALT) || IsModifierPressed(VK_OPTION)) {
      double lStart, lEnd;
      bool asLoop = IsModifierPressed(VK_ALT);
      GetSet_LoopTimeRange(false, asLoop, &lStart, &lEnd, false);
      if (IsModifierPressed(VK_SHIFT)) {
        lStart = QuantizeTimeToBeat(lStart);
        lEnd = QuantizeTimeToBeat(lEnd);
      } else {
        lStart = QuantizeTimeToBar(lStart);
        lEnd = QuantizeTimeToBar(lEnd);
      }
      Undo_BeginBlock();
      GetSet_LoopTimeRange(true, asLoop, &lStart, &lEnd, false);
      SetEditCurPos(lStart, false, false);
      Undo_EndBlock("Time Selection Change (via Surface)", UNDO_STATE_MISCCFG);
    } else {
      if (IsModifierPressed(VK_SHIFT))
        SetEditCurPos(QuantizeTimeToBeat(GetCursorPosition()), false, false);
      else
        SetEditCurPos(QuantizeTimeToBar(GetCursorPosition()), false, false);
    }
    break;
  case B_REWIND:
    break;
  case B_FFWD:
    break;
  }
  return true;
}

bool CSurf_MCU::OnMarker(MIDI_event_t *evt) {
  m_pTransport->handleButton(Transport::MARKER, evt->midi_message[2] > 0x40);
  return true;
}

bool CSurf_MCU::OnNudge(MIDI_event_t *evt) {
  m_pTransport->handleButton(Transport::NUDGE, evt->midi_message[2] > 0x40);
  return true;
}

bool CSurf_MCU::OnCycle(MIDI_event_t *evt) {
  SendMessage(g_hwnd, WM_COMMAND, IDC_REPEAT, 0);
  return true;
}

bool CSurf_MCU::OnClick(MIDI_event_t *evt) {
  SendMessage(g_hwnd, WM_COMMAND, ID_METRONOME, 0);
  return true;
}

void CSurf_MCU::ClearSaveLed() {
  SetLED(B_SAVE, 0);
}

bool CSurf_MCU::OnSave(MIDI_event_t *evt) {
  SetLED(B_SAVE, 0x7f);
  SendMessage(g_hwnd, WM_COMMAND,
              (IsModifierPressed(VK_SHIFT) | IsModifierPressed(VK_ALT))
							? ID_FILE_SAVEAS
							: ID_FILE_SAVEPROJECT,
              0);
  ScheduleAction(timeGetTime() + 1000, &CSurf_MCU::ClearSaveLed);
  return true;
}

void CSurf_MCU::ClearUndoLed() {
  SetLED(B_UNDO, 0);
}

bool CSurf_MCU::OnUndo(MIDI_event_t *evt) {
  SetLED(B_UNDO, 0x7f);
  SendMessage(g_hwnd, WM_COMMAND,
              IsModifierPressed(VK_SHIFT) ? IDC_EDIT_REDO : IDC_EDIT_UNDO, 0);
  ScheduleAction(timeGetTime() + 150, &CSurf_MCU::ClearUndoLed);
  return true;
}

bool CSurf_MCU::OnCancel(MIDI_event_t *evt) {
  SendMessage(g_hwnd, WM_COMMAND, ID_STOP_AND_DELETE_MEDIA, 0);
  return true;
}

bool CSurf_MCU::OnZoom(MIDI_event_t *evt) {
  if (IsModifierPressed(VK_SHIFT)) {
    SendMessage(g_hwnd, WM_COMMAND, ID_ZOOM_OUT_PROJECT, 0);
    return true;
  }
  m_mackie_arrow_states ^= ARROW_STATE_ZOOM;
  SetLED(B_ZOOM, (m_mackie_arrow_states & ARROW_STATE_ZOOM) ? 0x7f : 0);
  return true;
}

bool CSurf_MCU::OnScrub(MIDI_event_t *evt) {
  m_mackie_arrow_states ^= ARROW_STATE_SCRUB;
  SetLED(B_SCRUB, (m_mackie_arrow_states & ARROW_STATE_SCRUB) ? 0x7f : 0);
  return true;
}

bool CSurf_MCU::OnFlip(MIDI_event_t *evt) {
  return m_pCCSManager->buttonFlip(evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnGlobal(MIDI_event_t *evt) {
  return m_pCCSManager->buttonGView(evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnKeyModifier(MIDI_event_t *evt) {
  int mask = (1 << (evt->midi_message[1] - 0x46));
  if (evt->midi_message[2] >= 0x40)
    s_mackie_modifiers |= mask;
  else
    s_mackie_modifiers &= ~mask;

  m_pActionDisplay->switchTo(s_mackie_modifiers);

  return true;
}

bool CSurf_MCU::OnScroll(MIDI_event_t *evt) {
  if (evt->midi_message[2] > 0x40)
    m_mackie_arrow_states |= 1 << (evt->midi_message[1] - 0x60);
  else
    m_mackie_arrow_states &= ~(1 << (evt->midi_message[1] - 0x60));
  return true;
}

bool CSurf_MCU::OnTouch(MIDI_event_t *evt) {
  int fader = evt->midi_message[1] - 0x68;
  // translate local touch → global channel (master fader 8 → channel 0)
  int channel = (fader != 8) ? fader + 1 + m_currentInputOffset : 0;
  m_fader_touchstate[Tracks::instance()->getMediaTrackForChannel(channel)] =
		evt->midi_message[2] >= 0x7f;

  return m_pCCSManager->faderTouched(channel,
                                     evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnFunctionKey(MIDI_event_t *evt) {
  int fkey = evt->midi_message[1] - 0x35;

  if (IsModifierPressed(VK_ALT) || IsModifierPressed(VK_OPTION)) {
    HandleFunctionKeyForRegionsOrLoops(fkey, IsModifierPressed(VK_ALT));
    return true;
  }

  if (IsModifierPressed(VK_SHIFT)) {
    HandleFunctionKeyForRegionsOrLoops(fkey, true);
    return true;
  }

  int command =
		(IsModifierPressed(VK_CONTROL) ? ID_SET_MARKER1 : ID_GOTO_MARKER1) +
		fkey - 1;
  SendMessage(g_hwnd, WM_COMMAND, command, 0);
  return true;
}

void CSurf_MCU::HandleFunctionKeyForRegionsOrLoops(int fkey, bool loop) {
  m_region.GetFromActualRegion(loop);
  if (IsModifierPressed(VK_CONTROL)) {
    m_region.Store(fkey, loop);
  } else if (IsModifierPressed(VK_SHIFT)) {
    Undo_BeginBlock();
    m_region.SetStart(GetCursorPosition());
    double markerPos = m_region.MarkerPos(fkey);
    if (markerPos >= 0) {
      m_region.SetEnd(m_region.MarkerPos(fkey));
      m_region.Set(loop);
    }
    if (loop)
      Undo_EndBlock("Loop changed (via Surface)", UNDO_STATE_MISCCFG);
    else
      Undo_EndBlock("Region changed (via Surface)", UNDO_STATE_MISCCFG);
  } else if (m_region.FindRegion(fkey)) {
    Undo_BeginBlock();
    m_region.Set(loop);
    if (loop)
      Undo_EndBlock("Loop changed (via Surface)", UNDO_STATE_MISCCFG);
    else
      Undo_EndBlock("Region changed (via Surface)", UNDO_STATE_MISCCFG);
  }
}

bool CSurf_MCU::OnGlobalSoloButton(MIDI_event_t *evt) {
  int flags;
  GetTrackInfo(-1, &flags);

  if (flags & 8) {
    SendMessage(g_hwnd, WM_COMMAND, ID_TOGGLE_MASTER_MUTE, 0);
  } else {
    SoloAllTracks(0);
    m_pCCSManager->updateSoloLEDs();
  }
  UpdateGlobalSoloLED();
  return true;
}

bool CSurf_MCU::OnDropButton(MIDI_event_t *evt) {
  m_dropstate.toggleState();
  m_dropstate.updateReaper();
  SetLED(B_DROP, m_dropstate.ledState());
  return true;
}

bool CSurf_MCU::OnButtonPress(MIDI_event_t *evt) {
  // pass unit index so ButtonManager isolates per-unit double-click/long-press state
  int unitIndex = m_currentInputOffset / 8;
  return m_pButtonManager->dispatchMidiEvent(evt, unitIndex);
}

bool CSurf_MCU::OnPedalMove(MIDI_event_t *evt) {
  // pedal is global — only accept from primary unit
  if (m_currentInputOffset != 0)
    return false;
  if (evt->midi_message[0] == 0x90 && evt->midi_message[1] == 0x66) {
    MIDI_event_t sendEvent = {0, 3, {0xB0, 0x05, evt->midi_message[2]}};
    kbd_OnMidiEvent(&sendEvent, -1);
    return true;
  } else if (evt->midi_message[0] == 0x90 && evt->midi_message[1] == 0x67) {
    MIDI_event_t sendEvent = {0, 3, {0xB0, 0x06, evt->midi_message[2]}};
    kbd_OnMidiEvent(&sendEvent, -1);
    return true;
  } else if (evt->midi_message[0] == 0xB0 && evt->midi_message[1] == 0x2E) {
    MIDI_event_t sendEvent = {0, 3, {0xB0, 0x04, evt->midi_message[2]}};
    kbd_OnMidiEvent(&sendEvent, -1);
    return true;
  }

  return false;
}

typedef bool (CSurf_MCU::*MidiHandlerFunc)(MIDI_event_t *);

void CSurf_MCU::OnMIDIEvent(MIDI_event_t *evt) {
#if 0
  char buf[512];
  sprintf(buf,"message %02x, %02x, %02x\n",evt->midi_message[0],evt->midi_message[1],evt->midi_message[2]);
  OutputDebugString(buf);
#endif

  static const int nHandlers = 6;
  static const MidiHandlerFunc handlers[nHandlers] = {
		&CSurf_MCU::OnMCUReset,      &CSurf_MCU::OnFaderMove,
		&CSurf_MCU::OnRotaryEncoder, &CSurf_MCU::OnJogWheel,
		&CSurf_MCU::OnButtonPress,   &CSurf_MCU::OnPedalMove,
  };
  for (int i = 0; i < nHandlers; i++)
    if ((this->*handlers[i])(evt))
      return;
}

CSurf_MCU::CSurf_MCU(const SurfaceConfig &cfg, int *errStats)
    : m_pActionsDialogComponent(NULL), m_currentInputOffset(0) {
  //_CrtSetBreakAlloc(6938);
  if (s_iNumInstances == 0) {
    MCU_LOG_INIT();
    initialiseJuce_GUI();
  }
  s_iNumInstances++;

  m_pButtonManager = new ButtonManager(this);

  Tracks::instance()->setMCU(this);

  // Store the parsed config
  m_surfaceConfig = cfg;

  // diagnostic assert — dense topology should be guaranteed by
  // createFunc / dialog validation; this catches programming errors.
  ASSERT(hasDenseUnitTopology(m_surfaceConfig));

  // Legacy compat shims (used by !m_is_mcuex gating, GetOffset/GetSize, etc.)
  m_is_mcuex = false;
  m_offset = 0;
  m_size = availableChannels(); // = numUnits()*8, 8 for N=1
  m_midi_in_dev = cfg.units[0].midiInDev;
  m_midi_out_dev = cfg.units[0].midiOutDev;
  s_cfg_flags = cfg.flags;

  // construct HardwareUnits for all configured units.
  // Unit 1 (index 0) is always constructed even with MIDI None.
  // Units 2–8: constructed only if real (non-(-1)) MIDI devices are assigned.
  // Duplicate MIDI device checking: warn but don't block.
  {
    // Warn on duplicate MIDI device IDs across units
    for (int i = 0; i < MAX_SURFACE_UNITS; i++) {
      for (int j = i + 1; j < MAX_SURFACE_UNITS; j++) {
        int inI = cfg.units[i].midiInDev, inJ = cfg.units[j].midiInDev;
        int outI = cfg.units[i].midiOutDev, outJ = cfg.units[j].midiOutDev;
        if (inI != -1 && inI == inJ)
          MCU_LOG("duplicate MIDI input device %d on units %d and %d", inI, i + 1, j + 1);
        if (outI != -1 && outI == outJ)
          MCU_LOG("duplicate MIDI output device %d on units %d and %d", outI, i + 1, j + 1);
      }
    }

    for (int i = 0; i < MAX_SURFACE_UNITS; i++) {
      bool hasDevice = (cfg.units[i].midiInDev != -1 || cfg.units[i].midiOutDev != -1);
      bool isUnit1 = (i == 0);

      // Skip disabled units (both MIDI -1 and not a main unit)
      if (!hasDevice && !isUnit1)
        continue;

      HardwareUnit *pUnit = new HardwareUnit(i, cfg.units[i], this, errStats);
      m_units.push_back(pUnit);
    }
  }

  // Cache port pointers from unit 1 (NON-OWNING, for legacy call sites)
  m_midiin = (!m_units.empty()) ? m_units[0]->midiInput() : NULL;
  m_midiout = (!m_units.empty()) ? m_units[0]->midiOutput() : NULL;

  // choose the global-input owner.
  // If unit 0 is transport-capable, it owns global input (N=1: always true).
  // Otherwise use the first transport-capable unit in dense order.
  // -1 means no transport-capable unit → accept no global input.
  m_globalInputUnitIndex = -1;
  if (!m_units.empty() && m_units[0]->isMain())
    m_globalInputUnitIndex = 0;
  else if (hasTransportUnits())
    m_globalInputUnitIndex = firstTransportUnit()->unitIndex();

  // per-unit splash displays (one per HardwareUnit's DisplayHandler)
  for (size_t ui = 0; ui < m_units.size(); ui++) {
    HardwareUnit *u = m_units[ui];
    if (u && u->displayHandler()) {
      m_pSplashDisplays.push_back(new Display(u->displayHandler(), 2));
    }
  }
  m_pActionDisplay = new ActionsDisplay(getDisplayHandler());
  m_pCCSManager =
		new CCSManager(this); // DisplayHandler is constructed per-unit by HardwareUnit ctor

  m_repeatState = false;

  g_mcu_list.Add(this);

  m_selected_tracks = NULL;

  m_mcu_timedisp_lastforce = 0;
  m_frameupd_lastrun = 0;

  // NOTE: MIDI open + JACK usleep workaround + errStats now live in the
  // HardwareUnit ctor above.

  // per-unit reset, then invalidate caches so subsequent sends
  // are not deduped away (caches are stale after hardware reset).
  for (size_t ui = 0; ui < m_units.size(); ui++) {
    m_units[ui]->reset();
    m_units[ui]->invalidateFaderCache();
    m_units[ui]->invalidateLEDCache();
  }

  MCUReset();

  // Start MIDI input on all constructed units
  for (size_t ui = 0; ui < m_units.size(); ui++)
    m_units[ui]->startInput();

  m_schedule = NULL;

  // ensure Tracks knows the channel count BEFORE init/activate,
  // so the first updateFaders() iterates the correct number of strips.
  Tracks::instance()->adjust(availableChannels());

  m_pCCSManager->init();

  // force Reaper to re-send all volume/pan data to this surface.
  // After a config change, the old surface's m_surface_volume map was
  // destroyed; this ensures Reaper repopulates it.
  CSurf_ResetAllCachedVolPanStates();

  // Force all LEDs off on all units (cache was invalidated, will actually send)
  for (size_t ui = 0; ui < m_units.size(); ui++)
    m_units[ui]->forceAllLEDsOff();

  // Publish initial transport state (LEDs freshly sent because caches are clean)
  m_pTransport = new Transport(this);
  m_pTransport->updateLeds();

  // Re-publish surface-level LEDs that MCUReset set and forceAllLEDsOff cleared
  SetLED(B_DROP, m_dropstate.ledState());
  m_pCCSManager->updateFlipLED();
  m_pCCSManager->updateGlobalViewLED();
  SetLED(B_ZOOM, (m_mackie_arrow_states & ARROW_STATE_ZOOM) ? 0x7f : 0);
  SetLED(B_SCRUB, (m_mackie_arrow_states & ARROW_STATE_SCRUB) ? 0x7f : 0);
  // assignment digits per transport unit
  sendMidiToTransportUnits(0xB0, 0x40 + 11,
      '0' + (((Tracks::instance()->getGlobalOffset() + 1) / 10) % 10), -1);
  sendMidiToTransportUnits(0xB0, 0x40 + 10,
      '0' + ((Tracks::instance()->getGlobalOffset() + 1) % 10), -1);

  connect2FrameSignal(boost::bind(&UndoEnd::run, UndoEnd::instance(), _1));

  Actions::instance()->init(this);
}

CSurf_MCU::~CSurf_MCU() {
  // Send per-unit reset SysEx (F0 00 00 66 <devId> 08 00 F7) to ALL units.
  for (size_t ui = 0; ui < m_units.size(); ui++)
    m_units[ui]->reset();

  // per-unit LED/fader shutdown.
  // Invalidate caches first so forceAllLEDsOff actually sends.
  for (size_t ui = 0; ui < m_units.size(); ui++) {
    HardwareUnit *u = m_units[ui];
    u->invalidateFaderCache();
    u->invalidateLEDCache();
    for (int local = 0; local < 8; local++)
      u->sendStripFader(local, 0);
    u->setMasterFader(0);
    u->forceAllLEDsOff();
  }

	// Write goodbye lines on ALL units' displays in 4-char chunks (same
	// SysEx-fragmentation workaround as DisplayHandler::sendDifferences).
	for (size_t ui = 0; ui < m_units.size(); ui++) {
		DisplayHandler *dh = m_units[ui]->displayHandler();
		for (int pos = 0; pos < 55; pos += 4)
			dh->sendToHardware(0, pos, &"                        Goodbye                          "[pos], std::min(4, 55 - pos));
		for (int i = 1; i < 4; i++)
			for (int pos = 0; pos < 55; pos += 4)
				dh->sendToHardware(i, pos, &"                                                       "[pos], std::min(4, 55 - pos));
	}

	// turn off the meter bridge (via owning unit)
	for (int i = 1; i <= availableChannels(); i++)
		sendStripMeter(i, 0);
	// master meters to ProX units
	sendMasterMetersToProXUnits(0, 0);

	// we must ensure that all events are send before the midi out is deleted
	Sleep(100);
	
  delete m_pTransport;
  for (size_t ui = 0; ui < m_pSplashDisplays.size(); ui++)
    safe_delete(m_pSplashDisplays[ui]);
  delete m_pActionsDialogComponent;
  delete m_pActionDisplay;
  delete m_pCCSManager;

  g_mcu_list.Delete(g_mcu_list.Find(this));
  // units own MIDI ports (dtors call DELETE_ASYNC on close).
  for (size_t i = 0; i < m_units.size(); i++)
    delete m_units[i];
  m_units.clear();
  while (m_schedule != NULL) {
    ScheduledAction *temp = m_schedule;
    m_schedule = temp->next;
    delete temp;
  }

  SelectedTrack::FreeTrackList(m_selected_tracks);

  delete (m_pButtonManager);

  s_iNumInstances--;
  if (s_iNumInstances == 0) {
    shutdownJuce_GUI();
    delete (Tracks::instance());
    delete (PlugMoveWatcher::instance());
    delete (ProjectConfig::instance());
    delete (UndoEnd::instance());
    //    delete(Actions::instance());
  }
}

void CSurf_MCU::CloseNoReset() {
  // units own MIDI ports now; close them through the unit.
  for (size_t i = 0; i < m_units.size(); i++)
    delete m_units[i];
  m_units.clear();
  m_midiout = NULL;
  m_midiin = NULL;
}

void CSurf_MCU::Run() {
  DWORD now = timeGetTime();

  Tracks *m_pTracks = Tracks::instance();

  if (now >= m_frameupd_lastrun + (1000 / std::max((*g_config_csurf_rate), 1)) ||
      now < m_frameupd_lastrun - 250) {
    m_frameupd_lastrun = now;

    ProjectConfig::instance()->checkReaProjectChange();

		// tracksStatesChanged() is called every frame (not every 3rd) so a
		// deleted/changed track is flushed before any code can dereference a
		// stale MediaTrack*. The fast-exit in tracksStatesChanged() makes this
		// cheap (~0.001ms) when the track list is unchanged.
		if (Tracks::instance()->tracksStatesChanged())
			m_pCCSManager->trackListChange();

    PlugMoveWatcher::instance()->checkMovement();

    signalFrame(now);

		if (anyUnitNeedsBlinkEmulation())
			EmulateBlinkingLEDs(now);

    Tracks::instance()->adjust(availableChannels());

    UpdateGlobalSoloLED();
    UpdateAutoModes();
    UpdateMetronomLED();

    while (m_schedule && now >= m_schedule->time) {
      ScheduledAction *action = m_schedule;
      m_schedule = m_schedule->next;
      (this->*(action->func))();
      delete action;
    }

    if (m_midiout) {
      double pp =
				(GetPlayState() & 1) ? GetPlayPosition() : GetCursorPosition();
        unsigned char bla[10];
        //      bla[-2]='A';//first char of assignment
        //    bla[-1]='Z';//second char of assignment

        // if 0x40 set, dot below item

        memset(bla, 0, sizeof(bla));

        int *tmodeptr =
					(int *)projectconfig_var_addr(NULL, __g_projectconfig_timemode2);

        int tmode = 0;

        if (tmodeptr && (*tmodeptr) >= 0)
          tmode = *tmodeptr;
        else {
          tmodeptr =
						(int *)projectconfig_var_addr(NULL, __g_projectconfig_timemode);
          if (tmodeptr)
            tmode = *tmodeptr;
        }

        if (tmode == 3) // seconds
					{
						double *toptr = (double *)projectconfig_var_addr(
																														 NULL, __g_projectconfig_timeoffs);

						if (toptr)
							pp += *toptr;
						char buf[64];
						sprintf(buf, "%d %02d", (int)pp, ((int)(pp * 100.0)) % 100);
						if (strlen(buf) > sizeof(bla))
							memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
						else
							memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));

					} else if (tmode == 4) // samples
					{
						char buf[128];
						format_timestr_pos(pp, buf, sizeof(buf), 4);
						if (strlen(buf) > sizeof(bla))
							memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
						else
							memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
					} else if (tmode == 5) // frames
					{
						char buf[128];
						format_timestr_pos(pp, buf, sizeof(buf), 5);
						char *p = buf;
						char *op = buf;
						int ccnt = 0;
						while (*p) {
							if (*p == ':') {
								ccnt++;
								if (ccnt != 3) {
									p++;
									continue;
								}
								*p = ' ';
							}

							*op++ = *p++;
						}
						*op = 0;
						if (strlen(buf) > sizeof(bla))
							memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
						else
							memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
					} else if (tmode > 0) {
          int num_measures = 0;
          double beats =
						TimeMap2_timeToBeats(NULL, pp, &num_measures, NULL, NULL, NULL) +
						0.000000000001;
          double nbeats = floor(beats);

          beats -= nbeats;

          int fracbeats = (int)(1000.0 * beats);

          int *measptr =
						(int *)projectconfig_var_addr(NULL, __g_projectconfig_measoffs);
          int nm = num_measures + 1 + (measptr ? *measptr : 0);
          if (nm >= 100)
            bla[0] = '0' + (nm / 100) % 10; // bars hund
          if (nm >= 10)
            bla[1] = '0' + (nm / 10) % 10; // barstens
          bla[2] = '0' + (nm) % 10;        // bars

          int nb = (int)nbeats + 1;
          if (nb >= 10)
            bla[3] = '0' + (nb / 10) % 10; // beats tens
          bla[4] = '0' + (nb) % 10;        // beats

          bla[7] = '0' + (fracbeats / 100) % 10;
          bla[8] = '0' + (fracbeats / 10) % 10;
          bla[9] = '0' + (fracbeats % 10); // frames
        } else {
          double *toptr = (double *)projectconfig_var_addr(
																													 NULL, __g_projectconfig_timeoffs);
          if (toptr)
            pp += (*toptr);

          int ipp = (int)pp;
          int fr = (int)((pp - ipp) * 1000.0);

          if (ipp >= 360000)
            bla[0] = '0' + (ipp / 360000) % 10; // hours hundreds
          if (ipp >= 36000)
            bla[1] = '0' + (ipp / 36000) % 10; // hours tens
          if (ipp >= 3600)
            bla[2] = '0' + (ipp / 3600) % 10; // hours

          bla[3] = '0' + (ipp / 600) % 6; // min tens
          bla[4] = '0' + (ipp / 60) % 10; // min
          bla[5] = '0' + (ipp / 10) % 6;  // sec tens
          bla[6] = '0' + (ipp % 10);      // sec
          bla[7] = '0' + (fr / 100) % 10;
          bla[8] = '0' + (fr / 10) % 10;
          bla[9] = '0' + (fr % 10); // frames
        }

        if (m_mackie_lasttime_mode != tmode) {
          m_mackie_lasttime_mode = tmode;
          SetLED(L_SMPTE, tmode == 5 ? 0x7F : 0);    // smpte light
          SetLED(L_BEATS,
                 m_mackie_lasttime_mode > 0 && tmode < 3 ? 0x7F : 0); // beats light
        }

        // if (memcmp(m_mackie_lasttime,bla,sizeof(bla)))
        {
          bool force = false;
          if (now > m_mcu_timedisp_lastforce) {
            m_mcu_timedisp_lastforce = now + 2000;
            force = true;
          }
          int x;
          for (x = 0; x < sizeof(bla); x++) {
            int idx = sizeof(bla) - x - 1;
            if (bla[idx] != m_mackie_lasttime[idx] || force) {
              sendMidiToTransportUnits(0xB0, 0x40 + x, bla[idx], -1);
              m_mackie_lasttime[idx] = bla[idx];
            }
          }
        }

      m_pCCSManager->frameUpdate(now);

      // resend all rows on every unit, not just unit 0
      for (size_t ui = 0; ui < m_units.size(); ui++) {
        HardwareUnit *u = m_units[ui];
        if (u && u->displayHandler() && u->displayHandler()->getDisplay())
          u->displayHandler()->getDisplay()->resendAllRows();
      }
      m_pCCSManager->getDisplayHandler()->waitForMoreChanges(true);
    }
  }

  if (m_midiin) {
    // iterate over all units' MIDI inputs.
    // m_currentInputOffset is set per-unit so strip handlers translate
    // local channel → global channel. Global events (transport, modifiers,
    // jog wheel, etc.) are only accepted from unit 0.
    for (size_t ui = 0; ui < m_units.size(); ui++) {
      midi_Input *in = m_units[ui]->midiInput();
      if (!in) continue;
      in->SwapBufs(timeGetTime());

      m_currentInputOffset = (int)ui * 8;

      int l = 0;
      MIDI_eventlist *list = in->GetReadBuf();
      MIDI_event_t *evts;
      while ((evts = list->EnumItems(&l))) {
#ifdef KLINKE
        if (!m_surfaceEnabled) {
          // inactive = units 2+ disabled: unit 1 is processed normally; the
          // Platform M+ (unit 3) is only scanned for the hand-off combo.
          const bool comboNote =
              (int)ui == KLINKE_COMBO_UNIT_INDEX &&
              (evts->midi_message[0] & 0xf0) == 0x90 &&
              evts->midi_message[1] >= 0x2e && evts->midi_message[1] <= 0x31;
          if ((int)ui != 0 && !comboNote)
            continue;
        }
#endif
        OnMIDIEvent(evts);
      }
    }

    if (m_button_states || m_mackie_arrow_states) {
      DWORD now = timeGetTime();
      if (now >= m_buttonstate_lastrun + 100) {
        m_buttonstate_lastrun = now;

        if (m_mackie_arrow_states) {
          bool iszoom = ((m_mackie_arrow_states & ARROW_STATE_ZOOM) > 0);
          int left = (iszoom && IsFlagSet(CONFIG_FLAG_SWAPZOOM)) ? 3 : 2;
          int right = (iszoom && IsFlagSet(CONFIG_FLAG_SWAPZOOM)) ? 2 : 3;

          if (m_mackie_arrow_states & 1)
            CSurf_OnArrow(0, iszoom);
          if (m_mackie_arrow_states & 2)
            CSurf_OnArrow(1, iszoom);
          if (m_mackie_arrow_states & 4)
            CSurf_OnArrow(left, iszoom);
          if (m_mackie_arrow_states & 8)
            CSurf_OnArrow(right, iszoom);
        }

        if ((m_button_states & 3) != 3) {
          if (m_button_states & 1) {
            CSurf_OnRew(1);
          } else if (m_button_states & 2) {
            CSurf_OnFwd(1);
          }
        }
      }
    }
  }

  // Drain JUCE's X11 event queue so editor windows paint, respond to mouse/
  // keyboard input, and handle WM_DELETE_WINDOW (the title-bar X button).
  // Each call processes one pending event and returns false when the queue is
  // empty — no blocking, negligible overhead when no windows are open.
  // Linux/X11 only: on macOS JUCE dispatches via the native NSRunLoop (driven
  // by REAPER's main thread), and on Windows via the native message loop, so
  // no manual pump is needed there (and dispatchNextMessageOnSystemQueue has
  // no macOS implementation in JUCE 8).
#if JUCE_LINUX
  for (int i = 0; i < 30 && juce::detail::dispatchNextMessageOnSystemQueue(true); ++i) {}
#endif
}

#ifdef KLINKE
void CSurf_MCU::setSurfaceEnabled(bool enabled) {
  if (m_surfaceEnabled == enabled)
    return;
  m_surfaceEnabled = enabled;

  // Behave exactly as if units 2+ (the iCON controllers) were disabled:
  //   - unit 1 stays fully active (input + output)
  //   - units 2+ are muted (no input, no output); only the Platform M+
  //     (unit 3, KLINKE_COMBO_UNIT_INDEX) is monitored for the hand-off combo
  //   - the channel count shrinks to unit 1's strips: numUnits()/availableChannels()
  //     drop accordingly and Tracks::adjust() clamps the global offset and
  //     rebuilds the track->strip mapping (this also limits the bank up/down
  //     navigation on unit 1 to the reduced channel count)
  for (size_t ui = 0; ui < m_units.size(); ui++)
    m_units[ui]->setUnitEnabled(enabled || (int)ui == 0);

  if (enabled) {
    // The re-activated units were driven by Schaltmix in between. Clear ALL
    // visual elements so nothing stays on the hardware: every button LED off
    // (also resets the LED cache), LCD rows blanked, fader cache invalidated
    // (so the pushed volumes/pan are re-sent). The per-frame refresh then
    // repopulates displays/LEDs — sendDifferences always re-sends the whole
    // row, so no Schaltmix characters can remain.
    for (size_t ui = 1; ui < m_units.size(); ui++) {
      m_units[ui]->forceAllLEDsOff();
      m_units[ui]->invalidateFaderCache();
      if (m_units[ui]->displayHandler()) {
        char spaces[56];
        memset(spaces, ' ', sizeof(spaces));
        m_units[ui]->displayHandler()->sendToHardware(0, 0, spaces, 55);
        m_units[ui]->displayHandler()->sendToHardware(1, 0, spaces, 55);
      }
    }
  }

  Tracks::instance()->adjust(availableChannels());
  CSurf_ResetAllCachedVolPanStates();
}
#endif

void CSurf_MCU::SendMidi(unsigned char status, unsigned char d1,
                         unsigned char d2, int frame_offset) {
  if (m_midiout)
    m_midiout->Send(status, d1, d2, frame_offset);
}

void CSurf_MCU::SetLED(int button_nr, int led_state) {
  // SetLED is global-only. Strip notes MUST go through setStripLED().
  ASSERT(isGlobalLedNote(button_nr));
  if (isGlobalLedNote(button_nr))
    setGlobalLED(button_nr, led_state);
}

void CSurf_MCU::EmulateBlinkingLEDs(DWORD now) {
  // blink emulation across all units, each tracks its own LED state.
  for (size_t i = 0; i < m_units.size(); i++)
    m_units[i]->emulateBlinkingLEDs(now);
}

bool CSurf_MCU::anyUnitNeedsBlinkEmulation() const {
  for (size_t i = 0; i < m_units.size(); i++)
    if (m_units[i]->needsBlinkEmulation())
      return true;
  return false;
}


void CSurf_MCU::SetTrackListChange() {
  //  ProjectConfig::instance()->checkReaProjectChange();
  //  Tracks::instance()->tracksStatesChanged();
  //  m_pCCSManager->trackListChange();
}

void CSurf_MCU::SetSurfaceVolume(MediaTrack *trackid, double volume) {
  // REAPER calls SetSurfaceVolume() once per track on every bank operation
  // via TrackList_UpdateAllExternalSurfaces(). The two calls that used to run
  // here were both redundant and expensive at scale:
  //  - tracksStatesChanged()/trackListChange(): volume changes never add,
  //    remove, or reorder tracks. Run() already polls tracksStatesChanged()
  //    every frame (with a cheap early-exit) — that is the only place needed.
  //  - updateFader(): sent MIDI for all visible channels on every per-track
  //    callback. With 250+ tracks that is hundreds of redundant MIDI bursts
  //    per bank where one per frame suffices. MultiTrackMode::frameUpdate()
  //    calls updateFaders() every frame and picks up the stored volume there.
  m_surface_volume[trackid] = volume;
}

void CSurf_MCU::SetSurfacePan(MediaTrack *trackid, double pan) {
  m_surface_pan[trackid] = pan;
}

void CSurf_MCU::SetSurfaceMute(MediaTrack *trackid, bool mute) {}

void CSurf_MCU::SetSurfaceSelected(MediaTrack *trackid, bool selected) {
  // updateSelection() applies the single trackid/selected change REAPER passes
  // directly to m_selectedTracks (O(log n)) instead of selectionChanged()'s
  // full O(n) rebuild. REAPER fires this callback once per track on operations
  // like Ctrl+A, so the O(n) rebuild made selection O(n^2) overall.
  Tracks::instance()->updateSelection(trackid, selected);

  m_pCCSManager->trackSelected(trackid, selected);
}

void CSurf_MCU::SetSurfaceSolo(MediaTrack *trackid, bool solo) {
  // UpdateGlobalSoloLED() is called once per frame in Run(); firing it here
  // too means it runs once per track callback. REAPER broadcasts SetSurfaceSolo
  // to all tracks on every bank operation, so this per-track call flooded the
  // MCU's MIDI receive buffer (the midiEvents spikes during banking). Run()
  // already covers it.
}

void CSurf_MCU::SetSurfaceRecArm(MediaTrack *trackid, bool recarm) {}

void CSurf_MCU::SetPlayState(bool play, bool pause, bool rec) {
  SetLED(B_RECORD, rec ? 0x7f : 0);
  SetLED(B_PLAY, play ? 0x7f : 0);
  SetLED(B_PAUSE, pause ? 0x7f : 0);    // B_PAUSE is 0x5d (transport pause)
}

void CSurf_MCU::SetRepeatState(bool rep) {
  m_repeatState = rep;
  SetLED(B_CYCLE, rep ? 0x7f : 0);
}

void CSurf_MCU::SetTrackTitle(MediaTrack *trackid, const char *title) {}

bool CSurf_MCU::GetTouchState(MediaTrack *pMT, int isPan) {
  if (MultiTrackMode::getFlipMode() == !isPan) {
    if (pMT && m_pan_lasttouch.find(pMT) != m_pan_lasttouch.end()) {
      DWORD now = timeGetTime();
      if (m_pan_lasttouch[pMT] == 1 ||
          (now < m_pan_lasttouch[pMT] +
					 3000)) // fake touch, go for 3s after last movement
				{
					return true;
				}
    }
    return false;
  }

  if (pMT && m_fader_touchstate.find(pMT) != m_fader_touchstate.end())
    return m_fader_touchstate[pMT];

  return false;
}

bool CSurf_MCU::ResetAllFaderTouch(MIDI_event_t *evt) {
	m_pCCSManager->resetAllFaderTouch();

	return true;
}

bool CSurf_MCU::OpenFXFavorite(MIDI_event_t *evt) {
  int slot = evt->midi_message[1] - 0x72; // 0 - 7

	m_pCCSManager->getPlugMode()->accessFXFavorite(slot);

	return true;
}


void CSurf_MCU::SetAutoMode(int mode) {
  // wird von Reaper aufgerufen, wenn der automation-mode geaendert wurde,
  // m_pCCSManager->updateAutoMode();
  UpdateAutoModes();
}

void CSurf_MCU::UpdateAutoModes() {
  int modes[5] = {0, 0, 0, 0, 0};
  for (SelectedTrack *i = m_selected_tracks; i; i = i->next) {
    MediaTrack *track = i->track();
    if (!track)
      continue;
    int mode = GetTrackAutomationMode(track);
    if (0 <= mode && mode < 5)
      modes[mode] = 1;
  }
  bool multi = (modes[0] + modes[1] + modes[2] + modes[3] + modes[4]) > 1;
  SetLED(B_AUTO_READ, modes[AUTO_MODE_READ] ? (multi ? 1 : 0x7f) : 0);
  SetLED(B_AUTO_WRITE, modes[AUTO_MODE_WRITE] ? (multi ? 1 : 0x7f) : 0);
  SetLED(B_AUTO_TRIM, modes[AUTO_MODE_TRIM] ? (multi ? 1 : 0x7f) : 0);
  SetLED(B_AUTO_TOUCH, modes[AUTO_MODE_TOUCH] ? (multi ? 1 : 0x7f) : 0);
  SetLED(B_AUTO_LATCH, modes[AUTO_MODE_LATCH] ? (multi ? 1 : 0x7f) : 0);
}

void CSurf_MCU::OnTrackSelection(MediaTrack *trackid) {}

bool CSurf_MCU::IsModifierPressed(int key) {
  ASSERT_M(key == VK_SHIFT || key == VK_OPTION || key == VK_CONTROL ||
					 key == VK_ALT,
           "Only for Modifier");
  if (m_midiin && !m_is_mcuex) {
    if (key == VK_SHIFT)
      return (s_mackie_modifiers & 1) || IsKeyboardPressed(VK_SHIFT);
    if (key == VK_OPTION)
      return !!(s_mackie_modifiers & 2) || IsKeyboardPressed(VK_MENU);
    if (key == VK_CONTROL)
      return !!(s_mackie_modifiers & 4) || IsKeyboardPressed(VK_CONTROL);
    if (key == VK_ALT)
      return !!(s_mackie_modifiers & 8) || IsKeyboardPressed(VK_MENU);
  }

  return false;
}

bool CSurf_MCU::IsKeyboardPressed(int key) {
  return (GetAsyncKeyState(key) & 0x8000) &&
		IsFlagSet(CONFIG_FLAG_KEYBOARD_MODIFIER);
}

bool CSurf_MCU::OnGlobalViewKeys(MIDI_event_t *evt) {
  if (evt->midi_message[2] >= 0x40) {
    unsigned char a = evt->midi_message[1];
    if (IsButtonPressed(B_GLOBAL_VIEW))
      a -= 0x10;
    else if (IsButtonPressed(B_MARKER))
      a -= 0x20;
    else if (IsButtonPressed(B_NUDGE))
      a -= 0x30;

    unsigned char byte2 = 0xbf;
    if (IsModifierPressed(VK_SHIFT))
      byte2 -= 1;
    if (IsModifierPressed(VK_OPTION))
      byte2 -= 2;
    if (IsModifierPressed(VK_CONTROL))
      byte2 -= 4;
    if (IsModifierPressed(VK_ALT))
      byte2 -= 8;

    MIDI_event_t evt = {0, 3, byte2, a, 0};
    kbd_OnMidiEvent(&evt, -1);
  }
  return true;
}

void CSurf_MCU::SendMsg(MIDI_event_t *message, int frame_offset) {
  if (m_midiout)
    m_midiout->SendMsg(message, frame_offset);
}

void CSurf_MCU::sendStripFader(int channel, int value) {
  // channel 0 = master fader → broadcast to all units.
  // Channels 1..N*8 → owning unit.
  if (channel == 0) {
    broadcastMasterFader(value);
  } else {
    HardwareUnit *u = unitForChannel(channel);
    if (u) {
      int local = (channel - 1) % 8;
      u->sendStripFader(local, value);
    }
  }
}

int CSurf_MCU::getFaderPos(int channel) {
  if (channel == 0) {
    return m_units.empty() ? 0 : m_units[0]->getFaderPos(8);
  }
  HardwareUnit *u = unitForChannel(channel);
  if (!u) return 0;
  int local = (channel - 1) % 8;
  return u->getFaderPos(local);
}

void CSurf_MCU::broadcastMasterFader(int value) {
  for (size_t i = 0; i < m_units.size(); i++)
    m_units[i]->setMasterFader(value);
}

// --- capability queries ---

bool CSurf_MCU::hasTransportUnits() const {
  for (size_t i = 0; i < m_units.size(); i++)
    if (m_units[i]->isMain())
      return true;
  return false;
}

HardwareUnit *CSurf_MCU::firstTransportUnit() const {
  for (size_t i = 0; i < m_units.size(); i++)
    if (m_units[i]->isMain())
      return m_units[i];
  return NULL;
}

// --- global broadcast ---

void CSurf_MCU::setGlobalLED(int note, int state) {
  for (size_t i = 0; i < m_units.size(); i++) {
    if (m_units[i]->isMain())
      m_units[i]->setLED(note, state);
  }
}

void CSurf_MCU::sendMidiToTransportUnits(unsigned char status,
                                         unsigned char d1, unsigned char d2,
                                         int frameOffset) {
  for (size_t i = 0; i < m_units.size(); i++) {
    if (m_units[i]->isMain())
      m_units[i]->sendMidi(status, d1, d2, frameOffset);
  }
}

void CSurf_MCU::sendMidiToAllUnits(unsigned char status, unsigned char d1,
                                   unsigned char d2, int frameOffset) {
  for (size_t i = 0; i < m_units.size(); i++)
    m_units[i]->sendMidi(status, d1, d2, frameOffset);
}

void CSurf_MCU::setLEDOnAllUnits(int note, int state) {
  for (size_t i = 0; i < m_units.size(); i++)
    m_units[i]->setLED(note, state);
}

// --- strip routing (global channel → owning unit) ---

void CSurf_MCU::setStripLED(int globalChannel, int localNote, int state) {
  HardwareUnit *u = unitForChannel(globalChannel);
  if (!u) return;
  u->setLED(localNote, state);
}

void CSurf_MCU::sendStripCC(int globalChannel, unsigned char cc,
                            unsigned char value, int frameOffset) {
  HardwareUnit *u = unitForChannel(globalChannel);
  if (!u) return;
  u->sendMidi(0xB0, cc, value, frameOffset);
}

void CSurf_MCU::sendStripFaderToUnit(int globalChannel, int value) {
  HardwareUnit *u = unitForChannel(globalChannel);
  if (!u) return;
  int local = (globalChannel - 1) % 8;
  u->sendStripFader(local, value);
}

void CSurf_MCU::sendStripMeter(int globalChannel, short meter) {
  HardwareUnit *u = unitForChannel(globalChannel);
  if (!u) return;
  int local = (globalChannel - 1) % 8;
  u->sendMidi(0xD0, (local << 4) | meter, 0, -1);
}

void CSurf_MCU::enableMCUMeters(bool enable) {
  for (size_t i = 0; i < m_units.size(); i++)
    m_units[i]->displayHandler()->enableMCUMeter(enable);
}

void CSurf_MCU::enableMCUMeters(bool enable, bool excludeProX) {
  for (size_t i = 0; i < m_units.size(); i++) {
    if (excludeProX && m_units[i]->isProX())
      continue;
    m_units[i]->displayHandler()->enableMCUMeter(enable);
  }
}

void CSurf_MCU::sendMasterMetersToProXUnits(short left, short right) {
  for (size_t i = 0; i < m_units.size(); i++) {
    HardwareUnit *u = m_units[i];
    if (!u->isProX()) continue;
    u->sendMidi(0xD1, (0 << 4) | left,  0, -1);
    u->sendMidi(0xD1, (1 << 4) | right, 0, -1);
  }
}

bool CSurf_MCU::OnNameValue(MIDI_event_t *evt) {
  if (IsModifierPressed(VK_ALT)) {
    if (!m_pActionsDialogComponent)
      m_pActionsDialogComponent = new ActionsDialogComponent(m_pActionDisplay);

		if (evt->midi_message[2] >= 0x40) {
			m_pCCSManager->getEditor()->setMainComponent(&m_pActionsDialogComponent,
																									 true);
		}
  } else {
    if (evt->midi_message[2] >= 0x40) {
      m_pActionDisplay->activate(s_mackie_modifiers);
    } else {
      m_pActionDisplay->deactivate();
    }
  }
  return true;
}

bool CSurf_MCU::OnNameValueDC(MIDI_event_t *evt) {
  m_pCCSManager->buttonNameValue(evt->midi_message[2] >= 0x40);
  return true;
}

void CSurf_MCU::UnselectAllTracks() {
  // Clear master track
  CSurf_OnSelectedChange(CSurf_TrackFromID(0, false), 0);
  // Clear already selected tracks
  SelectedTrack *i = GetSelectedTracks();
  while (i) {
    // Call to OnSelectedChange will cause 'i' to be destroyed, so go ahead
    // and get 'next' now
    SelectedTrack *next = i->next;
    MediaTrack *track = i->track();

    if (track)
      CSurf_OnSelectedChange(track, 0);
    i = next;
  }
}

void CSurf_MCU::UpdateGlobalSoloLED() {
  // check master track flags, if master track is muted, blink the LED
  int flags;
  GetTrackInfo(-1, &flags);
  if (flags & 8) {
    SetLED(B_SOLO, LED_BLINK);
    SetLED(L_RUDESOLO, LED_BLINK);
    return;
  }
  if (SomethingSoloed()) {
    SetLED(B_SOLO, LED_ON);
    SetLED(L_RUDESOLO, LED_ON);
    return;
  }
  SetLED(B_SOLO, LED_OFF);
  SetLED(L_RUDESOLO, LED_OFF);
}

void CSurf_MCU::UpdateMetronomLED() {
  if (m_metronom_offset) {
    int *metro_en = (int *)projectconfig_var_addr(NULL, m_metronom_offset);
    if (metro_en) {
      int a = *metro_en;
      SetLED(B_CLICK, a & 1 ? LED_ON : LED_OFF);
    }
  }
}

bool CSurf_MCU::IsButtonPressed(int i) {
  return m_pButtonManager->isButtonPressed(i);
}

int CSurf_MCU::IsLastButton(int button) {
  return m_pButtonManager->isLastButton(button);
}

double CSurf_MCU::GetSurfaceVolume(MediaTrack *pMT) {
  if (pMT && m_surface_volume.find(pMT) != m_surface_volume.end())
    return m_surface_volume[pMT];

  // fallback — read directly from Reaper when cache is cold
  // (e.g. after surface recreation / config change)
  if (pMT) {
    double *vol = (double *)GetSetMediaTrackInfo(pMT, "D_VOL", NULL);
    if (vol) return *vol;
  }
  return 0;
}

double CSurf_MCU::GetSurfaceVolume(int channel) {
  return GetSurfaceVolume(Tracks::instance()->getMediaTrackForChannel(channel));
}

double CSurf_MCU::GetSurfacePan(MediaTrack *pMT) {
  if (pMT && m_surface_pan.find(pMT) != m_surface_pan.end())
    return m_surface_pan[pMT];

  // fallback — read directly from Reaper when cache is cold
  if (pMT) {
    double *pan = (double *)GetSetMediaTrackInfo(pMT, "D_PAN", NULL);
    if (pan) return *pan;
  }
  return 0;
}

double CSurf_MCU::GetSurfacePan(int channel) {
  return GetSurfacePan(Tracks::instance()->getMediaTrackForChannel(channel));
}
