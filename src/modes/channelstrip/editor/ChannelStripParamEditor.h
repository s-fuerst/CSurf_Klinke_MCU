/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Second-level editor for one Channel Strip: maps the strip's plugin
 * parameters onto the 16 VPOT positions (1..8 normal, Shift-1..Shift-8).
 * Opened by the "edit…" button in the main strip table.
 *
 * Learn mode: select a row in the table, then turn a parameter in the
 * floating FX window — the parameter is automatically assigned to the
 * selected VPOT (one-shot, identical to PlugMap learn behaviour).
 *
 * Columns:  # | Parameter | Name (≤6)
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

class ChannelStripParamEditor : public Component,
                                public TableListBoxModel,
                                private Timer {
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

  // PluginWatcher signal
  void onParamChanged(MediaTrack *, int slot, int paramNr,
                      double value, String valueStr);

  // used by cell components
  void setVPOTParam(int vpot, int paramIdx);
  ChannelStripMap *getStrip() const { return m_strip; }
  const StringArray &paramNames() const { return m_paramNames; }
  void notifyBindingChanged();

  static void open(ChannelStripMode *pMode, int stripIndex);

  ChannelStripMap *m_strip;
  StringArray m_paramNames;

  ChannelStripMode *m_pMode;
  int m_stripIndex;
  MediaTrack *m_pTrack;
  int m_fxSlot;
  TableListBox *m_table;

  PluginWatcher *m_pWatcher;
  int m_paramChangedConnectionId;
  int m_learnVPOT;
  bool m_ownsTempPlugin; // true only if we added a temp instance to learn from

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
