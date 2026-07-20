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
  void writeTrackPlugTopLine();
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
  void fillPlugNames();
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
  void clearDisplay() { m_pDisplay->clear(); }

private:
  eSelect m_selectWhat;
  int m_unit; // WP-PlugMode Phase 3: owning unit index
};
