/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once

#include <boost/signals2.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include "JuceHeader.h"
#include <list>
#include <set>
#include "csurf.h"

using boost::signals2::connection;

class MediaTrack;

class PlugInstanceInfo {
public:
  PlugInstanceInfo(MediaTrack *pMediaTrack, int iSlot);

  bool existAndIsSame();

  typedef boost::tuple<bool, MediaTrack *, int>
      tMoveToResult; // hasMoved, Track GUID, slot
  tMoveToResult movedTo();

private:
  bool compareWith(MediaTrack *pMediaTrack, int iSlot);

  MediaTrack *m_pMediaTrack;
  int m_slot;
  String m_plugGUID;
};

class PlugMoveWatcher {
public:
  typedef boost::signals2::signal<void(MediaTrack *, int, MediaTrack *, int)>
      tPlugMoveSignal; // GUID oldPos, slot oldPos, GUID newPos, slot newPos
  typedef boost::signals2::signal<void()> tPlugMoveFinishedSignal;
  typedef tPlugMoveSignal::slot_type tPlugMoveSignalSlot;
  typedef tPlugMoveFinishedSignal::slot_type tPlugMoveFinishedSignalSlot;

  static PlugMoveWatcher *instance();
  virtual ~PlugMoveWatcher(void);

  int connectPlugMoveSignal(const tPlugMoveSignalSlot &slot);
  void disconnectPlugMoveSignal(int connectionId);

  int connectPlugMoveFinishedSignal(const tPlugMoveFinishedSignalSlot &slot);
  void disconnectPlugMoveFinishedSignal(int connectionId);

  void checkMovementForTrack(MediaTrack *pTrack);
	void checkMovement();

  void trackRemoved(MediaTrack *pMT);

private:
  PlugMoveWatcher(void);

  static PlugMoveWatcher *s_instance;

  std::map<int, connection> m_activePlugMoveConnections;
  typedef boost::tuple<MediaTrack *, int> tPlug;
  typedef std::map<tPlug, PlugInstanceInfo *> tPlugInstances;
  tPlugInstances m_plugInstanceInfos;
  int m_nextConnectionId;

  tPlugMoveSignal signalPlugMove;
  tPlugMoveFinishedSignal signalPlugMoveFinished;
  tPlugInstances::iterator m_iterPlugInstances;

  int m_trackRemovedConnection;
};
