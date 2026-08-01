/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "Selector.h"

class PlugMode;

class PlugModeSelector : public Selector {
public:
  PlugModeSelector(DisplayHandler *pDH, PlugMode *pCM) : Selector(pDH) {
    m_pPlugMode = pCM;
  }

  void activateSelector() = 0;

  bool select(int index) = 0;

protected:
  void writeTrackPlugTopLine(Display *d);
  void writePlugBankPageTopLine();

  PlugMode *m_pPlugMode;
};

class PlugSelector : public PlugModeSelector {

public:
  PlugSelector(DisplayHandler *pDH, PlugMode *pPM)
      : PlugModeSelector(pDH, pPM) {}

  void activateSelector();

  bool select(int index);

private:
  int m_startWith;
  // multi-unit plugin list: render the window on every unit's display and
  // route each unit's handler to it
  void renderAllUnits();
  void refreshHeaderAllUnits();
  void fillPlugNames(Display *d, int unit);
};

class BankPagePlugSelector : public PlugModeSelector {
public:
  enum eSelect { NOTHING = 0, BANK, PAGE, PLUG };

  BankPagePlugSelector(DisplayHandler *pDH, PlugMode *pPM, int unit);

  // from Selector
  void activateSelector();
  bool select(int index);

  void activateSelector(eSelect newSelect);

  eSelect getWhatToSelect() { return m_selectWhat; }
  int  getUnit() const { return m_unit; }

  void updateDisplay();
  // Render this unit's plugin range into the selector display, independent
  // of m_selectWhat. Used by PlugMode to broadcast the PLUG overlay to every
  // unit while Select is held on any unit (all displays show the plugin map).
  void renderPlugOverlay();
  void clearDisplay() { m_pDisplay->clear(); }

private:
  eSelect m_selectWhat;
  int m_unit; // owning unit index
};
