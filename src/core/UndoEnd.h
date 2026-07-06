/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "reaper_plugin.h" // DWORD
#pragma once

class UndoEnd {
public:
  static UndoEnd *instance();

  virtual ~UndoEnd();

  void callUndoBegin();
  void callUndoEnd(const char *pMessage, const int flags, const int waitInMs);
  void run(DWORD timeInMs);

private:
  UndoEnd() {
    m_sendUndoAfterTime = 0;
    m_waitInMs = 0;
    m_bInBlock = false;
  }
  static UndoEnd *s_instance;

  bool m_bInBlock;
  const char *m_pMessage;
  int m_flags;
  int m_waitInMs; // will be reseted to zero after sendtime is calculated
  DWORD m_sendUndoAfterTime;
};
