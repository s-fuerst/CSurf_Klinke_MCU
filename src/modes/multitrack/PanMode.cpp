/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PanMode.h"
#include "reaper_plugin.h"
#include "csurf_mcu.h"
#include "McuAssert.h"
#include "Display.h"
#include "HardwareUnit.h"
#include "Tracks.h"

PanMode::PanMode(CCSManager *pManager) : MultiTrackMode(pManager) {}

PanMode::~PanMode(void) {}

void PanMode::ensureVpotValueState(int channelCount) {
  if ((int)m_vpotValueShownTill.size() < channelCount + 1)
    m_vpotValueShownTill.resize(channelCount + 1, 0);
}

bool PanMode::vpotMoved(int channel, int numSteps) {
  if (m_pCCSManager->getVPOT(channel)->isPressed()) {
    numSteps *= 5;
  }

  MediaTrack *tr = getMediaTrackForChannel(channel);
  if (tr) {
    if (s_flipmode) {
      CSurf_SetSurfaceVolume(
          tr, CSurf_OnVolumeChange(tr, numSteps * 11.0 / 31.0, true), NULL);
    } else {
      CSurf_SetSurfacePan(tr, CSurf_OnPanChange(tr, numSteps / 40.0, true),
                          NULL);
    }
    updateVPOTs();

    // Briefly show the value the VPOT just changed (Pan, or Volume when
    // flipped) on this channel's row 1. ProX already shows every value, so
    // only non-ProX units opt in.
    HardwareUnit *u = m_pCCSManager->getMCU()->unitForChannel(channel);
    if (u && !u->isProX()) {
      ensureVpotValueState(m_pCCSManager->getMCU()->availableChannels());
      m_vpotValueShownTill[channel] =
          m_pCCSManager->getLastTime() + VPOT_VALUE_SHOW_MS;
    }

    return true;
  }

  return false;
}

void PanMode::activate() {
  // call MultiTrackMode::activate() first to handle display init
  // (MultiDisplay::switchToAll for N>1, clear + switch for N=1)
  MultiTrackMode::activate();
  m_pCCSManager->getMCU()->enableMCUMeters(true);
}

void PanMode::updateDisplay() {
	m_pCCSManager->switchToDisplay(this, m_pDisplay);
	
  MultiTrackMode::updateDisplay();
  // widened from 8 to getNumberOfChannelStrips()
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  DWORD now = m_pCCSManager->getLastTime();
    for (int iTrack = 1; iTrack <= nStrips; iTrack++) {
      MediaTrack *tr = getMediaTrackForChannel(iTrack);
      if (tr) {
        // per-unit ProX check
        HardwareUnit *u = m_pCCSManager->getMCU()->unitForChannel(iTrack);
        if (u && u->isProX()) {
          if (s_flipmode) {
            m_pDisplay->showPan(3, iTrack,
                                m_pCCSManager->getMCU()->GetSurfacePan(tr));
            m_pDisplay->showDB(1, iTrack,
                              m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
          }
          else {
            m_pDisplay->showDB(3, iTrack,
                              m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
            m_pDisplay->showPan(1, iTrack,
                                m_pCCSManager->getMCU()->GetSurfacePan(tr));
          }
        } else {
          // Non-ProX: row 1 normally shows the value the FADER controls
          // (Volume, or Pan when flipped). For ~1s after the VPOT is turned
          // it instead shows the value the VPOT controls (Pan, or Volume when
          // flipped), which is otherwise not visible on a single-panel unit.
          if (showingVpotValue(iTrack, now)) {
            if (s_flipmode)
              m_pDisplay->showDB(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
            else
              m_pDisplay->showPan(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfacePan(tr));
          } else {
            if (s_flipmode)
              m_pDisplay->showPan(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfacePan(tr));
            else
              m_pDisplay->showDB(1, iTrack,
                  m_pCCSManager->getMCU()->GetSurfaceVolume(tr));
          }
        }
      } else {
        m_pDisplay->changeField(1, iTrack, "");
      }
    }
}
