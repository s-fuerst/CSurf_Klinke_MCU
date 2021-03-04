/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "PanMode.h"
#include "reaper_plugin.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "DisplayTrackMeter.h"
//#include "MultiTrackSelector.h"

PanMode::PanMode(CCSManager *pManager) : MultiTrackMode(pManager) {}

PanMode::~PanMode(void) {}

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

bool PanMode::faderTouched(int channel, bool touched) {
  if (channel > 0) {
    m_pCCSManager->getDisplayHandler()->enableMeter(channel, !touched);
  }
  return true;
}

void PanMode::updateDisplay() {
  MultiTrackMode::updateDisplay();
  if (m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_NO_LEVEL_METER) ||
			m_pCCSManager->getMCU()->IsFlagSet(CONFIG_FLAG_PROX)) {
    for (int iTrack = 1; iTrack < 9; iTrack++) {
      MediaTrack *tr = getMediaTrackForChannel(iTrack);
      if (tr) {
        showDB(iTrack, *((double *)GetSetMediaTrackInfo(tr, "D_VOL", NULL)));
      } else {
        m_pDisplay->changeField(1, iTrack, "");
      }
    }
  }
}
