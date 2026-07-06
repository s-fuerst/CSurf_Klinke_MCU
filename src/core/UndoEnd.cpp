/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "UndoEnd.h"
#include "csurf_mcu.h"
#include "McuAssert.h"

UndoEnd *UndoEnd::s_instance = NULL;

UndoEnd *UndoEnd::instance() {
  if (s_instance == NULL) {
    s_instance = new UndoEnd();
  }
  return s_instance;
}

void UndoEnd::callUndoBegin() {
  if (!m_bInBlock) {
    Undo_BeginBlock();
    m_bInBlock = true;
  }
}

void UndoEnd::callUndoEnd(const char *pMessage, const int flags,
                          const int waitInMs) {
  ASSERT(m_bInBlock);

  m_pMessage = pMessage;
  m_flags = flags;
  m_waitInMs = waitInMs;
}

void UndoEnd::run(DWORD timeInMs) {
  if (m_waitInMs > 0) {
    m_sendUndoAfterTime = timeInMs + m_waitInMs;
    m_waitInMs = 0;
  }

  if (m_bInBlock && timeInMs > m_sendUndoAfterTime) {
    Undo_EndBlock(m_pMessage, m_flags);
    m_bInBlock = false;
    m_sendUndoAfterTime = 0;
  }
}

UndoEnd::~UndoEnd() { s_instance = NULL; }
