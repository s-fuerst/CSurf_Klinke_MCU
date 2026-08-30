/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Editor table for the 16 GLOBAL Channel Strips — one row per strip.
 * Modeled on TrackStatesTableComponent.
 *
 *   # | Plugin | Abbrev (≤5) | InsPos | Parameter… | Save | Load | Clear
 *
 * Each row is one ChannelStripMap (a plugin + its VPOT→param mapping). The
 * Plugin/Abbrev/InsPos cells edit the strip's header fields. The Parameter
 * button opens the VPOT→param sub-editor for that strip. Save stores a
 * single strip in the Strips/ folder (name dialog, PlugMap-style); Load
 * lists the existing strip files and replaces the strip with the selected
 * one; Clear resets the whole strip (unassigned). Editing notifies
 * ChannelStripMode::bindingChanged() so the hardware refreshes.
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripAccess.h"
#include "ChannelStripMap.h"

class ChannelStripMode;

#define CST_COL_NR 1
#define CST_COL_PLUGIN 2
#define CST_COL_ABBREV 3
#define CST_COL_INSPOS 4
#define CST_COL_PARAM 5
#define CST_COL_SAVE 6
#define CST_COL_LOAD 7
#define CST_COL_CLEAR 8

class ChannelStripBindingTable : public Component, public TableListBoxModel {
public:
  ChannelStripBindingTable(ChannelStripMode *pMode);
  ~ChannelStripBindingTable();

  void resized() override;

  int getNumRows() override;
  void paintRowBackground(Graphics &g, int row, int w, int h,
                          bool rowIsSelected) override;
  void paintCell(Graphics &g, int row, int col, int w, int h,
                 bool rowIsSelected) override;
  Component *refreshComponentForCell(int row, int col, bool selected,
                                     Component *existing) override;

  ChannelStripMode *getMode() { return m_pMode; }
  const std::vector<ChannelStripAccess::InstalledFX> &installedFX() const {
    return m_installedFX;
  }
  // the strip shown in this row (always valid, may be unassigned)
  ChannelStripMap *stripForRow(int row);
  void notifyBindingChanged();

  void refreshInstalledFX();
  void updateEverything();
  // force recreation of all cell components — needed after a strip change
  // that affects a DIFFERENT cell than the one being edited (e.g. picking a
  // plugin changes the Abbrev cell, clearing the strip empties it)
  void resetCells();

private:
  ChannelStripMode *m_pMode;
  TableListBox *m_table;
  std::vector<ChannelStripAccess::InstalledFX> m_installedFX;
};

// --- custom cell components ---

class CSTPluginCombo : public Component,
                       public TextEditor::Listener,
                       public ListBoxModel {
public:
  CSTPluginCombo(ChannelStripBindingTable &o);
  ~CSTPluginCombo() override;
  void resized() override { m_editor->setBoundsInset(BorderSize(1)); }
  void setRowAndColumn(int r, int c);

  void textEditorTextChanged(TextEditor &) override;
  void textEditorFocusLost(TextEditor &) override;
  void textEditorReturnKeyPressed(TextEditor &) override;
  void textEditorEscapeKeyPressed(TextEditor &) override;

  int getNumRows() override;
  void paintListBoxItem(int row, Graphics &, int w, int h, bool sel) override;
  void listBoxItemClicked(int row, const MouseEvent &) override;

private:
  void applyFilter(const String &text);
  void showPopup();
  void hidePopup();
  void pickFiltered(int filteredRow);
  void setSelectedByIdent(const String &ident);

  ChannelStripBindingTable &owner;
  TextEditor *m_editor;
  int row, column;
  String m_lastValidText;

  std::vector<ChannelStripAccess::InstalledFX> m_filtered;
  Component::SafePointer<ListBox> m_popupList;
};

class CSTAbbrevLabel : public Component, public Label::Listener {
public:
  CSTAbbrevLabel(ChannelStripBindingTable &o);
  ~CSTAbbrevLabel() { deleteAllChildren(); }
  void resized() override { m_label->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void labelTextChanged(Label *) override;
private:
  ChannelStripBindingTable &owner; Label *m_label; int row, column;
};

class CSTInsPosCombo : public Component, public ComboBox::Listener {
public:
  CSTInsPosCombo(ChannelStripBindingTable &o);
  ~CSTInsPosCombo() { deleteAllChildren(); }
  void resized() override { m_combo->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void comboBoxChanged(ComboBox *) override;
private:
  ChannelStripBindingTable &owner; ComboBox *m_combo; int row, column;
};

class CSTParamButton : public Component, public Button::Listener {
public:
  CSTParamButton(ChannelStripBindingTable &o);
  ~CSTParamButton() { deleteAllChildren(); }
  void resized() override { m_button->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void buttonClicked(Button *) override;
private:
  ChannelStripBindingTable &owner; TextButton *m_button; int row, column;
};

// One button class for the Save / Load / Clear columns. Save and Load open
// a JUCE FileChooser (XML); Load overwrites the slot without confirmation.
// Clear resets the whole strip (plugin + mapping) to unassigned.
class CSTStripFileButton : public Component, public Button::Listener {
public:
  enum Action { SAVE = 0, LOAD, CLEAR };
  CSTStripFileButton(ChannelStripBindingTable &o, Action action);
  ~CSTStripFileButton() { deleteAllChildren(); }
  void resized() override { m_button->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void buttonClicked(Button *) override;
private:
  ChannelStripBindingTable &owner; TextButton *m_button; int row, column;
  Action m_action;
};
