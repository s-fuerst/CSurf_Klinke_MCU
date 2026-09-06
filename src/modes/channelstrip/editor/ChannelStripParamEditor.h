/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Second-level editor for one Channel Strip: maps the strip's plugin
 * parameters onto the 16 VPOT positions (1..8 normal, Shift-1..Shift-8).
 * Opened by the "edit…" button in the main strip table.
 *
 * A **Learn** toggle arms the automatic learn (default OFF — the editor
 * opens for manual parameter selection). When Learn is ON, the armed row
 * is highlighted in RED as a whole (Colour(255,0,0) — the same red
 * PlugMode's mapping editor uses for its learn state), including its
 * parameter dropdown; a row click re-arms learn at that row. Turning a
 * parameter in the floating FX window assigns it to the armed row. One
 * knob gesture emits MANY param-change events; those are coalesced onto
 * ONE row, and the arm advances to the next row ~1 s after the last event
 * (or immediately when a different parameter is touched = new gesture).
 * With Learn OFF no row is highlighted and knob movements are ignored.
 * The learn feed comes from the editor's own PluginWatcher in poll mode
 * (the Part C event feed only reaches PlugMode's watcher).
 *
 * Columns:  # | Parameter | Name (≤6) | Steps | Clear
 *
 * **Steps**: when a parameter is bound to a VPOT row (learn or manual),
 * the discrete detection (ChannelStripAccess::detectDiscreteCount, based on
 * PlugMode's fillDiscreteSteps plus looser fallbacks) is run on just THAT
 * parameter (no full scan) against the live plugin instance.
 * The detected value count is stored in the strip's mapping and shown in
 * this column; 0 = continuous / not detected. The cell is
 * EDITABLE so a failed detection can be corrected by hand (0 = not
 * discrete).
 *
 * A temporary plugin instance is added to the track (end of chain), its
 * floating window is opened, and it is removed when the dialog closes.
 */
#pragma once
#include "JuceHeader.h"
#include "ChannelStripMap.h"

class ChannelStripMode;
class MediaTrack;
class PluginWatcher;

#define CSTP_COL_NR 1
#define CSTP_COL_PARAM 2
#define CSTP_COL_NAME 3
#define CSTP_COL_STEPS 4
#define CSTP_COL_CLEAR 5

class ChannelStripParamEditor : public Component,
                                public TableListBoxModel,
                                private Timer,
                                private Button::Listener {
public:
  ChannelStripParamEditor(ChannelStripMode *pMode, int stripIndex,
                          MediaTrack *tr, int fxSlot);
  ~ChannelStripParamEditor() override;

  void resized() override;

  // TableListBoxModel
  int getNumRows() override;
  void paintRowBackground(Graphics &, int row, int w, int h,
                          bool sel) override;
  void paintCell(Graphics &, int row, int col, int w, int h,
                 bool sel) override;
  Component *refreshComponentForCell(int row, int col, bool sel,
                                     Component *existing) override;
  void selectedRowsChanged(int lastRow) override;

  // Timer
  void timerCallback() override;

  // Button::Listener (Learn toggle)
  void buttonClicked(Button *) override;

  // PluginWatcher signal
  void onParamChanged(MediaTrack *, int slot, int paramNr,
                      double value, String valueStr);

  // used by cell components
  void setVPOTParam(int vpot, int paramIdx);
  void clearVPOT(int vpot); // unbind the row: param -1 + empty name
  ChannelStripMap *getStrip() const { return m_strip; }
  const StringArray &paramNames() const { return m_paramNames; }
  void notifyBindingChanged();
  bool isLearnTarget(int row) const; // armed learn row while Learn is ON
  // Re-colours the parameter dropdowns after the armed row changed (row
  // click, programmatic arm advance, learn toggle). Deferred to keep it out
  // of the listbox's own update callbacks.
  void scheduleLearnHighlightRefresh();

  static void open(ChannelStripMode *pMode, int stripIndex);

  ChannelStripMap *m_strip;
  StringArray m_paramNames;

  ChannelStripMode *m_pMode;
  int m_stripIndex;
  MediaTrack *m_pTrack;
  int m_fxSlot;
  TableListBox *m_table;
  ToggleButton *m_learnButton; // Learn toggle (arms the automatic learn)

  PluginWatcher *m_pWatcher;
  int m_paramChangedConnectionId;
  int m_learnVPOT;         // row the next learn gesture fills; -1 = off
  int m_lastLearnParam;    // param of the current gesture; -1 = fresh row
  int m_learnTicksRemaining; // 100 ms ticks until the advance to the next row
  bool m_ownsTempPlugin;   // true only if we added a temp instance to learn from

  static bool s_dialogOpen;
};

// --- cell components ---

class VpotParamCombo : public Component, public ComboBox::Listener {
public:
  VpotParamCombo(ChannelStripParamEditor &o);
  ~VpotParamCombo() { deleteAllChildren(); }
  void resized() override { m_combo->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void comboBoxChanged(ComboBox *) override;
private:
  ChannelStripParamEditor &owner;
  ComboBox *m_combo;
  int row, col;
};

class VpotNameLabel : public Component, public Label::Listener {
public:
  VpotNameLabel(ChannelStripParamEditor &o);
  ~VpotNameLabel() { deleteAllChildren(); }
  void resized() override { m_label->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void labelTextChanged(Label *) override;
private:
  ChannelStripParamEditor &owner;
  Label *m_label;
  int row, col;
};

class VpotStepsLabel : public Component, public Label::Listener {
public:
  VpotStepsLabel(ChannelStripParamEditor &o);
  ~VpotStepsLabel() { deleteAllChildren(); }
  void resized() override { m_label->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void labelTextChanged(Label *) override;
private:
  ChannelStripParamEditor &owner;
  Label *m_label;
  int row, col;
};

class VpotClearButton : public Component, public Button::Listener {
public:
  VpotClearButton(ChannelStripParamEditor &o);
  ~VpotClearButton() { deleteAllChildren(); }
  void resized() override { m_button->setBoundsInset(BorderSize(2)); }
  void setRowAndColumn(int r, int c);
  void buttonClicked(Button *) override;
private:
  ChannelStripParamEditor &owner;
  TextButton *m_button;
  int row, col;
};
