/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "DisplayHandler.h"
#include "Display.h"
#include "csurf_mcu.h"
#pragma once

class Display;

class Selector {
public:
  Selector(DisplayHandler *pDH, Display *pDisplay = NULL) {
    m_pDisplayHandler = pDH;
    m_pDisplay = pDisplay ? pDisplay : new Display(pDH, 2);
  }

  virtual ~Selector(void) { safe_delete(m_pDisplay); }

  virtual Display *getSelectorDisplay() { return m_pDisplay; }
  virtual void activateSelector() = 0;
  // returns true if selector should still be active
  virtual bool select(int index) = 0; // 0-7

protected:
  Display *m_pDisplay;
  DisplayHandler *m_pDisplayHandler;
};
