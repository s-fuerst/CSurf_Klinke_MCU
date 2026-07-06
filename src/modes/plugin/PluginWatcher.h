/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "boost/signals2.hpp"
#include "reaper_plugin.h"
#include "JuceHeader.h"
#include <map>

class MediaTrack;
class CSurf_MCU;

using boost::signals2::connection;

class PluginWatcher {
public:
  PluginWatcher(CSurf_MCU *pMCU);
  ~PluginWatcher(void);

  void setPlugin(MediaTrack *pMediaTrack, int iSlot);
  void frame(DWORD time);

  typedef boost::signals2::signal<void(MediaTrack *, int, int, double, String)>
      tParamSignal; // Parameters: MediaTrack, Slot, ParameterNummer, Value,
                    // ValueString
  typedef tParamSignal::slot_type tParamSignalSlot;
  int connect2ParamChanged(
      const tParamSignalSlot
          &slot); // the returned int is the id that must be used for disconnect
  void disconnectParamChange(int connectionId);

  typedef boost::signals2::signal<void(MediaTrack *, int, String)>
      tNameSignal; // Parameters: MediaTrack, Slot, PlugName
  typedef tNameSignal::slot_type tNameSignalSlot;
  int connect2NameChanged(
      const tNameSignalSlot
          &slot); // the returned int is the id that must be used for disconnect
  void disconnectNameChange(int connectionId);

  String getParamString(MediaTrack *pMediaTrack, int iSlot, int iParam,
                        double dValue);

protected:
  bool plugExist();
  MediaTrack *m_pMediaTrack;
  int m_iSlot;
  connection m_signalFrameConnection;

  tParamSignal m_signalParamChanged;
  std::map<int, connection> m_activeParamConnections;
  tNameSignal m_signalNameChanged;
  std::map<int, connection> m_activeNameConnections;
  int m_nextConnectionId;

  typedef std::map<int, double> tParamValueCache;
  tParamValueCache m_mapParamValues;

  String m_plugName;
};
