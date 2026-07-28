/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Editor table for one unit's Channel Strip Map: 16 rows (slots), one per
 * VPOT position (1..8 normal, 9..16 Shift). Modeled on
 * TrackStatesTableComponent (hand-written TableListBoxModel).
 *
 *   # | Plugin (EnumInstalledFX combo) | Abbrev (<=5) | InsPos | Param… (button)
 *
 * Editing a cell writes straight into the active unit's ChannelStripMap and
 * notifies ChannelStripMode::bindingChanged() so the hardware reflects it.
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripAccess.h"
#include "ChannelStripBinding.h"
#include "ChannelStripMap.h"

class ChannelStripMode;

// column ids
#define CST_COL_NR 1
#define CST_COL_PLUGIN 2
#define CST_COL_ABBREV 3
#define CST_COL_INSPOS 4
#define CST_COL_PARAM 5

class ChannelStripBindingTable : public Component, public TableListBoxModel {
public:
  ChannelStripBindingTable(ChannelStripMode *pMode);
  ~ChannelStripBindingTable();

  void resized() override;

  int getNumRows() override { return ChannelStripMap::kNumSlots; }
  void paintRowBackground(Graphics &g, int rowNumber, int width, int height,
                          bool rowIsSelected) override;
  void paintCell(Graphics &g, int rowNumber, int columnId, int width,
                 int height, bool rowIsSelected) override;
  Component *refreshComponentForCell(int rowNumber, int columnId,
                                     bool isRowSelected,
                                     Component *existingComponentToUpdate) override;

  // the active unit whose map is shown
  void setActiveUnit(int unit);
  int getActiveUnit() const { return m_activeUnit; }
  void updateEverything() {
    if (m_table)
      m_table->updateContent();
  }

  // access for the custom cell components
  ChannelStripMode *getMode() { return m_pMode; }
  const std::vector<ChannelStripAccess::InstalledFX> &installedFX() const {
    return m_installedFX;
  }
  ChannelStripBinding *bindingForRow(int row); // may be NULL (no track/unit)

  // called by cell components after they mutate a binding
  void notifyBindingChanged();

  // (re)read the current track's installed FX list
  void refreshInstalledFX();

private:
  ChannelStripMode *m_pMode;
  int m_activeUnit;
  TableListBox *m_table;
  std::vector<ChannelStripAccess::InstalledFX> m_installedFX;
};

// --- custom cell components (live in the same translation unit) ---

class CSTPluginCombo : public Component, public ComboBox::Listener {
public:
  CSTPluginCombo(ChannelStripBindingTable &owner);
  ~CSTPluginCombo() { deleteAllChildren(); }
  void resized() override { m_combo->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int row, int column);
  void comboBoxChanged(ComboBox *) override;

private:
  ChannelStripBindingTable &m_owner;
  ComboBox *m_combo;
  int m_row, m_columnId;
};

class CSTAbbrevLabel : public Component, public Label::Listener {
public:
  CSTAbbrevLabel(ChannelStripBindingTable &owner);
  ~CSTAbbrevLabel() { deleteAllChildren(); }
  void resized() override { m_label->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int row, int column);
  void labelTextChanged(Label *) override;

private:
  ChannelStripBindingTable &m_owner;
  Label *m_label;
  int m_row, m_columnId;
};

class CSTInsPosCombo : public Component, public ComboBox::Listener {
public:
  CSTInsPosCombo(ChannelStripBindingTable &owner);
  ~CSTInsPosCombo() { deleteAllChildren(); }
  void resized() override { m_combo->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int row, int column);
  void comboBoxChanged(ComboBox *) override;

private:
  ChannelStripBindingTable &m_owner;
  ComboBox *m_combo;
  int m_row, m_columnId;
};

class CSTParamButton : public Component, public Button::Listener {
public:
  CSTParamButton(ChannelStripBindingTable &owner);
  ~CSTParamButton() { deleteAllChildren(); }
  void resized() override { m_button->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int row, int column);
  void buttonClicked(Button *) override;

private:
  ChannelStripBindingTable &m_owner;
  TextButton *m_button;
  int m_row, m_columnId;
};
