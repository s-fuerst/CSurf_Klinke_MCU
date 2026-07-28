/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripComponent.h"
#include "ChannelStripMode.h"

ChannelStripComponent::ChannelStripComponent(ChannelStripMode *pMode)
    : m_pMode(pMode), m_table(NULL) {
  addAndMakeVisible(m_table = new ChannelStripBindingTable(m_pMode));
  setSize(640, 420);
}

ChannelStripComponent::~ChannelStripComponent() { deleteAllChildren(); }

void ChannelStripComponent::resized() {
  const int m = 8;
  if (m_table)
    m_table->setBounds(m, m, getWidth() - 2 * m, getHeight() - 2 * m);
}

void ChannelStripComponent::updateEverything() {
  if (m_table) {
    m_table->refreshInstalledFX();
    m_table->updateEverything();
  }
}
