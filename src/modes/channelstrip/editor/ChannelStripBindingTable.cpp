/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripBindingTable.h"
#include "ChannelStripMode.h"
#include "ChannelStripParamEditor.h"
#include "ChannelStripFileDialogs.h"
#include "McuDebugLog.h"

ChannelStripBindingTable::ChannelStripBindingTable(ChannelStripMode *pMode)
    : m_pMode(pMode), m_table(NULL) {
  refreshInstalledFX();
  addAndMakeVisible(m_table = new TableListBox("ChannelStrip strips", this));
  m_table->setHeaderHeight(22);
  m_table->setColour(ListBox::outlineColourId, Colours::grey);
  m_table->setOutlineThickness(1);

  m_table->getHeader().addColumn("VPOT", CST_COL_NR, 60, 60, 60,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Plugin", CST_COL_PLUGIN, 230, 120, 400,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Abbrev", CST_COL_ABBREV, 60, 50, 80,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Insert Position", CST_COL_INSPOS, 110, 90, 130,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Edit Mapping", CST_COL_PARAM, 130, 100, 200,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Save", CST_COL_SAVE, 60, 50, 80,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Load", CST_COL_LOAD, 60, 50, 80,
                                 TableHeaderComponent::notResizable);
  m_table->getHeader().addColumn("Clear", CST_COL_CLEAR, 60, 50, 80,
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

void ChannelStripBindingTable::updateEverything() {
  if (!m_table)
    return;
  m_table->updateContent();
  resetCells();
}

void ChannelStripBindingTable::resetCells() {
  if (!m_table)
    return;
  // JUCE's TableListBox::updateContent() does NOT re-run
  // refreshComponentForCell when the row count is unchanged, so custom cell
  // components (plugin names etc.) keep stale state when the editor is
  // re-opened. Resetting the model forces a full recreation of all cell
  // components, re-running setRowAndColumn with current data.
  m_table->setModel(nullptr);
  m_table->setModel(this);
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
  // No grid lines between rows — keep the look identical to the
  // ChannelStripParamEditor table (which also draws no row separators).
}

void ChannelStripBindingTable::paintCell(Graphics &g, int row, int col,
                                         int w, int h, bool) {
  if (col != CST_COL_NR) return;
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  String n = (row < 8) ? String(row + 1) : ("Shift " + String(row - 7));
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
  case CST_COL_SAVE:
  case CST_COL_LOAD:
  case CST_COL_CLEAR: {
    auto *c = (CSTStripFileButton *)existing;
    if (!c)
      c = new CSTStripFileButton(*this,
                                 (CSTStripFileButton::Action)(col - CST_COL_SAVE));
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
  // Explicit colours: inside the TableListBox the window's
  // KlinkeLookAndFeel defaults are not resolved for this editor, and the
  // default text colour is white -> the picked plugin name was invisible
  // (bug C). Same pattern as every other editable cell in the codebase.
  m_editor->setColour(TextEditor::textColourId, Colours::black);
  m_editor->setColour(TextEditor::backgroundColourId, Colours::white);
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
  // Defer layout + repaint to after the table has positioned this cell.
  // During refreshComponentForCell the cell bounds may still be 0, so sizing
  // the TextEditor synchronously leaves it blank — most visible when the
  // editor is re-opened (the cell components are recreated then).
  SafePointer<CSTPluginCombo> sp(this);
  MessageManager::callAsync([sp]() {
    if (!sp) return;
    sp->resized();
    if (sp->m_editor) sp->m_editor->repaint();
  });
}

// row 0 of the popup is always a sentinel to clear the strip
static const ChannelStripAccess::InstalledFX &clearSentinel() {
  static const ChannelStripAccess::InstalledFX s = {
      String("empty (no plugin)"), String()};
  return s;
}

void CSTPluginCombo::applyFilter(const String &text) {
  String f = text.trim();
  m_filtered.clear();
  m_filtered.push_back(clearSentinel());
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
  // the empty ident of the sentinel (row 0) clears the strip
  setSelectedByIdent(m_filtered[filteredRow].ident);
  hidePopup();
}

void CSTPluginCombo::setSelectedByIdent(const String &ident) {
  ChannelStripMap *strip = owner.stripForRow(row);
  if (!strip) return;
  if (ident.isEmpty()) {
    // clear the whole strip: plugin, abbrev, VPOT mapping and per-VPOT names
    strip->setFxIdent(String());
    strip->setShortName(String());
    for (int i = 0; i < ChannelStripMap::kNumVPOTs; i++) {
      strip->setParamForVPOT(i, -1);
      strip->setVPOTName(i, String());
    }
    m_editor->setText(String(), dontSendNotification);
  } else {
    const auto &fx = owner.installedFX();
    for (const auto &f : fx) {
      if (f.ident == ident) {
        strip->setFxIdent(f.ident);
        // new plugin -> its parameter indices differ, clear the VPOT map
        for (int i = 0; i < ChannelStripMap::kNumVPOTs; i++)
          strip->setParamForVPOT(i, -1);
        // refresh shortName on every plugin change: strip "TYPE:" prefix
        // and " (Manufacturer)" (a user-customised name is overwritten when
        // the plugin itself changes)
        {
          String base = f.name;
          int colon = base.indexOfChar(':');
          if (colon >= 0)
            base = base.substring(colon + 1).trimStart();
          int paren = base.indexOfChar('(');
          if (paren > 0)
            base = base.substring(0, paren).trimEnd();
          strip->setShortName(base.substring(0, 5));
        }
        m_editor->setText(f.name, dontSendNotification);
        break;
      }
    }
  }
  m_lastValidText = m_editor->getText();
  // refresh the other cells of this row (Abbrev label, Edit button) which
  // do not update themselves when the strip changes
  owner.resetCells();
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
void CSTPluginCombo::textEditorReturnKeyPressed(TextEditor &) {
  // pick the first real match, never the clear sentinel
  for (int i = 1; i < (int)m_filtered.size(); i++)
    if (!m_filtered[i].ident.isEmpty()) {
      pickFiltered(i);
      return;
    }
  pickFiltered(0); // only the sentinel -> clear
}
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
    m_combo->addItem(String(p + 1), p + 1);
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
  addAndMakeVisible(m_button = new TextButton("edit"));
  // Make it unmistakably a button: light-grey fill + dark border so users see
  // it opens the VPOT->param mapping dialog.
  m_button->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_button->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_button->setColour(TextButton::textColourOffId, Colours::black);
  m_button->setColour(ComboBox::outlineColourId, Colours::darkgrey);
  m_button->addListener(this);
}

void CSTParamButton::setRowAndColumn(int r, int c) {
  row = r; column = c;
  ChannelStripMap *strip = owner.stripForRow(row);
  String label;
  if (strip && strip->isAssigned())
    label = "Edit " + String(strip->numBoundVPOTs()) + "/" +
            String(ChannelStripMap::kNumVPOTs);
  else
    label = "-";
  m_button->setButtonText(label);
}

void CSTParamButton::buttonClicked(Button *) {
  ChannelStripMap *strip = owner.stripForRow(row);
  if (!strip)
    return;
  // open() shows a hint dialog when the strip is unassigned
  ChannelStripParamEditor::open(owner.getMode(), row);
}

// ===== Save / Load / Clear buttons (single-strip user files) =====

CSTStripFileButton::CSTStripFileButton(ChannelStripBindingTable &o,
                                       Action action)
    : owner(o), m_button(NULL), row(0), column(0), m_action(action) {
  const char *label = (action == SAVE) ? "Save" : (action == LOAD ? "Load"
                                                                  : "Clear");
  addAndMakeVisible(m_button = new TextButton(label));
  m_button->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_button->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_button->setColour(TextButton::textColourOffId, Colours::black);
  m_button->setColour(ComboBox::outlineColourId, Colours::darkgrey);
  m_button->setTooltip(String("Save: store this strip in a file of the Strips "
                              "folder (name dialog, default = abbrev).\n"
                              "Load: pick one of the existing strip files and "
                              "replace this strip with it (no confirmation).\n"
                              "Clear: reset this strip (plugin and VPOT mapping) to "
                              "unassigned."));
  m_button->addListener(this);
}

void CSTStripFileButton::setRowAndColumn(int r, int c) {
  row = r; column = c;
  ChannelStripMap *strip = owner.stripForRow(row);
  const bool assigned = strip && strip->isAssigned();
  // Loading into an empty slot is valid; Save/Clear need an assigned strip
  const bool enabled = (m_action == LOAD) || assigned;
  const char *label = (m_action == SAVE) ? "Save"
                                         : (m_action == LOAD ? "Load" : "Clear");
  m_button->setEnabled(enabled);
  m_button->setButtonText(enabled ? label : "-");
}

void CSTStripFileButton::buttonClicked(Button *) {
  ChannelStripMode *mode = owner.getMode();
  if (!mode)
    return;
  if (m_action == CLEAR) {
    // Deferred to the next message: notifyBindingChanged() -> bindingChanged()
    // -> updateEverything() -> resetCells() destroys all cell components —
    // including THIS button while its click handler is still on the stack
    // (use-after-free crash).
    ChannelStripBindingTable &table = owner;
    const int stripRow = row;
    MessageManager::callAsync([&table, stripRow]() {
      ChannelStripMap *strip = table.stripForRow(stripRow);
      if (!strip)
        return;
      // clear the whole strip: plugin, abbrev, VPOT mapping and per-VPOT names
      strip->initEmpty();
      table.notifyBindingChanged(); // refreshes the cells via bindingChanged()
    });
    return;
  }
  // single strips live in the Strips/ folder (save and load alike)
  const File dir = ChannelStripMode::stripFilesDir();
  const int stripRow = row;
  if (m_action == SAVE) {
    // name dialog (PlugMap-style): default = the strip's abbrev, plus the
    // list of existing strip files (click pre-fills the name)
    openStripSaveDialog("Save channel strip map",
                        mode->stripDefaultName(stripRow),
                        ChannelStripMode::listXmlFiles(dir),
                        [mode, stripRow, dir](const String &name) {
                          const File file =
                              ChannelStripMode::stripFileForName(name, dir);
                          if (!mode->saveStripToUserFile(stripRow, file)) {
                            MCU_LOG("CSM save strip " + String(stripRow + 1) +
                                    " to " + file.getFullPathName() + " FAILED");
                            return false;
                          }
                          return true;
                        });
    return;
  }
  const StringArray files = ChannelStripMode::listXmlFiles(dir);
  const String note = files.isEmpty()
                          ? ("No channel strip files found in " +
                             dir.getFullPathName())
                          : String();
  openStripLoadDialog("Load channel strip map", files, note,
                      [mode, stripRow, dir, files](int index) {
                        const File file = dir.getChildFile(files[index]);
                        if (!mode->loadStripFromUserFile(stripRow, file)) {
                          MCU_LOG("CSM load strip " + String(stripRow + 1) +
                                  " from " + file.getFullPathName() +
                                  " FAILED");
                          return false;
                        }
                        return true;
                      });
}
