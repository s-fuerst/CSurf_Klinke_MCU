/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripParamEditor.h"
#include "ChannelStripMode.h"
#include "ChannelStripAccess.h"
#include "CCSManager.h"
#include "PluginWatcher.h"
#include "csurf_mcu.h"
#include "KlinkeLookAndFeel.h"
#include <boost/bind.hpp>

bool ChannelStripParamEditor::s_dialogOpen = false;

// ===== ChannelStripParamEditor =====

ChannelStripParamEditor::ChannelStripParamEditor(ChannelStripMode *pMode,
                                                  int stripIndex,
                                                  MediaTrack *tr, int fxSlot)
    : m_pMode(pMode), m_stripIndex(stripIndex), m_pTrack(tr),
      m_fxSlot(fxSlot), m_strip(NULL), m_table(NULL),
      m_pWatcher(NULL), m_paramChangedConnectionId(-1), m_learnVPOT(-1),
      m_ownsTempPlugin(false) {

  m_strip = m_pMode ? m_pMode->getStrip(stripIndex) : NULL;
  int numParams =
      (tr && fxSlot >= 0) ? ChannelStripAccess::getNumParams(tr, fxSlot) : 0;
  for (int p = 0; p < numParams; p++)
    m_paramNames.add(ChannelStripAccess::getParamName(tr, fxSlot, p));

  // PluginWatcher for learn
  CSurf_MCU *mcu = m_pMode ? m_pMode->getCCSManager()->getMCU() : NULL;
  if (mcu && tr && fxSlot >= 0) {
    m_pWatcher = new PluginWatcher(mcu);
    m_pWatcher->setPlugin(tr, fxSlot);
    m_paramChangedConnectionId = m_pWatcher->connect2ParamChanged(
        boost::bind(&ChannelStripParamEditor::onParamChanged, this,
                    _1, _2, _3, _4, _5));
  }

  // Table
  addAndMakeVisible(m_table = new TableListBox("VPOT mapping", this));
  m_table->setHeaderHeight(22);
  m_table->setColour(ListBox::outlineColourId, Colours::grey);
  m_table->setOutlineThickness(1);

  m_table->getHeader().addColumn("VPOT", CSTP_COL_NR, 60, 60, 60,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Parameter", CSTP_COL_PARAM, 180, 120, 300,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Name", CSTP_COL_NAME, 70, 60, 80,
                                 TableHeaderComponent::notResizable);

  m_table->setMultipleSelectionEnabled(false);
  // 8px margin around the table so this dialog matches the BindingTable's
  // look (which sits inside ChannelStripComponent with the same margin).
  const int m = 8;
  setSize(400 + 2 * m, 24 + ChannelStripMap::kNumVPOTs * 24 + 8 + 2 * m);

  startTimer(100);
}

ChannelStripParamEditor::~ChannelStripParamEditor() {
  stopTimer();
  // Persist all strips to the single channelstrips.xml before tearing down
  // (the mapping editor edits a strip's VPOT->param bindings).
  if (m_pMode)
    m_pMode->saveStripsToFile();
  if (m_pWatcher) {
    if (m_paramChangedConnectionId >= 0)
      m_pWatcher->disconnectParamChange(m_paramChangedConnectionId);
    safe_delete(m_pWatcher);
  }
  // Only remove the plugin if we added a temporary instance for learning.
  // If we reused an existing instance on the track, leave it untouched.
  if (m_ownsTempPlugin && m_pTrack && m_fxSlot >= 0)
    TrackFX_Delete(m_pTrack, m_fxSlot);
  s_dialogOpen = false;
}

void ChannelStripParamEditor::resized() {
  if (m_table) {
    const int m = 8;
    m_table->setBounds(m, m, getWidth() - 2 * m, getHeight() - 2 * m);
  }
}

int ChannelStripParamEditor::getNumRows() {
  return ChannelStripMap::kNumVPOTs;
}

void ChannelStripParamEditor::paintRowBackground(Graphics &g, int, int w,
                                                  int h, bool sel) {
  g.fillAll(sel ? Colours::lightblue : Colours::white);
}

void ChannelStripParamEditor::paintCell(Graphics &g, int row, int col,
                                         int w, int h, bool) {
  if (col != CSTP_COL_NR) return;
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  String n = (row < 8) ? String(row + 1) : ("Shift " + String(row - 7));
  g.drawText(n, 2, 1, w - 4, h, Justification::centred, true);
}

Component *ChannelStripParamEditor::refreshComponentForCell(
    int row, int col, bool, Component *existing) {
  switch (col) {
  case CSTP_COL_PARAM: {
    auto *c = (VpotParamCombo *)existing;
    if (!c) c = new VpotParamCombo(*this);
    c->setRowAndColumn(row, col);
    return c;
  }
  case CSTP_COL_NAME: {
    auto *c = (VpotNameLabel *)existing;
    if (!c) c = new VpotNameLabel(*this);
    c->setRowAndColumn(row, col);
    return c;
  }
  default:
    jassert(!existing);
    return nullptr;
  }
}

void ChannelStripParamEditor::selectedRowsChanged(int lastRow) {
  m_learnVPOT = lastRow;
}

void ChannelStripParamEditor::timerCallback() {
  if (m_pWatcher)
    m_pWatcher->frame(timeGetTime());
}

void ChannelStripParamEditor::onParamChanged(MediaTrack *, int /*slot*/,
                                              int paramNr, double /*val*/,
                                              String /*valStr*/) {
  if (m_learnVPOT < 0 || m_learnVPOT >= ChannelStripMap::kNumVPOTs)
    return;
  setVPOTParam(m_learnVPOT, paramNr);
  // one-shot: deselect so the next knob turn goes to the next selected VPOT
  m_table->deselectAllRows();
  m_learnVPOT = -1;
}

void ChannelStripParamEditor::setVPOTParam(int vpot, int paramIdx) {
  if (!m_strip || vpot < 0 || vpot >= ChannelStripMap::kNumVPOTs)
    return;
  m_strip->setParamForVPOT(vpot, paramIdx);
  // auto-fill the name from the param name if empty
  if (paramIdx >= 0 && paramIdx < m_paramNames.size() &&
      m_strip->getVPOTName(vpot).isEmpty())
    m_strip->setVPOTName(vpot, m_paramNames[paramIdx].substring(0, 6));
  // refresh AFTER setting both param and name so the name label picks it up
  m_table->updateContent();
  if (m_pMode)
    m_pMode->bindingChanged();
}

// ===== open =====

void ChannelStripParamEditor::notifyBindingChanged() {
  if (m_pMode) m_pMode->bindingChanged();
}

void ChannelStripParamEditor::open(ChannelStripMode *pMode, int stripIndex) {
  if (!pMode || s_dialogOpen)
    return; // a mapping dialog is already open
  MediaTrack *tr = pMode->selectedTrack();
  ChannelStripMap *strip = pMode->getStrip(stripIndex);
  if (!strip || !strip->isAssigned()) {
    AlertWindow::showMessageBox(
        AlertWindow::WarningIcon, String("No plugin assigned"),
        String("This strip has no plugin assigned, so there is no VPOT "
               "mapping to edit."));
    return;
  }
  if (!tr) {
    AlertWindow::showMessageBox(
        AlertWindow::WarningIcon, String("No track selected"),
        String("Select exactly one track in REAPER first: the VPOT mapping "
               "editor reads the plugin's parameters from that track."));
    return;
  }

  // Reuse an existing instance of this plugin on the track if one is
  // present (do NOT add a duplicate). Only add a temp instance at the end of
  // the chain when the plugin is not yet on the track.
  int slot = ChannelStripAccess::findSlotByIdent(tr, strip->getFxIdent());
  bool ownsTemp = false;
  if (slot < 0) {
    int chainLen = TrackFX_GetCount(tr);
    int instArg = ChannelStripAccess::instantiateArgFor(ChannelStripMap::LAST,
                                                        chainLen);
    slot = TrackFX_AddByName(tr, strip->getFxIdent().toRawUTF8(), false,
                             instArg);
    if (slot < 0) {
      AlertWindow::showMessageBox(
          AlertWindow::WarningIcon, String("Could not open the plugin"),
          String("REAPER could not instantiate\n") + strip->getFxIdent() +
              String("\non the selected track."));
      return;
    }
    ownsTemp = true;
  }

  TrackFX_Show(tr, slot, 3);

  ChannelStripParamEditor *content =
      new ChannelStripParamEditor(pMode, stripIndex, tr, slot);
  content->m_ownsTempPlugin = ownsTemp;

  static KlinkeLookAndFeel klf;
  content->setLookAndFeel(&klf);

  s_dialogOpen = true;

  DialogWindow::LaunchOptions o;
  o.dialogTitle = "Channel Strip " + String(stripIndex + 1) +
                  String(" - VPOT mapping");
  o.content.setOwned(content);
  o.escapeKeyTriggersCloseButton = true;
  o.useNativeTitleBar = true;
  o.resizable = true;
  o.dialogBackgroundColour = Colours::white;
  // Centre around the component with keyboard focus (normally the main MCU
  // editor window): the default placement can end up off-screen, in which
  // case the dialog is invisible and s_dialogOpen sticks at true (dead
  // Edit-Mapping button).
  Component *focused = Component::getCurrentlyFocusedComponent();
  o.componentToCentreAround = focused ? focused->getTopLevelComponent()
                                      : nullptr;
  o.launchAsync();
  content->enterModalState(true);
}

// ===== VpotParamCombo =====

VpotParamCombo::VpotParamCombo(ChannelStripParamEditor &o)
    : owner(o), m_combo(NULL), row(0), col(0) {
  addAndMakeVisible(m_combo = new ComboBox());
  m_combo->addItem("(none)", 1);
  const StringArray &names = owner.paramNames();
  for (int i = 0; i < names.size(); i++)
    m_combo->addItem(names[i], i + 2);
  m_combo->addListener(this);
  m_combo->setColour(ComboBox::backgroundColourId, Colours::white);
  m_combo->setColour(ComboBox::textColourId, Colours::black);
  m_combo->setColour(ComboBox::outlineColourId, Colours::black);
}

void VpotParamCombo::setRowAndColumn(int r, int c) {
  row = r; col = c;
  ChannelStripMap *strip = owner.getStrip();
  int cur = strip ? strip->getParamForVPOT(row) : -1;
  m_combo->setSelectedId(cur >= 0 ? cur + 2 : 1, dontSendNotification);
}

void VpotParamCombo::comboBoxChanged(ComboBox *) {
  int sel = m_combo->getSelectedId();
  int param = (sel > 1) ? sel - 2 : -1;
  owner.setVPOTParam(row, param);
}

// ===== VpotNameLabel =====

VpotNameLabel::VpotNameLabel(ChannelStripParamEditor &o)
    : owner(o), m_label(NULL), row(0), col(0) {
  addAndMakeVisible(m_label = new Label(String(), String()));
  m_label->setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  m_label->setJustificationType(Justification::centredLeft);
  m_label->setEditable(true, true, false);
  m_label->setColour(Label::backgroundColourId, Colours::white);
  m_label->setColour(Label::outlineColourId, Colours::lightgrey);
  m_label->setColour(TextEditor::textColourId, Colours::black);
  m_label->addListener(this);
}

void VpotNameLabel::setRowAndColumn(int r, int c) {
  row = r; col = c;
  ChannelStripMap *strip = owner.getStrip();
  m_label->setText(strip ? strip->getVPOTName(row) : String(),
                   dontSendNotification);
}

void VpotNameLabel::labelTextChanged(Label *l) {
  ChannelStripMap *strip = owner.getStrip();
  if (!strip) return;
  String s = l->getText().substring(0, 6);
  strip->setVPOTName(row, s);
  l->setText(s, dontSendNotification);
  owner.notifyBindingChanged();
}
