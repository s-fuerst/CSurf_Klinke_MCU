/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "MultiTrackMode.h"
#include "McuDebugLog.h"
#include "MultiTrackMeterBridge.h"
#include "CCSManager.h"
#include "csurf_mcu.h"
#include "McuAssert.h"
#include "stdio.h"
#include "Display.h"
#include "MultiTrackOptions.h"
#include "MultiTrackOptions2.h"
#include "TrackStatesEditorComponent.h"
#include "MultiTrackSelector.h"

bool MultiTrackMode::s_flipmode = false;
bool MultiTrackMode::s_mcpmode = false;

MultiTrackMode::MultiTrackMode(CCSManager *pManager)
    : CCSMode(pManager), m_pTrackStatesEditor(NULL), m_lastSelectedTrackNr(-1) {
  m_pDisplay = pManager->createDisplay(4);
  m_pSelector = new MultiTrackSelector(pManager->getDisplayHandler());
	m_pMeterBridge = new MultiTrackMeterBridge();
  s_mcpmode = CSurf_MCU::IsFlagSet(CONFIG_FLAG_STARTGLOBALVIEW);

  Tracks::instance()->setDisplayHandler(pManager->getDisplayHandler());
}

MultiTrackMode::~MultiTrackMode(void) {
	safe_delete(m_pMeterBridge);
  safe_delete(m_pSelector);
  safe_delete(m_pDisplay);
}

MediaTrack *MultiTrackMode::getMediaTrackForChannel(int channel) {
  return Tracks::instance()->getMediaTrackForChannel(channel);
}

void MultiTrackMode::frameUpdate() {
  m_pMeterBridge->updateMeterBridge(m_pCCSManager->getMCU());
	
  updateEverything();

  if (Tracks::instance()->clampCurrentGlobalOffset()) {
    TrackList_UpdateAllExternalSurfaces();
    updateAssignmentDisplay();
  }
}

void MultiTrackMode::activate() {
  CCSMode::activate();
  m_pDisplay->clear();
  m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
}

void MultiTrackMode::updateRecLEDs() {
  // WP-F: widen to 1..Tracks::getNumberOfChannelStrips()
  for (int channel = 1; channel < 9; channel++) {
    if (MediaTrack *tr = getMediaTrackForChannel(channel)) {
      int *pRecArm = (int *)GetSetMediaTrackInfo(tr, "I_RECARM", NULL);
      if (isModifierPressed(VK_SHIFT)) {
        if (*pRecArm) {
          int *pMonStatus = (int *)GetSetMediaTrackInfo(tr, "I_RECMON", NULL);
          m_pCCSManager->setRecLED(this, channel,
                                   *pMonStatus ? LED_ON : LED_OFF);
        }
      } else if (isModifierPressed(VK_OPTION)) {
        if (*pRecArm) {
          int *pRecMode = (int *)GetSetMediaTrackInfo(tr, "I_RECMODE", NULL);
          m_pCCSManager->setRecLED(this, channel, *pRecMode == 0 ? LED_ON : LED_OFF);
        }
      } else if (isModifierPressed(VK_ALT)) {
        if (*pRecArm) {
          int *pRecMode = (int *)GetSetMediaTrackInfo(tr, "I_RECMODE", NULL);
          m_pCCSManager->setRecLED(this, channel, *pRecMode == 2 ? LED_OFF : LED_ON);
        }
      } else {
        m_pCCSManager->setRecLED(this, channel, *pRecArm ? LED_ON : LED_OFF);
      }
    } else {
      m_pCCSManager->setRecLED(this, channel, LED_OFF);
    }
  }
}

void MultiTrackMode::updateSoloLEDs() {
  // WP-F: widen to 1..Tracks::getNumberOfChannelStrips()
  for (int channel = 1; channel < 9; channel++) {
    if (MediaTrack *tr = getMediaTrackForChannel(channel)) {
      int *pSolo = (int *)GetSetMediaTrackInfo(tr, "I_SOLO", NULL);
      m_pCCSManager->setSoloLED(this, channel, *pSolo ? LED_ON : LED_OFF);
    } else {
      m_pCCSManager->setSoloLED(this, channel, LED_OFF);
    }
  }
}

void MultiTrackMode::updateMuteLEDs() {
  // WP-F: widen to 1..Tracks::getNumberOfChannelStrips()
  for (int channel = 1; channel < 9; channel++) {
    if (MediaTrack *tr = getMediaTrackForChannel(channel)) {
      bool *pMute = (bool *)GetSetMediaTrackInfo(tr, "B_MUTE", NULL);
      m_pCCSManager->setMuteLED(this, channel, *pMute ? LED_ON : LED_OFF);
    } else {
      m_pCCSManager->setMuteLED(this, channel, LED_OFF);
    }
  }
}

void MultiTrackMode::updateSelectLEDs() {
  // WP-F: widen to 1..Tracks::getNumberOfChannelStrips()
  for (int channel = 1; channel < 9; channel++) {
    if (MediaTrack *tr = getMediaTrackForChannel(channel)) {
      int *pSelect = (int *)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL);
      m_pCCSManager->setSelectLED(this, channel, *pSelect ? LED_ON : LED_OFF);
    } else {
      m_pCCSManager->setSelectLED(this, channel, LED_OFF);
    }
  }
}

void MultiTrackMode::updateFlipLED() {
  m_pCCSManager->setFlipLED(this, s_flipmode);
}

void MultiTrackMode::updateGlobalViewLED() {
  m_pCCSManager->setGlobalViewLED(this,
                                  Tracks::instance()->baseTrackHasParent());
}

void MultiTrackMode::updateFaders() {
  // WP-F: widen to 1..Tracks::getNumberOfChannelStrips()
  for (int channel = 1; channel < 9; channel++) {
    if (MediaTrack *tr = getMediaTrackForChannel(channel)) {
      if (!s_flipmode)
        trackVolume(channel,
                    m_pCCSManager->getMCU()->GetSurfaceVolume(channel));
      else
        trackPan(channel, m_pCCSManager->getMCU()->GetSurfacePan(channel));
    } else {
      m_pCCSManager->setFader(this, channel, 0);
    }
  }
  // master fader (doesn't flip)
  m_pCCSManager->setFader(this, 0,
                          volToInt14(*((double *)GetSetMediaTrackInfo(
                              getMediaTrackForChannel(0), "D_VOL", 0))));
}

void MultiTrackMode::updateVPOTs() {
  // WP-F: widen to 1..Tracks::getNumberOfChannelStrips()
  for (int channel = 1; channel < 9; channel++) {
    if (MediaTrack *tr = getMediaTrackForChannel(channel)) {
			if (s_flipmode)
				m_pCCSManager->getVPOT(channel)->setMode(VPOT_LED::FROM_LEFT);
			else
				m_pCCSManager->getVPOT(channel)->setMode(VPOT_LED::FROM_MIDDLE_POINT);

			m_pCCSManager->getVPOT(channel)->setBottom(
          Tracks::instance()->hasChilds(tr));
      if (s_flipmode)
        trackVolume(channel,
                    m_pCCSManager->getMCU()->GetSurfaceVolume(channel));
      else
        trackPan(channel, m_pCCSManager->getMCU()->GetSurfacePan(channel));
    } else {
      m_pCCSManager->getVPOT(channel)->setMode(VPOT_LED::OFF);
      m_pCCSManager->getVPOT(channel)->setBottom(false);
    }
  }
}

void MultiTrackMode::updateAssignmentDisplay() {
  int nr;
  MediaTrack *pMT = Tracks::instance()->getSelectedSingleTrack();
  if (pMT) {
    nr = (int)(intptr_t)GetSetMediaTrackInfo(pMT, "IP_TRACKNUMBER", NULL);

    char display[2];
    display[0] = '0' + ((nr / 10) % 10);
    display[1] = '0' + (nr % 10);

    m_pCCSManager->setAssignmentDisplay(this, display);
  } else {
    m_pCCSManager->setAssignmentDisplay(this, "");
  }
}

bool MultiTrackMode::fader(int channel, int value) {
  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    if (channel > 0 /*master fader doesn't flip */ && s_flipmode)
      CSurf_SetSurfacePan(tr, CSurf_OnPanChange(tr, int14ToPan(value), false),
                          NULL);
    else
      CSurf_SetSurfaceVolume(
          tr, CSurf_OnVolumeChange(tr, int14ToVol(value), false), NULL);

    return true;
  }

  return false;
}

bool MultiTrackMode::buttonFaderBanks(int button, bool pressed) {
  if (!pressed)
    return false;

  int move;
  switch (button) {
  case B_BANK_DOWN:
    if (isModifierPressed(VK_SHIFT))
      move = -Tracks::instance()->getLegacyPageStep();
    else
      move = -Tracks::instance()->getEffectiveBankStep();
    break;
  case B_CHANNEL_DOWN:
    move = -1;
    break;
  case B_CHANNEL_UP:
    move = 1;
    break;
  case B_BANK_UP:
    if (isModifierPressed(VK_SHIFT))
      move = Tracks::instance()->getLegacyPageStep();
    else
      move = Tracks::instance()->getEffectiveBankStep();
    break;
  default:
    ASSERT_M(true, "unknown button");
    return false;
  }

  bool changed = Tracks::instance()->setGlobalOffset(
      move + Tracks::instance()->getGlobalOffset());

  if (changed)
    TrackList_UpdateAllExternalSurfaces();

  return true;
}

bool MultiTrackMode::buttonRec(int channel, bool pressed) {
  if (!pressed)
    return false;

  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    if (isModifierPressed(VK_SHIFT)) {
      int *pRecArm = (int *)GetSetMediaTrackInfo(tr, "I_RECARM", NULL);
      if (*pRecArm) {
        int *pMonStatus = (int *)GetSetMediaTrackInfo(tr, "I_RECMON", NULL);
        int newMon = *pMonStatus ? 0 : 1;
        GetSetMediaTrackInfo(tr, "I_RECMON", &newMon);
        TrackList_AdjustWindows(false);
      }
    } else if (isModifierPressed(VK_OPTION)) {
      int *pRecArm = (int *)GetSetMediaTrackInfo(tr, "I_RECARM", NULL);
      if (*pRecArm) {
        int *pRecMode = (int *)GetSetMediaTrackInfo(tr, "I_RECMODE", NULL);
        int newMode = (*pRecMode == 0) ? 2 : 0;
        GetSetMediaTrackInfo(tr, "I_RECMODE", &newMode);
        TrackList_AdjustWindows(false);
      }
    } else if (isModifierPressed(VK_ALT)) {
      int *pRecArm = (int *)GetSetMediaTrackInfo(tr, "I_RECARM", NULL);
      if (*pRecArm) {
        int *pRecMode = (int *)GetSetMediaTrackInfo(tr, "I_RECMODE", NULL);
        int newMode = (*pRecMode == 1) ? 5 : (*pRecMode == 5) ? 2 : 1;
        GetSetMediaTrackInfo(tr, "I_RECMODE", &newMode);
        TrackList_AdjustWindows(false);
      }
    } else {
      // Explicit toggle — CSurf_OnRecArmChange(-1) doesn't arm on Linux
      int *pRecArm = (int *)GetSetMediaTrackInfo(tr, "I_RECARM", NULL);
      int newArm = (*pRecArm) ? 0 : 1;
      GetSetMediaTrackInfo(tr, "I_RECARM", &newArm);
      CSurf_OnRecArmChange(tr, newArm);
    }
    updateRecLEDs();

    return true;
  }

  return false;
}

bool MultiTrackMode::buttonMute(int channel, bool pressed) {
  MCU_LOG("MUTE ch=%d pressed=%d", channel, pressed);
  if (!pressed)
    return false;

  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    CSurf_SetSurfaceMute(tr, CSurf_OnMuteChange(tr, -1), NULL);
    updateMuteLEDs();
    return true;
  }

  return false;
}

bool MultiTrackMode::buttonSolo(int channel, bool pressed) {
  MCU_LOG("SOLO ch=%d pressed=%d", channel, pressed);
  if (!pressed)
    return false;

  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    CSurf_SetSurfaceSolo(tr, CSurf_OnSoloChange(tr, -1), NULL);
    updateSoloLEDs();
    return true;
  }

  return false;
}

bool MultiTrackMode::buttonSoloDC(int channel) {
  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    SoloAllTracks(0);
    CSurf_SetSurfaceSolo(tr, CSurf_OnSoloChange(tr, 1), NULL);
    updateSoloLEDs();
    return true;
  }

  return false;
}

bool MultiTrackMode::buttonSelect(int channel, bool pressed) {
  if (pressed)
    return false;

  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    if (isModifierPressed(VK_SHIFT) || isModifierPressed(VK_ALT)) {
      if (m_lastSelectedTrackNr >= 0) {
        m_pCCSManager->getMCU()->UnselectAllTracks();
        int m_toTrackNr = MediaTrackInfo::getTrackNr(tr);
        if (m_toTrackNr < m_lastSelectedTrackNr) {
          for (int trackNr = m_toTrackNr; trackNr <= m_lastSelectedTrackNr;
               trackNr++) {
            MediaTrack *track = CSurf_TrackFromID(trackNr, false);
            if (isModifierPressed(VK_SHIFT) ||
                Tracks::instance()->isTrackInFilter(track))
              SetTrackSelected(track, true);
          }
        } else if (m_toTrackNr > m_lastSelectedTrackNr) {
          for (int trackNr = m_lastSelectedTrackNr; trackNr <= m_toTrackNr;
               trackNr++) {
            MediaTrack *track = CSurf_TrackFromID(trackNr, false);
            if (isModifierPressed(VK_SHIFT) ||
                Tracks::instance()->isTrackInFilter(track))
              SetTrackSelected(track, true);
          }
        }
        CSurf_OnTrackSelection(tr);
        TrackList_AdjustWindows(false);
      }

    } else if (isModifierPressed(VK_CONTROL)) {
      // Toggle selection of this track (add/remove from multi-select)
      int *sel = (int *)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL);
      bool newSel = !(*sel);
      SetTrackSelected(tr, newSel);
      if (newSel) CSurf_OnTrackSelection(tr);
      TrackList_AdjustWindows(false);
      m_lastSelectedTrackNr = MediaTrackInfo::getTrackNr(tr);
    } else {
      m_lastSelectedTrackNr = MediaTrackInfo::getTrackNr(tr);
      m_pCCSManager->getMCU()->UnselectAllTracks();
      SetTrackSelected(tr, true);
      CSurf_OnTrackSelection(tr);
      TrackList_AdjustWindows(false);
    }

    if (Tracks::instance()->get2ndOptions()->isOptionSetTo(
            MTO2_AUTO_TOUCH, MTO2A_AUTO_TOUCH_ON)) {
      SendMessage(g_hwnd, WM_COMMAND, ID_TOUCH_SELECTED_TRACK, 0);
    }

    return true;
  }

  return false;
}

bool MultiTrackMode::buttonSelectLong(int channel) {
  MediaTrack *pMT = getMediaTrackForChannel(channel);
  if (pMT &&
      !getOptions()->isOptionSetTo(MTO_REFLECT_FOLDER, MTOA_REFLECT_NO)) {
    if (Tracks::instance()->moveBaseTrack(getMediaTrackForChannel(channel))) {
      Tracks::instance()->setGlobalOffset(0);
      return true;
    }
  }

  return false;
}

bool MultiTrackMode::buttonFlip(bool pressed) {
  if (!pressed)
    return false;

  s_flipmode = !s_flipmode;
  CSurf_ResetAllCachedVolPanStates();
  TrackList_UpdateAllExternalSurfaces();
  updateEverything();
  return true;
}

bool MultiTrackMode::buttonGView(bool pressed) {
  if (!pressed)
    return false;

  Tracks::instance()->moveBaseTrackToParent();
  Tracks::instance()->setGlobalOffset(0);
  return true;
}

void MultiTrackMode::trackListChange() { updateEverything(); }

void MultiTrackMode::trackVolume(int id, double volume) {
  MIDIOUT
  // WP-F: widen to id < Tracks::getNumberOfChannelStrips()
  if (id >= 0 && id < 9) {
    if (s_flipmode) {
      unsigned char volch = volToChar(volume);
      m_pCCSManager->getVPOT(id)->setValue(1 + ((volch * 11) >> 7));
    } else {
      int volint = volToInt14(volume);
      m_pCCSManager->setFader(this, id, volint);
      if (m_pCCSManager->getFaderTouched(id)) {
        //        if (m_pCCSManager->getNumFadersTouched() > 1) {
        //          m_pCCSManager->getDisplayHandler()->waitForMoreChanges(true);
        //        }
        //m_pDisplay->showDB(1, id, volume);
      }
    }
  }
}

void MultiTrackMode::trackPan(int id, double pan) {
  MIDIOUT
  // WP-F: widen to id < Tracks::getNumberOfChannelStrips()
  if (id >= 0 && id < 9) {
    if (!s_flipmode) {
      unsigned char volch = panToChar(pan);
      m_pCCSManager->getVPOT(id)->setValue(1 + ((volch * 11) >> 7));
    } else {
      int volint = panToInt14(pan);
      m_pCCSManager->setFader(this, id, volint);
    }
  }
}

void MultiTrackMode::updateDisplay() {
  // WP-F: widen to 1..Tracks::getNumberOfChannelStrips()
  for (int x = 1; x < 9; x++) {
    MediaTrack *tr = getMediaTrackForChannel(x);
    if (tr) {
      TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(tr);
      if (pTS) {
        m_pDisplay->changeField(0, x, pTS->showInDisplay().toRawUTF8());
				if (m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_PROX)) 
					m_pDisplay->changeField(2, x, pTS->showInDisplay().toRawUTF8());
      }
    } else {
      m_pDisplay->changeField(0, x, "");
			//			if (! m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_MACKIE_LEVEL_METER))
			m_pDisplay->changeField(1, x, "");
      if (m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_PROX)) {
        m_pDisplay->changeField(2, x, "");
        m_pDisplay->changeField(3, x, "");
      }
    }
  }

	if (m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_PROX)) {
		m_pDisplay->changeField(2, 9, "Master");
		m_pDisplay->showDB(3, 9,
		             *((double *)GetSetMediaTrackInfo(getMediaTrackForChannel(0),
																									"D_VOL", NULL)));
	}
}

void MultiTrackMode::toggleShowInMixer(MediaTrack *tr) {
  // TODO: Add Implementation
  bool *shown = (bool *)GetSetMediaTrackInfo(tr, "B_SHOWINMIXER", NULL);
  *shown = !*shown;
  Undo_BeginBlock();
  GetSetMediaTrackInfo(tr, "B_SHOWINMIXER", shown);
  Undo_EndBlock("Toggle track Shown In Mixer (via Surface)",
                UNDO_STATE_TRACKCFG);
  TrackList_AdjustWindows(false);
  UpdateTimeline();
}

Component **MultiTrackMode::createEditorComponent() {
  if (!m_pTrackStatesEditor)
    m_pTrackStatesEditor = new TrackStatesEditorComponent();

  return reinterpret_cast<Component **>(&m_pTrackStatesEditor);
}

void MultiTrackMode::deleteEditorComponent() {
  safe_delete(m_pTrackStatesEditor)
}
