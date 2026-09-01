/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
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
  // Restore the per-parameter poll in frame() for watchers that are NOT fed
  // by the Part C event stream (CSurf_MCU::Extended() routes
  // CSURF_EXT_SETFXPARAM only to PlugMode's watcher). Used by the
  // ChannelStripParamEditor, whose learn feed has no event path: it polls
  // the watched plugin while its dialog is open (100 ms timer, one plugin).
  void setParamFeedFromEvents(bool on) { m_paramFeedFromEvents = on; }
  void frame(DWORD time);
  // Event-driven parameter feed (Part C). REAPER delivers parameter changes
  // through CSURF_EXT_SETFXPARAM with a NORMALIZED value, but this watcher's
  // pipeline (signal value, getParamString -> TrackFX_FormatParamValue, and
  // the Learn step-map key) is raw end-to-end. onParamChangedFromHost() uses
  // the event only as a trigger and re-reads the raw value. While
  // m_paramFeedFromEvents is true, frame() skips the per-parameter poll and
  // this method drives signalParamChanged. Flip m_paramFeedFromEvents to
  // false in the constructor to restore the poll fallback.
  void onParamChangedFromHost(MediaTrack *pTrack, int fxidx, int paramidx);

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
  bool m_paramFeedFromEvents; // true: events drive signalParamChanged (Part C)
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
