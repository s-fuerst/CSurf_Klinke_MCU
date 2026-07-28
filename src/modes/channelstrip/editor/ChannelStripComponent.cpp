/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripComponent.h"
#include "ChannelStripMode.h"

ChannelStripComponent::ChannelStripComponent(ChannelStripMode *pMode)
    : m_pMode(pMode), m_unitLabel(NULL), m_unitCombo(NULL), m_table(NULL) {
  addAndMakeVisible(m_unitLabel = new Label(String(), String("Unit:")));
  m_unitLabel->setJustificationType(Justification::centredRight);

  addAndMakeVisible(m_unitCombo = new ComboBox());
  int nUnits = m_pMode ? m_pMode->numUnits() : 1;
  for (int u = 0; u < nUnits; u++)
    m_unitCombo->addItem(String(u + 1), u + 1);
  m_unitCombo->setSelectedId(1, dontSendNotification);
  m_unitCombo->addListener(this);

  addAndMakeVisible(m_table = new ChannelStripBindingTable(m_pMode));
  m_table->setActiveUnit(0);

  setSize(620, 420);
}

ChannelStripComponent::~ChannelStripComponent() { deleteAllChildren(); }

void ChannelStripComponent::resized() {
  const int margin = 8;
  const int rowH = 24;
  m_unitLabel->setBounds(margin, margin, 40, rowH);
  m_unitCombo->setBounds(margin + 44, margin, 70, rowH);
  m_table->setBounds(margin, margin + rowH + 4, getWidth() - 2 * margin,
                     getHeight() - (margin + rowH + 4) - margin);
}

void ChannelStripComponent::comboBoxChanged(ComboBox *combo) {
  if (combo == m_unitCombo) {
    int u = m_unitCombo->getSelectedId() - 1;
    m_table->setActiveUnit(u);
    m_table->updateEverything();
  }
}

void ChannelStripComponent::updateEverything() {
  if (m_table) {
    m_table->refreshInstalledFX();
    m_table->updateEverything();
  }
}
