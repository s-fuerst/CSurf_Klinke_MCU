/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripBindingTable.h"
#include "ChannelStripMode.h"
#include "ChannelStripParamEditor.h"

ChannelStripBindingTable::ChannelStripBindingTable(ChannelStripMode *pMode)
    : m_pMode(pMode), m_table(NULL) {
  refreshInstalledFX();
  addAndMakeVisible(m_table = new TableListBox("ChannelStrip strips", this));
  m_table->setHeaderHeight(22);
  m_table->setColour(ListBox::outlineColourId, Colours::grey);
  m_table->setOutlineThickness(1);

  m_table->getHeader().addColumn("#", CST_COL_NR, 28, 28, 28,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Plugin", CST_COL_PLUGIN, 230, 120, 400,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Abbrev", CST_COL_ABBREV, 60, 50, 80,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("InsPos", CST_COL_INSPOS, 70, 60, 90,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Parameter", CST_COL_PARAM, 130, 100, 200,
                                 TableHeaderComponent::notResizable);

  m_table->setMultipleSelectionEnabled(false);
  m_table->setAutoSizeMenuOptionShown(false);
  m_table->updateContent();
}

ChannelStripBindingTable::~ChannelStripBindingTable() { deleteAllChildren(); }

void ChannelStripBindingTable::resized() {
  if (m_table) m_table->setBounds(getLocalBounds());
}

int ChannelStripBindingTable::getNumRows() {
  return ChannelStripMode::kNumStrips;
}

void ChannelStripBindingTable::refreshInstalledFX() {
  m_installedFX.clear();
  ChannelStripAccess::getInstalledFX(m_installedFX);
}

ChannelStripMap *ChannelStripBindingTable::stripForRow(int row) {
  if (!m_pMode || row < 0 || row >= ChannelStripMode::kNumStrips)
    return NULL;
  return m_pMode->getStrip(row);
}

void ChannelStripBindingTable::notifyBindingChanged() {
  if (m_pMode) m_pMode->bindingChanged();
}

void ChannelStripBindingTable::paintRowBackground(Graphics &g, int, int w,
                                                  int h, bool sel) {
  g.fillAll(sel ? Colours::lightblue : Colours::white);
  g.setColour(Colours::grey); g.drawRect(0, 0, w, h);
}

void ChannelStripBindingTable::paintCell(Graphics &g, int row, int col,
                                         int w, int h, bool) {
  if (col != CST_COL_NR) return;
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  String n = (row < 8) ? String(row + 1) : ("S" + String(row - 7));
  g.drawText(n, 2, 1, w - 4, h, Justification::centred, true);
}

Component *ChannelStripBindingTable::refreshComponentForCell(
    int row, int col, bool, Component *existing) {
  switch (col) {
  case CST_COL_PLUGIN: {
    auto *c = (CSTPluginCombo *)existing;
    if (!c) c = new CSTPluginCombo(*this);
    c->setRowAndColumn(row, col); return c;
  }
  case CST_COL_ABBREV: {
    auto *c = (CSTAbbrevLabel *)existing;
    if (!c) c = new CSTAbbrevLabel(*this);
    c->setRowAndColumn(row, col); return c;
  }
  case CST_COL_INSPOS: {
    auto *c = (CSTInsPosCombo *)existing;
    if (!c) c = new CSTInsPosCombo(*this);
    c->setRowAndColumn(row, col); return c;
  }
  case CST_COL_PARAM: {
    auto *c = (CSTParamButton *)existing;
    if (!c) c = new CSTParamButton(*this);
    c->setRowAndColumn(row, col); return c;
  }
  default: jassert(!existing); return nullptr;
  }
}

// ===== Plugin combo (autocomplete) =====

CSTPluginCombo::CSTPluginCombo(ChannelStripBindingTable &o)
    : owner(o), m_editor(NULL), row(0), column(0) {
  addAndMakeVisible(m_editor = new TextEditor());
  m_editor->setFont(
      Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  m_editor->setTextToShowWhenEmpty("type plugin name here", Colours::lightgrey);
  m_editor->addListener(this);
}

CSTPluginCombo::~CSTPluginCombo() {
  hidePopup();
  deleteAllChildren();
}

void CSTPluginCombo::setRowAndColumn(int r, int c) {
  row = r; column = c;
  ChannelStripMap *strip = owner.stripForRow(row);
  String name;
  if (strip && strip->isAssigned()) {
    const auto &fx = owner.installedFX();
    for (const auto &f : fx) {
      if (f.ident == strip->getFxIdent()) { name = f.name; break; }
    }
    if (name.isEmpty()) name = strip->getFxIdent();
  }
  m_editor->setText(name, dontSendNotification);
  m_lastValidText = name;
}

void CSTPluginCombo::applyFilter(const String &text) {
  String f = text.trim();
  m_filtered.clear();
  const auto &fx = owner.installedFX();
  for (const auto &item : fx) {
    if (f.isEmpty() || item.name.containsIgnoreCase(f))
      m_filtered.push_back(item);
  }
  if (m_popupList) {
    m_popupList->updateContent();
    int h = jlimit(4, 12, (int)m_filtered.size()) * 22 + 4;
    m_popupList->setSize(m_popupList->getWidth(), h);
  }
}

void CSTPluginCombo::showPopup() {
  if (m_popupList || !m_editor) return;
  applyFilter(m_editor->getText());
  if (m_filtered.empty()) return;
  ListBox *lb = new ListBox();
  lb->setModel(this);
  lb->setRowHeight(22);
  lb->setColour(ListBox::outlineColourId, Colours::grey);
  lb->setOutlineThickness(1);
  int width = jmax(m_editor->getWidth(), 280);
  int h = jlimit(4, 12, (int)m_filtered.size()) * 22 + 4;
  lb->setSize(width, h);
  Component *top = getTopLevelComponent();
  Point<int> rel = m_editor->getScreenPosition() - top->getScreenPosition();
  lb->setTopLeftPosition(rel.x, rel.y + m_editor->getHeight());
  top->addAndMakeVisible(lb);
  lb->toFront(false);
  m_popupList = lb;
}

void CSTPluginCombo::hidePopup() {
  if (m_popupList) delete m_popupList.getComponent();
}

void CSTPluginCombo::pickFiltered(int filteredRow) {
  if (filteredRow < 0 || filteredRow >= (int)m_filtered.size()) return;
  setSelectedByIdent(m_filtered[filteredRow].ident);
  hidePopup();
}

void CSTPluginCombo::setSelectedByIdent(const String &ident) {
  ChannelStripMap *strip = owner.stripForRow(row);
  if (!strip) return;
  if (ident.isEmpty()) {
    strip->setFxIdent(String());
    strip->setFxGUID(String());
    for (int i = 0; i < ChannelStripMap::kNumVPOTs; i++)
      strip->setParamForVPOT(i, -1);
    m_editor->setText(String(), dontSendNotification);
  } else {
    const auto &fx = owner.installedFX();
    for (const auto &f : fx) {
      if (f.ident == ident) {
        strip->setFxIdent(f.ident);
        strip->setFxGUID(String());
        // new plugin -> its parameter indices differ, clear the VPOT map
        for (int i = 0; i < ChannelStripMap::kNumVPOTs; i++)
          strip->setParamForVPOT(i, -1);
        if (strip->getShortName().isEmpty())
          strip->setShortName(f.name.substring(0, 5));
        m_editor->setText(f.name, dontSendNotification);
        break;
      }
    }
  }
  m_lastValidText = m_editor->getText();
  owner.notifyBindingChanged();
}

void CSTPluginCombo::textEditorTextChanged(TextEditor &) {
  if (!m_popupList) showPopup(); else applyFilter(m_editor->getText());
}
void CSTPluginCombo::textEditorFocusLost(TextEditor &) {
  SafePointer<CSTPluginCombo> sp(this);
  MessageManager::callAsync([sp]() {
    if (!sp || !sp->m_popupList) return;
    Component *focused = Component::getCurrentlyFocusedComponent();
    if (focused && (focused == sp->m_popupList ||
                    sp->m_popupList->isParentOf(focused)))
      return;
    sp->hidePopup();
  });
}
void CSTPluginCombo::textEditorReturnKeyPressed(TextEditor &) { pickFiltered(0); }
void CSTPluginCombo::textEditorEscapeKeyPressed(TextEditor &) {
  hidePopup();
  m_editor->setText(m_lastValidText, dontSendNotification);
}
int CSTPluginCombo::getNumRows() { return (int)m_filtered.size(); }
void CSTPluginCombo::paintListBoxItem(int r, Graphics &g, int w, int h, bool sel) {
  if (r < 0 || r >= (int)m_filtered.size()) return;
  g.fillAll(sel ? Colours::lightblue : Colours::white);
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  g.drawText(m_filtered[r].name, 6, 0, w - 8, h, Justification::centredLeft, true);
}
void CSTPluginCombo::listBoxItemClicked(int r, const MouseEvent &) { pickFiltered(r); }

// ===== Abbrev label =====

CSTAbbrevLabel::CSTAbbrevLabel(ChannelStripBindingTable &o)
    : owner(o), m_label(NULL), row(0), column(0) {
  addAndMakeVisible(m_label = new Label(String(), String()));
  m_label->setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  m_label->setJustificationType(Justification::centredLeft);
  m_label->setEditable(true, true, false);
  m_label->setColour(Label::backgroundColourId, Colours::white);
  m_label->setColour(Label::outlineColourId, Colours::lightgrey);
  m_label->setColour(TextEditor::textColourId, Colours::black);
  m_label->addListener(this);
}

void CSTAbbrevLabel::setRowAndColumn(int r, int c) {
  row = r; column = c;
  ChannelStripMap *strip = owner.stripForRow(row);
  m_label->setText(strip ? strip->getShortName() : String(), dontSendNotification);
}

void CSTAbbrevLabel::labelTextChanged(Label *l) {
  ChannelStripMap *strip = owner.stripForRow(row);
  if (!strip) return;
  String s = l->getText().substring(0, 5);
  strip->setShortName(s);
  l->setText(s, dontSendNotification);
  owner.notifyBindingChanged();
}

// ===== InsPos combo =====

CSTInsPosCombo::CSTInsPosCombo(ChannelStripBindingTable &o)
    : owner(o), m_combo(NULL), row(0), column(0) {
  addAndMakeVisible(m_combo = new ComboBox());
  using IP = ChannelStripMap::InsertPos;
  m_combo->addItem(ChannelStripMap::tokenForInsertPos(IP::FIRST), (int)IP::FIRST + 1);
  for (int p = (int)IP::POS2; p <= (int)IP::POS8; p++)
    m_combo->addItem(String(p), p + 1);
  m_combo->addItem(ChannelStripMap::tokenForInsertPos(IP::LAST), (int)IP::LAST + 1);
  m_combo->addListener(this);
  m_combo->setWantsKeyboardFocus(true);
}

void CSTInsPosCombo::setRowAndColumn(int r, int c) {
  row = r; column = c;
  ChannelStripMap *strip = owner.stripForRow(row);
  if (strip) m_combo->setSelectedId((int)strip->getInsertPos() + 1, dontSendNotification);
}

void CSTInsPosCombo::comboBoxChanged(ComboBox *) {
  ChannelStripMap *strip = owner.stripForRow(row);
  if (!strip) return;
  int sel = m_combo->getSelectedId() - 1;
  if (sel >= 0 && sel < (int)ChannelStripMap::INSERT_COUNT)
    strip->setInsertPos((ChannelStripMap::InsertPos)sel);
  owner.notifyBindingChanged();
}

// ===== Parameter button -> VPOT/param sub-editor =====

CSTParamButton::CSTParamButton(ChannelStripBindingTable &o)
    : owner(o), m_button(NULL), row(0), column(0) {
  addAndMakeVisible(m_button = new TextButton("edit..."));
  m_button->addListener(this);
}

void CSTParamButton::setRowAndColumn(int r, int c) {
  row = r; column = c;
  ChannelStripMap *strip = owner.stripForRow(row);
  String label = "edit...";
  if (strip && strip->isAssigned())
    label = String(strip->numBoundVPOTs()) + "/" + String(ChannelStripMap::kNumVPOTs);
  else
    label = "-";
  m_button->setButtonText(label);
}

void CSTParamButton::buttonClicked(Button *) {
  ChannelStripMap *strip = owner.stripForRow(row);
  if (!strip || !strip->isAssigned())
    return;
  ChannelStripParamEditor::open(owner.getMode(), row);
}
