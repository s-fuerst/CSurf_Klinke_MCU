/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "MultiTrackMode.h"
#include "MultiTrackMeterBridge.h"
#include "ccsmanager.h"
#include "csurf_mcu.h"
#include "assert.h"
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
  m_pDisplay = new Display(pManager->getDisplayHandler(), 4);
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

  int msize = Tracks::instance()->getNumMediaTracksOnMCU();

  if (Tracks::instance()->getGlobalOffset() >= msize &&
      Tracks::instance()->getGlobalOffset() > 0) {
    Tracks::instance()->setGlobalOffset(max(msize - 8, 0));
    // update all of the sliders
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
          m_pCCSManager->setRecLED(this, channel, *pRecMode ? LED_OFF : LED_ON);
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
    nr = (int)GetSetMediaTrackInfo(pMT, "IP_TRACKNUMBER", NULL);

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

  int movesize;
  switch (button) {
  case B_BANK_DOWN:
    if (isModifierPressed(VK_SHIFT))
      movesize = -8;
    else
      movesize = -m_pCCSManager->getMCU()->GetSize();
    movesize += Tracks::instance()->getNumberOfAnchors();
    break;
  case B_CHANNEL_DOWN:
    movesize = -1;
    break;
  case B_CHANNEL_UP:
    movesize = 1;
    break;
  case B_BANK_UP:
    if (isModifierPressed(VK_SHIFT))
      movesize = 8;
    else
      movesize = m_pCCSManager->getMCU()->GetSize();
    movesize -= Tracks::instance()->getNumberOfAnchors();
    break;
  default:
    ASSERT_M(true, "unknown button");
    break;
  }

  // TODO: check this (g_mcu_list is always empty so maxfaderpos is zero, but
  // what should this code do anyway?)
  int maxfaderpos = 0;
  for (int x = 0; x < g_mcu_list.GetSize(); x++) {
    CSurf_MCU *item = g_mcu_list.Get(x);
    if (item) {
      if (item->GetOffset() + 8 > maxfaderpos)
        maxfaderpos = item->GetOffset() + 8;
    }
  }

  int msize = Tracks::instance()->getNumMediaTracksOnMCU();
  if (movesize > 1 &&
      (Tracks::instance()->getGlobalOffset() + maxfaderpos >= msize))
    return true;

  if (movesize > 1 &&
      (Tracks::instance()->getGlobalOffset() + movesize + 1 > msize))
    return true;

  Tracks::instance()->setGlobalOffset(movesize +
                                      Tracks::instance()->getGlobalOffset());

  if (Tracks::instance()->getGlobalOffset() >= msize)
    Tracks::instance()->setGlobalOffset(msize - 1);
  if (Tracks::instance()->getGlobalOffset() < 0)
    Tracks::instance()->setGlobalOffset(0);

  // update all of the sliders
  TrackList_UpdateAllExternalSurfaces();

  //  updateAssignmentDisplay();

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
        *pMonStatus = *pMonStatus ? 0 : 1;
        GetSetMediaTrackInfo(tr, "I_RECMON", (bool *)pMonStatus);
        CSurf_OnRecArmChange(tr, -1); // workaround for missing gui update
        CSurf_OnRecArmChange(tr, -1);
      }
    } else if (isModifierPressed(VK_OPTION)) {
      int *pRecArm = (int *)GetSetMediaTrackInfo(tr, "I_RECARM", NULL);
      if (*pRecArm) {
        int *pRecMode = (int *)GetSetMediaTrackInfo(tr, "I_RECMODE", NULL);
        *pRecMode = *pRecMode ? 0 : 2;
        GetSetMediaTrackInfo(tr, "I_RECMODE", (bool *)pRecMode);
        CSurf_OnRecArmChange(tr, -1); // workaround for missing gui update
        CSurf_OnRecArmChange(tr, -1);
      }
    } else {
      CSurf_OnRecArmChange(tr, -1);
    }
    updateRecLEDs();

    return true;
  }

  return false;
}

bool MultiTrackMode::buttonMute(int channel, bool pressed) {
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
              CSurf_OnSelectedChange(track, 1);
          }
        } else if (m_toTrackNr > m_lastSelectedTrackNr) {
          for (int trackNr = m_lastSelectedTrackNr; trackNr <= m_toTrackNr;
               trackNr++) {
            MediaTrack *track = CSurf_TrackFromID(trackNr, false);
            if (isModifierPressed(VK_SHIFT) ||
                Tracks::instance()->isTrackInFilter(track))
              CSurf_OnSelectedChange(track, 1);
          }
        }
      }

    } else if (isModifierPressed(VK_CONTROL)) {
      CSurf_OnSelectedChange(tr, -1);
      m_lastSelectedTrackNr = MediaTrackInfo::getTrackNr(tr);
    } else {
      m_lastSelectedTrackNr = MediaTrackInfo::getTrackNr(tr);
      m_pCCSManager->getMCU()->UnselectAllTracks();

      // Select this track
      CSurf_OnSelectedChange(tr, 1);
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
        m_pDisplay->showDB(1, id, volume);
      }
    }
  }
}

void MultiTrackMode::trackPan(int id, double pan) {
  MIDIOUT
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
  for (int x = 1; x < 9; x++) {
    MediaTrack *tr = getMediaTrackForChannel(x);
    if (tr) {
      TrackState *pTS = Tracks::instance()->getTrackStateForMediaTrack(tr);
      if (pTS) {
        m_pDisplay->changeField(0, x, pTS->showInDisplay().toCString());
				if (m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_PROX)) 
					m_pDisplay->changeField(2, x, pTS->showInDisplay().toCString());
      }
    } else {
      m_pDisplay->changeField(0, x, "");
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
