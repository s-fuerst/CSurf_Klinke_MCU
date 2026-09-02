/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Small dialogs for the Channel Strip editor's file management, modeled on
 * the PlugMap save dialog (name entry + file list, no native file choosers).
 * Both dialogs share the same layout (list on top, status line, right-aligned
 * Delete / Ok-Load / Cancel button row, 400x300):
 *
 *   ChannelStripSaveDialog — title, text field for the file name (a sensible
 *   default is pre-filled) plus a list of all existing files in the target
 *   folder (clicking a list entry pre-fills the name field, PlugMap style).
 *   On Ok the onOk callback is invoked; returning false keeps the dialog
 *   open and shows the error in the status line.
 *   ChannelStripLoadDialog — a ListBox with all existing files of one
 *   category (single strips or complete sets). On Load the onOk callback is
 *   invoked with the selected file name; returning false keeps the dialog
 *   open.
 *   Both dialogs also offer a Delete button (greyed out while no list
 *   entry is selected, enabled while one is): it asks for
 *   confirmation (a short synchronous Yes/No AlertWindow — safe in this
 *   plugin), then invokes the onDelete callback with the selected file
 *   name. Returning false keeps the dialog open and shows the error in the
 *   status line; returning true removes the entry from the dialog's own
 *   list.
 *
 * IMPORTANT: these dialogs are launched ASYNCHRONOUSLY (LaunchOptions +
 * launchAsync, the same pattern as ChannelStripParamEditor) — NOT with
 * DialogWindow::runModalLoop(). A synchronous modal loop deadlocks in the
 * REAPER csurf plugin on Linux, where REAPER's main loop pumps JUCE's X11
 * queue and the JUCE "message thread" assumptions of runModalLoop do not
 * hold.
 */
#pragma once
#include "JuceHeader.h"
#include <functional>

class ChannelStripSaveDialog : public Component, public Button::Listener,
                               public ListBoxModel {
public:
  // onOk receives the trimmed name; returning false keeps the dialog open.
  // onDelete receives the selected file name from the list (with .xml);
  // returning false keeps the dialog open. The Delete button is greyed out
  // (disabled) while no list entry is selected.
  ChannelStripSaveDialog(const String &title, const String &defaultName,
                         const StringArray &existingFiles,
                         std::function<bool(const String &)> onOk,
                         std::function<bool(const String &)> onDelete);
  ~ChannelStripSaveDialog() override { deleteAllChildren(); }
  void resized() override;
  String chosenName() const { return m_name ? m_name->getText().trim() : String(); }

  // ListBoxModel (existing files; click = pre-fill the name field)
  int getNumRows() override { return m_existing.size(); }
  void paintListBoxItem(int row, Graphics &g, int w, int h, bool sel) override;
  void listBoxItemClicked(int row, const MouseEvent &) override;

private:
  void buttonClicked(Button *button) override;
  Label *m_label;
  Label *m_status;
  TextEditor *m_name;
  ListBox *m_files;
  TextButton *m_ok;
  TextButton *m_cancel;
  TextButton *m_delete; // disabled while no entry is selected
  StringArray m_existing;
  std::function<bool(const String &)> m_onOk;
  std::function<bool(const String &)> m_onDelete;
};

class ChannelStripLoadDialog : public Component, public Button::Listener,
                               public ListBoxModel {
public:
  // onOk receives the selected file name (with .xml); returning false keeps
  // the dialog open.
  // onDelete receives the selected file name; returning false keeps the
  // dialog open. The Delete button is greyed out (disabled) while no list
  // entry is selected.
  ChannelStripLoadDialog(const String &title, const StringArray &files,
                         std::function<bool(const String &)> onOk,
                         std::function<bool(const String &)> onDelete);
  ~ChannelStripLoadDialog() override { deleteAllChildren(); }
  void resized() override;
  String chosenFile() const {
    if (!m_list)
      return String();
    const int index = m_list->getSelectedRow();
    return (index >= 0 && index < m_files.size()) ? m_files[index] : String();
  }

  // ListBoxModel
  int getNumRows() override { return m_files.size(); }
  void paintListBoxItem(int row, Graphics &g, int w, int h, bool sel) override;
  void listBoxItemClicked(int row, const MouseEvent &) override;

private:
  void buttonClicked(Button *button) override;
  StringArray m_files;
  Label *m_status;
  ListBox *m_list;
  TextButton *m_ok;
  TextButton *m_cancel;
  TextButton *m_delete; // disabled while no entry is selected
  std::function<bool(const String &)> m_onOk;
  std::function<bool(const String &)> m_onDelete;
};

// Async launch helpers (launchAsync pattern; the DialogWindow deletes itself
// when the modal state ends).
void openStripSaveDialog(const String &title, const String &defaultName,
                         const StringArray &existingFiles,
                         std::function<bool(const String &)> onOk,
                         std::function<bool(const String &)> onDelete);
void openStripLoadDialog(const String &title, const StringArray &files,
                         std::function<bool(const String &)> onOk,
                         std::function<bool(const String &)> onDelete);
