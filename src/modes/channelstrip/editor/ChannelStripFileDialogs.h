/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Small dialogs for the Channel Strip editor's file management, modeled on
 * the PlugMap save dialog (name entry + file list, no native file choosers):
 *
 *   ChannelStripSaveDialog — text field for the file name (a sensible
 *   default is pre-filled) plus a list of all existing files in the target
 *   folder (clicking a list entry pre-fills the name field, PlugMap style).
 *   On Ok the onOk callback is invoked; returning false keeps the dialog
 *   open and shows the error in the status line.
 *   ChannelStripLoadDialog — a ListBox with all existing files of one
 *   category (single strips or complete sets). On Load the onOk callback is
 *   invoked with the selected index; returning false keeps the dialog open.
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
  ChannelStripSaveDialog(const String &title, const String &defaultName,
                         const StringArray &existingFiles,
                         std::function<bool(const String &)> onOk);
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
  StringArray m_existing;
  std::function<bool(const String &)> m_onOk;
};

class ChannelStripLoadDialog : public Component, public Button::Listener,
                               public ListBoxModel {
public:
  // onOk receives the selected row; returning false keeps the dialog open.
  // `note` is shown in the status line (e.g. "no files found in <dir>").
  ChannelStripLoadDialog(const String &title, const StringArray &files,
                         const String &note,
                         std::function<bool(int)> onOk);
  ~ChannelStripLoadDialog() override { deleteAllChildren(); }
  void resized() override;
  int chosenIndex() const { return m_list ? m_list->getSelectedRow() : -1; }

  // ListBoxModel
  int getNumRows() override { return m_files.size(); }
  void paintListBoxItem(int row, Graphics &g, int w, int h, bool sel) override;

private:
  void buttonClicked(Button *button) override;
  StringArray m_files;
  Label *m_status;
  ListBox *m_list;
  TextButton *m_ok;
  TextButton *m_cancel;
  std::function<bool(int)> m_onOk;
};

// Async launch helpers (launchAsync pattern; the DialogWindow deletes itself
// when the modal state ends).
void openStripSaveDialog(const String &title, const String &defaultName,
                         const StringArray &existingFiles,
                         std::function<bool(const String &)> onOk);
void openStripLoadDialog(const String &title, const StringArray &files,
                         const String &note, std::function<bool(int)> onOk);
