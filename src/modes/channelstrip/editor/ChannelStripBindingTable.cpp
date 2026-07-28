/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripBindingTable.h"
#include "ChannelStripMap.h"
#include "ChannelStripMode.h"
#include "csurf_mcu.h"

ChannelStripBindingTable::ChannelStripBindingTable(ChannelStripMode *pMode)
    : m_pMode(pMode), m_activeUnit(0), m_table(NULL) {
  refreshInstalledFX();
  addAndMakeVisible(m_table =
                        new TableListBox(String("ChannelStrip bindings"), this));
  m_table->setHeaderHeight(22);
  m_table->setColour(ListBox::outlineColourId, Colours::grey);
  m_table->setOutlineThickness(1);

  m_table->getHeader().addColumn(String("#"), CST_COL_NR, 28, 28, 28,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn(String("Plugin"), CST_COL_PLUGIN, 230, 120, 400,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn(String("Abbrev"), CST_COL_ABBREV, 60, 50, 80,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn(String("InsPos"), CST_COL_INSPOS, 70, 60, 90,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn(String("Parameter"), CST_COL_PARAM, 160, 120, 300,
                                 TableHeaderComponent::notResizable);

  m_table->setMultipleSelectionEnabled(false);
  m_table->setAutoSizeMenuOptionShown(false);
  m_table->updateContent();
  m_table->setAutoSizeMenuOptionShown(false);
}

ChannelStripBindingTable::~ChannelStripBindingTable() { deleteAllChildren(); }

void ChannelStripBindingTable::resized() {
  if (m_table)
    m_table->setBounds(getLocalBounds());
}

void ChannelStripBindingTable::refreshInstalledFX() {
  m_installedFX.clear();
  ChannelStripAccess::getInstalledFX(m_installedFX);
}

void ChannelStripBindingTable::setActiveUnit(int unit) {
  m_activeUnit = unit;
  if (m_table)
    m_table->updateContent();
}

ChannelStripBinding *ChannelStripBindingTable::bindingForRow(int row) {
  if (!m_pMode)
    return NULL;
  ChannelStripMap *map = m_pMode->getMapForUnit(m_activeUnit);
  if (!map || row < 0 || row >= ChannelStripMap::kNumSlots)
    return NULL;
  return map->getSlot(row);
}

void ChannelStripBindingTable::notifyBindingChanged() {
  if (m_pMode)
    m_pMode->bindingChanged();
}

void ChannelStripBindingTable::paintRowBackground(Graphics &g, int /*row*/,
                                                  int width, int height,
                                                  bool rowIsSelected) {
  g.fillAll(rowIsSelected ? Colours::lightblue : Colours::white);
  g.setColour(Colours::grey);
  g.drawRect(0, 0, width, height);
}

void ChannelStripBindingTable::paintCell(Graphics &g, int rowNumber,
                                         int columnId, int width, int height,
                                         bool /*rowIsSelected*/) {
  if (columnId != CST_COL_NR)
    return;
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  // 1..16; mark the Shift half (9..16)
  String n = String(rowNumber + 1);
  g.drawText(n, 2, 1, width - 4, height, Justification::centred, true);
}

Component *ChannelStripBindingTable::refreshComponentForCell(
    int rowNumber, int columnId, bool /*isRowSelected*/,
    Component *existingComponentToUpdate) {
  switch (columnId) {
  case CST_COL_PLUGIN: {
    CSTPluginCombo *c = (CSTPluginCombo *)existingComponentToUpdate;
    if (!c)
      c = new CSTPluginCombo(*this);
    c->setRowAndColumn(rowNumber, columnId);
    return c;
  }
  case CST_COL_ABBREV: {
    CSTAbbrevLabel *c = (CSTAbbrevLabel *)existingComponentToUpdate;
    if (!c)
      c = new CSTAbbrevLabel(*this);
    c->setRowAndColumn(rowNumber, columnId);
    return c;
  }
  case CST_COL_INSPOS: {
    CSTInsPosCombo *c = (CSTInsPosCombo *)existingComponentToUpdate;
    if (!c)
      c = new CSTInsPosCombo(*this);
    c->setRowAndColumn(rowNumber, columnId);
    return c;
  }
  case CST_COL_PARAM: {
    CSTParamButton *c = (CSTParamButton *)existingComponentToUpdate;
    if (!c)
      c = new CSTParamButton(*this);
    c->setRowAndColumn(rowNumber, columnId);
    return c;
  }
  default:
    jassert(existingComponentToUpdate == nullptr);
    return nullptr;
  }
}

//==============================================================================
// Plugin combo

CSTPluginCombo::CSTPluginCombo(ChannelStripBindingTable &owner)
    : m_owner(owner), m_combo(NULL), m_row(0), m_columnId(0) {
  addAndMakeVisible(m_combo = new ComboBox());
  m_combo->addItem(String("(none)"), 1);
  const std::vector<ChannelStripAccess::InstalledFX> &fx = m_owner.installedFX();
  for (size_t i = 0; i < fx.size(); i++)
    m_combo->addItem(fx[i].name, (int)i + 2);
  m_combo->addListener(this);
  m_combo->setWantsKeyboardFocus(true);
}

void CSTPluginCombo::setRowAndColumn(int row, int column) {
  m_row = row;
  m_columnId = column;
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  int sel = 1; // (none)
  if (b && b->isAssigned()) {
    const String &ident = b->getFxIdent();
    const std::vector<ChannelStripAccess::InstalledFX> &fx =
        m_owner.installedFX();
    for (size_t i = 0; i < fx.size(); i++) {
      if (fx[i].ident == ident) {
        sel = (int)i + 2;
        break;
      }
    }
  }
  m_combo->setSelectedId(sel, dontSendNotification);
}

void CSTPluginCombo::comboBoxChanged(ComboBox *) {
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  if (!b)
    return;
  int sel = m_combo->getSelectedId();
  if (sel <= 1) {
    b->setFxIdent(String());
    b->setFxGUID(String());
    b->setParamIndex(-1);
  } else {
    const std::vector<ChannelStripAccess::InstalledFX> &fx =
        m_owner.installedFX();
    int idx = sel - 2;
    if (idx >= 0 && idx < (int)fx.size()) {
      b->setFxIdent(fx[idx].ident);
      b->setFxGUID(String()); // re-resolved on the track at runtime
      b->setParamIndex(-1);
      if (b->getShortName().isEmpty())
        b->setShortName(fx[idx].name.substring(0, 5));
    }
  }
  m_owner.notifyBindingChanged();
}

//==============================================================================
// Abbrev label

CSTAbbrevLabel::CSTAbbrevLabel(ChannelStripBindingTable &owner)
    : m_owner(owner), m_label(NULL), m_row(0), m_columnId(0) {
  addAndMakeVisible(m_label = new Label(String(), String()));
  m_label->setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  m_label->setJustificationType(Justification::centredLeft);
  m_label->setEditable(true, true, false);
  m_label->setColour(Label::backgroundColourId, Colours::white);
  m_label->setColour(Label::outlineColourId, Colours::lightgrey);
  m_label->setColour(TextEditor::textColourId, Colours::black);
  m_label->addListener(this);
}

void CSTAbbrevLabel::setRowAndColumn(int row, int column) {
  m_row = row;
  m_columnId = column;
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  m_label->setText(b ? b->getShortName() : String(), dontSendNotification);
}

void CSTAbbrevLabel::labelTextChanged(Label *l) {
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  if (!b)
    return;
  String s = l->getText().substring(0, 5);
  b->setShortName(s);
  l->setText(s, dontSendNotification);
  m_owner.notifyBindingChanged();
}

//==============================================================================
// Insert-position combo

CSTInsPosCombo::CSTInsPosCombo(ChannelStripBindingTable &owner)
    : m_owner(owner), m_combo(NULL), m_row(0), m_columnId(0) {
  addAndMakeVisible(m_combo = new ComboBox());
  using IP = ChannelStripBinding::InsertPos;
  m_combo->addItem(ChannelStripBinding::tokenForInsertPos(IP::FIRST),
                   (int)IP::FIRST + 1);
  for (int p = (int)IP::POS2; p <= (int)IP::POS8; p++)
    m_combo->addItem(String(p), p + 1);
  m_combo->addItem(ChannelStripBinding::tokenForInsertPos(IP::LAST),
                   (int)IP::LAST + 1);
  m_combo->addListener(this);
  m_combo->setWantsKeyboardFocus(true);
}

void CSTInsPosCombo::setRowAndColumn(int row, int column) {
  m_row = row;
  m_columnId = column;
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  if (b)
    m_combo->setSelectedId((int)b->getInsertPos() + 1, dontSendNotification);
}

void CSTInsPosCombo::comboBoxChanged(ComboBox *) {
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  if (!b)
    return;
  int sel = m_combo->getSelectedId() - 1;
  if (sel >= 0 && sel < (int)ChannelStripBinding::INSERT_COUNT)
    b->setInsertPos((ChannelStripBinding::InsertPos)sel);
  m_owner.notifyBindingChanged();
}

//==============================================================================
// Parameter picker (button -> popup menu of the plugin's parameters)

CSTParamButton::CSTParamButton(ChannelStripBindingTable &owner)
    : m_owner(owner), m_button(NULL), m_row(0), m_columnId(0) {
  addAndMakeVisible(m_button = new TextButton(String("pick...")));
  m_button->addListener(this);
}

void CSTParamButton::setRowAndColumn(int row, int column) {
  m_row = row;
  m_columnId = column;
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  String label = String("pick...");
  if (b && b->isAssigned()) {
    if (b->getParamIndex() >= 0)
      label = "#" + String(b->getParamIndex());
    else
      label = String("(none)");
  }
  m_button->setButtonText(label);
}

void CSTParamButton::buttonClicked(Button *) {
  ChannelStripBinding *b = m_owner.bindingForRow(m_row);
  if (!b || !b->isAssigned())
    return;
  ChannelStripMode *mode = m_owner.getMode();
  MediaTrack *tr = mode ? mode->getSelectedTrack() : NULL;
  if (!tr)
    return;
  ChannelStripAccess *access = mode->getAccess();
  access->trackChanged(tr);
  // ensure the plugin is on the track so we can enumerate its parameters
  int fxSlot = access->resolveBinding(tr, *b);
  if (fxSlot < 0)
    fxSlot = access->addPlugin(*b);
  if (fxSlot < 0) {
    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                     String("Channel Strip"),
                                     String("Could not add the plugin to the track."));
    return;
  }
  int n = access->getNumParams(tr, fxSlot);
  if (n <= 0)
    return;
  PopupMenu menu;
  for (int i = 0; i < n; i++)
    menu.addItem(i + 1, access->getParamName(tr, fxSlot, i));
  int result = menu.show(); // modal (JUCE_MODAL_LOOPS_PERMITTED=1)
  if (result > 0) {
    int param = result - 1;
    b->setParamIndex(param);
    if (b->getShortName().isEmpty())
      b->setShortName(access->getParamName(tr, fxSlot, param).substring(0, 5));
    m_owner.notifyBindingChanged();
  }
}
