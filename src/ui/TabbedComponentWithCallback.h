/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once

#include "JuceHeader.h"

class TabbedCallback {
public:
  virtual void selectedTabHasChanged() = 0;
};

class TabbedComponentWithCallback : public juce::TabbedComponent {
public:
  TabbedComponentWithCallback(const TabbedButtonBar::Orientation orientation,
                              TabbedCallback *pCB);
  ~TabbedComponentWithCallback(void);

private:
  TabbedCallback *m_pCallback;
  void currentTabChanged(int newCurrentTabIndex, const String &newTabName);
};
