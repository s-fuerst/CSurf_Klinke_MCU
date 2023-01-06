/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "PlugWindowManager.h"
#include "PlugMode2ndOptions.h"
#include "windows.h"

PlugWindowManager::PlugWindowManager(PlugMode* pPlugMode) :
	m_pPlugMode(pPlugMode)
{
}

PlugWindowManager::~PlugWindowManager(void)
{
}

void PlugWindowManager::switchedTo(MediaTrack *pMediaTrack, int iSlot) {
  if (m_pPlugMode->isModifierPressed(VK_OPTION))
    return;

  if (m_pPlugMode->isModifierPressed(VK_ALT)) {
    if (isOptionSetTo(PMO_ALT_OPEN, PMOA_OPEN_FLOATING)) {
      if (TrackFX_GetFloatingWindow(pMediaTrack, iSlot)) {
        TrackFX_Show(pMediaTrack, iSlot, 2);
      } else {
        openFloating(pMediaTrack, iSlot);
      }
    } else if (isOptionSetTo(PMO_ALT_OPEN, PMOA_OPEN_CHAIN) ||
               isOptionSetTo(PMO_ALT_OPEN, PMOA_OPEN_CHAIN_CLOSE_FLOAT)) {
      if (TrackFX_GetChainVisible(pMediaTrack) == -1 ||
          iSlot != TrackFX_GetChainVisible(pMediaTrack)) // -1 means not visible
        TrackFX_Show(pMediaTrack, iSlot, 1);
      else
        TrackFX_Show(pMediaTrack, iSlot, 0);

      if (isOptionSetTo(PMO_ALT_OPEN, PMOA_OPEN_CHAIN_CLOSE_FLOAT))
        allowOnlySelectedFloat(NULL, -1); // this close all floatings
    }
    return;
  }

  if (isOptionSetTo(PMO_GUI_FOLLOW, PMOA_IF_CHAIN_OPEN)) {
    if (TrackFX_GetChainVisible(pMediaTrack) != -1) { // -1 means not visible
      TrackFX_Show(pMediaTrack, iSlot, 1);
    }
  } else if (isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_CHAIN)) {
    TrackFX_Show(pMediaTrack, iSlot, 1);
  } else if (isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_FLOATING)) {
    openFloating(pMediaTrack, iSlot);
  }
}

void PlugWindowManager::openFloating(MediaTrack *pMediaTrack, int iSlot) {
  if (TrackFX_GetChainVisible(pMediaTrack) != -1 &&
      !isOptionSetTo(PMO_MCU_FOLLOW, PMOA_OFF)) { // -1 means not visible
    TrackFX_Show(pMediaTrack, iSlot, 1);
  }
  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_ONE_MCU) &&
      m_lastFloat.hwnd != NULL) {
    HWND actualHwnd =
        TrackFX_GetFloatingWindow(m_lastFloat.pMediaTrack, m_lastFloat.iSlot);
    if (actualHwnd == m_lastFloat.hwnd) {
      TrackFX_Show(m_lastFloat.pMediaTrack, m_lastFloat.iSlot, 2);
    }
  }
  TrackFX_Show(pMediaTrack, iSlot, 3);
  if (isOptionSetTo(PMO_LIMIT_FLOATING, PMOA_ONLY_ONE_MCU) &&
      isOptionSetTo(PMO_GUI_FOLLOW, PMOA_OPEN_FLOATING))
    m_lastFloat.set(TrackFX_GetFloatingWindow(pMediaTrack, iSlot), pMediaTrack,
                    iSlot);
}

void PlugWindowManager::allowOnlySelectedFloat(MediaTrack *pAllowTrack,
                                               int iAllowSlot) {
  for (TrackIterator ti; !ti.end(); ++ti) {
    int numFX = TrackFX_GetCount(*ti);
    for (int i = 0; i < numFX; i++) {
      HWND actualHWND = TrackFX_GetFloatingWindow(*ti, i);
      if (actualHWND != NULL && !(pAllowTrack == *ti && i == iAllowSlot)) {
        TrackFX_Show(*ti, i, 2);
      }
    }
  }
}

void PlugWindowManager::closeAll() {
  for (TrackIterator ti; !ti.end(); ++ti) {
    if (TrackFX_GetChainVisible(*ti) >= 0) {
      TrackFX_Show(*ti, TrackFX_GetChainVisible(*ti), 0);
    }
  }

  allowOnlySelectedFloat(NULL, -1);
}

void PlugWindowManager::moveWnd() {
  if (is2ndOptionSetTo(PMO2_MOVE, PMO2A_ON)) {
    tListHWND actualHWNDs;

    for (TrackIterator ti; !ti.end(); ++ti) {
      int numFX = TrackFX_GetCount(*ti);
      for (int i = 0; i < numFX; i++) {
        HWND hwnd = TrackFX_GetFloatingWindow(*ti, i);
        if (m_knownHWND.find(hwnd) == m_knownHWND.end()) {
          if (hwnd) {
            RECT rect;
            ::GetWindowRect(hwnd, &rect);
            if (is2ndOptionSetTo(PMO2_MOVE, PMO2A_ON)) {
              ::SetWindowPos(hwnd,                   // handle to window
                             HWND_TOP,               // placement-order handle
#ifdef KLINKE
														 -1280,
#else														 
                             0,                      // horizontal position
#endif
                             0,                      // vertical position
                             rect.right - rect.left, // width
                             rect.bottom - rect.top, // height
                             SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOREDRAW |
                                 SWP_NOSIZE | SWP_NOSENDCHANGING |
                                 SWP_NOZORDER); // window-positioning options
            }
          }
        }
        actualHWNDs.insert(hwnd);
      }
    }
    m_knownHWND = actualHWNDs;
  }
}

// keepInFront:
//  if (is2ndOptionSetTo(PMO2_KEEP_IN_FRONT, PMO2A_ON)) {
//    ::SetWindowPos(hwnd,       // handle to window
//      HWND_TOPMOST,  // placement-order handle
//      rect.left,     // horizontal position
//      rect.top,      // vertical position
//      rect.right - rect.left,  // width
//      rect.bottom- rect.top, // height
//      SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOSIZE |
//      SWP_NOSENDCHANGING | SWP_NOMOVE); // window-positioning options
//  }
