/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Type-ahead plugin picker field — the plugin selection shared by the
 * Channel Strip editor (ChannelStripBindingTable) and the PlugMode plugin
 * category editor (PlugCategoryEditor).
 *
 * Behaviour (unchanged from the original ChannelStripBindingTable cell):
 *   • a TextEditor shows the current plugin name and filters as you type,
 *   • a popup ListBox lists the matches while the editor has focus,
 *   • row 0 of the popup is an "empty" sentinel that clears the assignment,
 *   • RETURN picks the first real match, ESC restores the last valid text,
 *   • focus loss closes the popup.
 *
 * The owner stores the picked value; the field only displays and reports.
 */
#pragma once
#include "JuceHeader.h"
#include "InstalledFx.h"

class PluginComboField;

/**
 * Implemented by the component that owns a set of PluginComboField cells
 * (a TableListBox model or a plain editor). All three callbacks are
 * delivered on the message thread; pluginPickFinished() is DEFERRED, so the
 * owner may delete the field from it (e.g. a table reset).
 */
class PluginComboFieldOwner {
public:
  virtual ~PluginComboFieldOwner() {}

  // The installed FX list used to build the popup.
  virtual const InstalledFxList &installedFxList() const = 0;

  // The user picked an entry: `ident` empty means the popup's "empty" row
  // was picked (clear the assignment). `displayName` is the text the field
  // now shows (empty for the clear row). The field has already updated its
  // editor text — do NOT touch the field from here.
  virtual void pluginPicked(int row, int column, const String &ident,
                            const String &displayName) = 0;

  // Called (deferred to the next message-loop turn) after pluginPicked() so
  // the owner can refresh itself. The owner is allowed to delete the field
  // component here (a TableListBox::setModel reset does).
  virtual void pluginPickFinished(int row, int column) = 0;
};

class PluginComboField : public Component,
                         public TextEditor::Listener,
                         public ListBoxModel {
public:
  PluginComboField(PluginComboFieldOwner &owner);
  ~PluginComboField() override;

  void resized() override;

  // Configure the cell this field stands for. `displayText` is what the
  // editor shows (installed plugin name, a raw ident for a missing plugin,
  // or an empty string). Safe to call repeatedly (cell reuse).
  void setRowAndColumn(int row, int column, const String &displayText);
  int getRow() const { return m_row; }
  int getColumn() const { return m_column; }
  // Text currently in the editor (mostly for tests / debugging).
  String getText() const;

  // TextEditor::Listener
  void textEditorTextChanged(TextEditor &) override;
  void textEditorFocusLost(TextEditor &) override;
  void textEditorReturnKeyPressed(TextEditor &) override;
  void textEditorEscapeKeyPressed(TextEditor &) override;

  // ListBoxModel
  int getNumRows() override;
  void paintListBoxItem(int row, Graphics &, int w, int h, bool sel) override;
  void listBoxItemClicked(int row, const MouseEvent &) override;

private:
  void applyFilter(const String &text);
  void showPopup();
  void hidePopup();
  void pickFiltered(int filteredRow);

  PluginComboFieldOwner &owner;
  TextEditor *m_editor;
  int m_row;
  int m_column;
  String m_lastValidText;

  // Popup contents: row 0 is always the "empty" sentinel.
  InstalledFxList m_filtered;
  Component::SafePointer<ListBox> m_popupList;
};
