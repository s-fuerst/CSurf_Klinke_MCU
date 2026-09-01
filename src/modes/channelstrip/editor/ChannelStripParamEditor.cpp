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
#include "McuDebugLog.h"
#include <boost/bind.hpp>

bool ChannelStripParamEditor::s_dialogOpen = false;

// Learn debounce: one knob movement in the floating FX window emits MANY
// param-change events, so the row must not advance between them. The next
// VPOT row is selected ~1 s after the last learn event (10 ticks at the
// editor's 100 ms timer).
static const int kLearnAdvanceTicks = 10;

// ===== ChannelStripParamEditor =====

ChannelStripParamEditor::ChannelStripParamEditor(ChannelStripMode *pMode,
                                                  int stripIndex,
                                                  MediaTrack *tr, int fxSlot)
    : m_pMode(pMode), m_stripIndex(stripIndex), m_pTrack(tr),
      m_fxSlot(fxSlot), m_strip(NULL), m_table(NULL),
      m_pWatcher(NULL), m_paramChangedConnectionId(-1), m_learnVPOT(-1),
      m_lastLearnParam(-1), m_learnTicksRemaining(0), m_learnButton(NULL),
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
    // The Part C event feed (CSURF_EXT_SETFXPARAM) only reaches PlugMode's
    // watcher (CSurf_MCU::Extended() -> PlugMode::onHostParamChanged), so
    // this editor's watcher uses the poll fallback instead. The poll is
    // active only while this dialog is open (100 ms timer, one plugin).
    m_pWatcher->setParamFeedFromEvents(false);
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
  m_table->getHeader().addColumn("Clear", CSTP_COL_CLEAR, 60, 60, 60,
                                 TableHeaderComponent::notResizable);

  m_table->setMultipleSelectionEnabled(false);

  // Learn toggle: arms the automatic learn (default ON). With Learn OFF no
  // row is highlighted and knob movements in the FX window are ignored.
  addAndMakeVisible(m_learnButton = new ToggleButton(String("Learn")));
  m_learnButton->setTooltip(String(
      "When ON, turning a parameter in the floating FX window assigns it\n"
      "to the highlighted (red) VPOT row and then advances to the next."));
  m_learnButton->setToggleState(true, dontSendNotification);
  m_learnButton->addListener(this);

  // Auto-arm learn: pre-select row 0 so a knob movement in the floating FX
  // window immediately assigns the first VPOT (no manual row click needed).
  // updateContent() FIRST: TableListBox's ListBox base is constructed with
  // totalItems=0 (the model is stored without setModel), so a bare
  // selectRow() would silently no-op (JUCE guards isPositiveAndBelow(row,
  // totalItems)) and even run the deselectAllRows fallback. After
  // updateContent() totalItems=16 and selectRow(0) sticks.
  m_table->updateContent();
  m_learnVPOT = 0;
  m_table->selectRow(0, false, true);
  MCU_LOG("CSTPE ctor: strip=%d slot=%d numParams=%d learnVPOT=%d",
          m_stripIndex, fxSlot, numParams, m_learnVPOT);
  // 8px margin around the table so this dialog matches the BindingTable's
  // look (which sits inside ChannelStripComponent with the same margin).
  const int m = 8;
  // 36 = Learn button row, 24 = table header, 8 = bottom margin.
  setSize(400 + 2 * m, 36 + 24 + ChannelStripMap::kNumVPOTs * 24 + 8 + m);

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
  const int m = 8;
  if (m_learnButton)
    m_learnButton->setBounds(m, m, 60, 22);
  if (m_table) {
    const int top = 36; // room for the Learn button row
    m_table->setBounds(m, top, getWidth() - 2 * m, getHeight() - top - m);
  }
}

int ChannelStripParamEditor::getNumRows() {
  return ChannelStripMap::kNumVPOTs;
}

void ChannelStripParamEditor::paintRowBackground(Graphics &g, int, int w,
                                                  int h, bool sel) {
  const bool learn = m_learnButton && m_learnButton->getToggleState();
  g.fillAll(sel ? (learn ? Colour(255, 0, 0) : Colours::lightblue)
                : Colours::white);
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
  case CSTP_COL_CLEAR: {
    auto *c = (VpotClearButton *)existing;
    if (!c) c = new VpotClearButton(*this);
    c->setRowAndColumn(row, col);
    return c;
  }
  default:
    jassert(!existing);
    return nullptr;
  }
}

void ChannelStripParamEditor::selectedRowsChanged(int lastRow) {
  MCU_LOG("CSTPE selectedRowsChanged row=%d", lastRow);
  m_learnVPOT = lastRow;
  // A row click (or the programmatic advance) starts a fresh learn gesture
  // at that row: the next knob movement must not advance past it, and any
  // pending debounce is cancelled.
  m_lastLearnParam = -1;
  m_learnTicksRemaining = 0;
}

void ChannelStripParamEditor::timerCallback() {
  if (m_pWatcher)
    m_pWatcher->frame(timeGetTime());
  // Learn debounce: ~1 s after the last learn event (a gesture ended),
  // advance the selection so the next gesture lands on the next VPOT.
  if (m_learnTicksRemaining > 0 && --m_learnTicksRemaining == 0) {
    m_lastLearnParam = -1; // gesture boundary: next event is a new gesture
    const int next = (m_learnVPOT + 1) % ChannelStripMap::kNumVPOTs;
    m_learnVPOT = next;
    m_table->selectRow(next, false, true);
  }
}

// ===== Button::Listener (Learn toggle) =====

void ChannelStripParamEditor::buttonClicked(Button *b) {
  if (b != m_learnButton)
    return;
  if (m_learnButton->getToggleState()) {
    // Learn ON: arm row 0 as a fresh gesture.
    m_lastLearnParam = -1;
    m_learnTicksRemaining = 0;
    m_table->updateContent();
    m_learnVPOT = 0;
    m_table->selectRow(0, false, true);
  } else {
    // Learn OFF: no row highlighted, no assignment, pending advance
    // cancelled. deselectAllRows() fires selectedRowsChanged(-1), which
    // resets the learn state; updateContent() re-runs setRowAndColumn so
    // the combos lose the red learn colour.
    m_table->deselectAllRows();
    m_table->updateContent();
  }
}

void ChannelStripParamEditor::onParamChanged(MediaTrack *, int /*slot*/,
                                              int paramNr, double /*val*/,
                                              String /*valStr*/) {
  MCU_LOG("CSTPE onParamChanged paramNr=%d learnVPOT=%d lastParam=%d",
          paramNr, m_learnVPOT, m_lastLearnParam);
  if (!m_learnButton || !m_learnButton->getToggleState())
    return; // Learn OFF: knob movements are ignored
  if (m_learnVPOT < 0 || m_learnVPOT >= ChannelStripMap::kNumVPOTs)
    return;
  if (paramNr == m_lastLearnParam) {
    // Same gesture continues (one knob movement emits many values): keep
    // the row, only extend the advance debounce.
    m_learnTicksRemaining = kLearnAdvanceTicks;
    return;
  }
  // New gesture: advance to the next row first — unless the debounce
  // already advanced it (m_lastLearnParam == -1 marks a fresh row after a
  // ~1 s pause). Wraps around after the last row.
  if (m_lastLearnParam != -1) {
    const int next = (m_learnVPOT + 1) % ChannelStripMap::kNumVPOTs;
    m_learnVPOT = next;
    m_table->selectRow(next, false, true);
  }
  setVPOTParam(m_learnVPOT, paramNr);
  m_lastLearnParam = paramNr;
  m_learnTicksRemaining = kLearnAdvanceTicks;
}

void ChannelStripParamEditor::setVPOTParam(int vpot, int paramIdx) {
  MCU_LOG("CSTPE setVPOTParam vpot=%d param=%d", vpot, paramIdx);
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

bool ChannelStripParamEditor::isLearnTarget(int row) const {
  return m_learnButton && m_learnButton->getToggleState() &&
         row == m_learnVPOT;
}

void ChannelStripParamEditor::clearVPOT(int vpot) {
  MCU_LOG("CSTPE clearVPOT vpot=%d", vpot);
  if (!m_strip || vpot < 0 || vpot >= ChannelStripMap::kNumVPOTs)
    return;
  m_strip->setParamForVPOT(vpot, -1);
  m_strip->setVPOTName(vpot, String());
  m_table->updateContent();
  if (m_pMode)
    m_pMode->bindingChanged();
}

// ===== open =====

// DialogWindow that dismisses itself on close so the window (and its owned
// content) is actually destroyed and the ChannelStripParamEditor destructor
// runs (persist strips, drop the temp plugin, clear s_dialogOpen). A plain
// DefaultDialogWindow from LaunchOptions would only hide itself, leaving a
// stuck modal that blocks other windows. See the comment in open().
class ChannelStripParamWindow : public DialogWindow {
public:
  ChannelStripParamWindow(const String &title, Component *content)
      : DialogWindow(title, Colours::white, true, true) {
    setContentOwned(content, true);
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    setAlwaysOnTop(true);
  }
  void closeButtonPressed() override { exitModalState(0); }
  bool escapeKeyPressed() override {
    closeButtonPressed();
    return true;
  }
};

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
  MCU_LOG("CSTPE open: strip=%d slot=%d ownsTemp=%d", stripIndex, slot,
          ownsTemp);

  ChannelStripParamEditor *content =
      new ChannelStripParamEditor(pMode, stripIndex, tr, slot);
  content->m_ownsTempPlugin = ownsTemp;

  static KlinkeLookAndFeel klf;
  content->setLookAndFeel(&klf);

  s_dialogOpen = true;

  // Dismiss the dialog properly on close (native title-bar X or Escape). A
  // plain DefaultDialogWindow from LaunchOptions only hides itself and leaves
  // its modal state active, so the hidden modal window would keep blocking
  // every other window (Windows alert tone on title-bar clicks) and
  // s_dialogOpen would never reset (dead "edit mapping" button). Exiting the
  // modal state lets the deleteWhenDismissed window destroy itself, which
  // deletes the owned content and runs its destructor (persist strips, drop
  // the temp plugin, clear s_dialogOpen).
  //
  // NOTE: only the dialog WINDOW may be modal. Calling enterModalState on the
  // CONTENT (as an earlier version did) registers a second, redundant modal
  // component that never gets dismissed -> stuck modal, blocks other windows.
  auto *window = new ChannelStripParamWindow(
      "Channel Strip " + String(stripIndex + 1) + " - VPOT mapping", content);
  // Centre around the component with keyboard focus (normally the main MCU
  // editor window): the default placement can end up off-screen, in which
  // case the dialog is invisible and s_dialogOpen sticks at true (dead
  // Edit-Mapping button).
  Component *focused = Component::getCurrentlyFocusedComponent();
  if (Component *around = focused ? focused->getTopLevelComponent() : nullptr)
    window->centreAroundComponent(around, window->getWidth(),
                                  window->getHeight());
  window->enterModalState(true, nullptr, true); // self-destroying on dismiss
  window->setVisible(true);
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
  // Background is pinned (white, or the learn red for the armed row); text
  // black. The BORDER stays the theme default on purpose — same look as the
  // InsPos combo in the main channel-strip editor (ChannelStripBindingTable).
  m_combo->setColour(ComboBox::backgroundColourId, Colours::white);
  m_combo->setColour(ComboBox::textColourId, Colours::black);
}

void VpotParamCombo::setRowAndColumn(int r, int c) {
  row = r; col = c;
  ChannelStripMap *strip = owner.getStrip();
  int cur = strip ? strip->getParamForVPOT(row) : -1;
  m_combo->setSelectedId(cur >= 0 ? cur + 2 : 1, dontSendNotification);
  // PlugMode learn red (Colour(255,0,0)) on the armed learn row; white
  // otherwise (also when Learn is OFF — no row is highlighted then).
  m_combo->setColour(ComboBox::backgroundColourId,
                     owner.isLearnTarget(row) ? Colour(255, 0, 0)
                                              : Colours::white);
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

// ===== VpotClearButton =====

VpotClearButton::VpotClearButton(ChannelStripParamEditor &o)
    : owner(o), m_button(NULL), row(0), col(0) {
  addAndMakeVisible(m_button = new TextButton(String("Clear")));
  // Same look as the Save/Load/Clear buttons in the main channel-strip
  // editor (ChannelStripBindingTable): light-grey fill, black text, dark
  // border via ComboBox::outlineColourId.
  m_button->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_button->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_button->setColour(TextButton::textColourOffId, Colours::black);
  m_button->setColour(ComboBox::outlineColourId, Colours::darkgrey);
  m_button->setTooltip(String("Clear this VPOT mapping"));
  m_button->addListener(this);
}

void VpotClearButton::setRowAndColumn(int r, int c) {
  row = r; col = c;
}

void VpotClearButton::buttonClicked(Button *) { owner.clearVPOT(row); }
