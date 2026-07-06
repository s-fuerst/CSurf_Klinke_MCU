/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "TabbedComponentWithCallback.h"

TabbedComponentWithCallback::TabbedComponentWithCallback(
    const TabbedButtonBar::Orientation orientation, TabbedCallback *pCB)
    : TabbedComponent(orientation), m_pCallback(pCB) {}

TabbedComponentWithCallback::~TabbedComponentWithCallback(void) {}

void TabbedComponentWithCallback::currentTabChanged(
    const int newCurrentTabIndex, const String &newTabName) {
  m_pCallback->selectedTabHasChanged();
}
