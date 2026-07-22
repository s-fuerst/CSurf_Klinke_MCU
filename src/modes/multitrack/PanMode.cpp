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

void PanMode::activate() {
  // call MultiTrackMode::activate() first to handle display init
  // (MultiDisplay::switchToAll for N>1, clear + switch for N=1)
  MultiTrackMode::activate();
  m_pCCSManager->getDisplayHandler()->enableMCUMeter(true);
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
    return true;
  }

  return false;
}

void PanMode::updateDisplay() {
	m_pCCSManager->switchToDisplay(this, m_pDisplay);
	
  MultiTrackMode::updateDisplay();
  // widened from 8 to getNumberOfChannelStrips()
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();
    for (int iTrack = 1; iTrack <= nStrips; iTrack++) {
      MediaTrack *tr = getMediaTrackForChannel(iTrack);
      if (tr) {
        // per-unit ProX check
        HardwareUnit *u = m_pCCSManager->getMCU()->unitForChannel(iTrack);
        if (u && u->isProX()) {
          if (s_flipmode) {
            m_pDisplay->showPan(3, iTrack,
                                *((double *)GetSetMediaTrackInfo(tr, "D_PAN", NULL)));
            m_pDisplay->showDB(1, iTrack,
                              *((double *)GetSetMediaTrackInfo(tr, "D_VOL", NULL)));
          }
          else {
            m_pDisplay->showDB(3, iTrack,
                              *((double *)GetSetMediaTrackInfo(tr, "D_VOL", NULL)));
            m_pDisplay->showPan(1, iTrack,
                                *((double *)GetSetMediaTrackInfo(tr, "D_PAN", NULL)));
          }
        } else {
          if (s_flipmode)
            m_pDisplay->showPan(1, iTrack,
                                *((double *)GetSetMediaTrackInfo(tr, "D_PAN", NULL)));
          else
            m_pDisplay->showDB(1, iTrack,
                              *((double *)GetSetMediaTrackInfo(tr, "D_VOL", NULL)));
        }
      } else {
        m_pDisplay->changeField(1, iTrack, "");
      }
    }
}
