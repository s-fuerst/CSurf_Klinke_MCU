/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "PluginWatcher.h"
#include "reaper_plugin.h"
#include "csurf_mcu.h"
#include "boost/bind.hpp"
#include "PlugAccess.h"

PluginWatcher::PluginWatcher(CSurf_MCU *pMCU)
    : m_pMediaTrack(NULL), m_iSlot(-1), m_nextConnectionId(0) {
  m_signalFrameConnection =
      pMCU->connect2FrameSignal(boost::bind(&PluginWatcher::frame, this, _1));
}

PluginWatcher::~PluginWatcher(void) { m_signalFrameConnection.disconnect(); }

void PluginWatcher::setPlugin(MediaTrack *pMediaTrack, int iSlot) {
  m_pMediaTrack = pMediaTrack;
  m_iSlot = iSlot;
  m_mapParamValues.clear();
  m_plugName = String::empty;
}

void PluginWatcher::frame(DWORD time) {
  if (!plugExist())
    return;

  // check name
  if (m_activeNameConnections.size() > 0) {
    char paramName[80];
    bool valid = TrackFX_GetFXName(m_pMediaTrack, m_iSlot, paramName, 79);
    String name = String(paramName);

    if (name != m_plugName) {
      if (m_plugName != String::empty)
        m_signalNameChanged(m_pMediaTrack, m_iSlot, name);
      m_plugName = name;
    }
  }

  // check params
  if (m_activeParamConnections.size() > 0) {
    int numParams = TrackFX_GetNumParams(m_pMediaTrack, m_iSlot);

    double minVal, maxVal;
    for (int iParam = 0; iParam < numParams - 2; iParam++) {
      double value =
          TrackFX_GetParam(m_pMediaTrack, m_iSlot, iParam, &minVal, &maxVal);
      if (m_mapParamValues.count(iParam) > 0) {
        if (value != m_mapParamValues[iParam]) {
          m_signalParamChanged(
              m_pMediaTrack, m_iSlot, iParam, value,
              getParamString(m_pMediaTrack, m_iSlot, iParam, value));
        }
      }
      m_mapParamValues[iParam] = value;
    }
  }
}

String PluginWatcher::getParamString(MediaTrack *pMediaTrack, int iSlot,
                                     int iParam, double dValue) {
  char valueString[80];
  bool valid = TrackFX_FormatParamValue(pMediaTrack, m_iSlot, iParam, dValue,
                                        valueString, 79);
  if (valid) {
    return PlugAccess::longNameFromCString(valueString);
  } else {
    return String::formatted(JUCE_T("%.2f"), dValue);
  }
}

int PluginWatcher::connect2ParamChanged(const tParamSignalSlot &slot) {
  m_mapParamValues.clear();
  m_activeParamConnections[++m_nextConnectionId] =
      m_signalParamChanged.connect(slot);
  return m_nextConnectionId;
}

int PluginWatcher::connect2NameChanged(const tNameSignalSlot &slot) {
  m_activeNameConnections[++m_nextConnectionId] =
      m_signalNameChanged.connect(slot);
  return m_nextConnectionId;
}

bool PluginWatcher::plugExist() {
  return (m_pMediaTrack && TrackFX_GetCount(m_pMediaTrack) > m_iSlot);
}

void PluginWatcher::disconnectParamChange(int connectionId) {
  m_activeParamConnections[connectionId].disconnect();
  m_activeParamConnections.erase(m_activeParamConnections.find(connectionId));
}

void PluginWatcher::disconnectNameChange(int connectionId) {
  m_activeNameConnections[connectionId].disconnect();
  m_activeNameConnections.erase(m_activeNameConnections.find(connectionId));
}
