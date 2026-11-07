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
  addAndMakeVisible(m_table = new ReorderTableListBox("ChannelStrip strips",
                                                      this));
  // Drag & drop reordering: rows move (insert semantics), the mode persists
  // the new slot order and refreshes the hardware.
  m_table->onReorder = [this](int from, int to) { moveStripRow(from, to); };
  m_table->setTooltip(
      String("Drag a row at its number column to move the strip to another "
             "VPOT slot."));
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
  m_stripFilesExist =
      !ChannelStripMode::listXmlFiles(ChannelStripMode::stripFilesDir())
           .isEmpty();
  m_table->updateContent();
  resetCells();
}

void ChannelStripBindingTable::refreshFileButtonStates() {
  if (!m_table)
    return;
  m_stripFilesExist =
      !ChannelStripMode::listXmlFiles(ChannelStripMode::stripFilesDir())
           .isEmpty();
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
  // Every column is a cell component: the number column is the
  // RowDragGrip (number + drag grip), the rest is filled by the cells in
  // refreshComponentForCell(). Nothing left to paint here.
  juce::ignoreUnused(g, row, col, w, h);
}

Component *ChannelStripBindingTable::refreshComponentForCell(
    int row, int col, bool, Component *existing) {
  switch (col) {
  case CST_COL_NR: {
    // Row number + drag grip (the whole cell starts the reorder drag).
    auto *c = (RowDragGrip *)existing;
    if (!c) c = new RowDragGrip(*m_table);
    c->setRow(row, rowLabel(row));
    return c;
  }
  case CST_COL_PLUGIN: {
    auto *c = (PluginComboField *)existing;
    if (!c) c = new PluginComboField(*this);
    c->setRowAndColumn(row, col, pluginDisplayText(row));
    return c;
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

// ===== drag & drop reordering =====

String ChannelStripBindingTable::rowLabel(int row) {
  if (row < 0)
    return String();
  return (row < 8) ? String(row + 1) : ("Shift " + String(row - 7));
}

void ChannelStripBindingTable::moveStripRow(int from, int to) {
  if (!m_pMode)
    return;
  m_pMode->moveStrip(from, to); // persists + fires bindingChanged()
  // The table is index-based: rebuild every cell from the new slot order.
  resetCells();
}

// ===== PluginComboFieldOwner =====

String ChannelStripBindingTable::pluginDisplayText(int row) {
  ChannelStripMap *strip = stripForRow(row);
  if (!strip || !strip->isAssigned())
    return String();
  const String ident = strip->getFxIdent();
  for (const auto &f : m_installedFX)
    if (f.ident == ident)
      return f.name;
  return ident; // plugin not installed: show the raw ident
}

void ChannelStripBindingTable::pluginPicked(int row, int column,
                                            const String &ident,
                                            const String &displayName) {
  juce::ignoreUnused(column);
  ChannelStripMap *strip = stripForRow(row);
  if (!strip)
    return;
  if (ident.isEmpty()) {
    // clear the whole strip: plugin, abbrev, VPOT mapping and per-VPOT names
    strip->setFxIdent(String());
    strip->setShortName(String());
    for (int i = 0; i < ChannelStripMap::kNumVPOTs; i++) {
      strip->setParamForVPOT(i, -1);
      strip->setVPOTName(i, String());
    }
    return;
  }
  strip->setFxIdent(ident);
  // new plugin -> its parameter indices differ, clear the VPOT map
  for (int i = 0; i < ChannelStripMap::kNumVPOTs; i++)
    strip->setParamForVPOT(i, -1);
  // refresh shortName on every plugin change: strip "TYPE:" prefix and
  // " (Manufacturer)" (a user-customised name is overwritten when the
  // plugin itself changes)
  strip->setShortName(InstalledFxCache::shortLabel(displayName).substring(0, 5));
}

void ChannelStripBindingTable::pluginPickFinished(int row, int column) {
  juce::ignoreUnused(row, column);
  // Non-destructive refresh FIRST: bindingChanged updates the MCU
  // display/LEDs (its editor refresh is itself deferred, so it does not
  // touch the table here). resetCells() must be LAST: it recreates the
  // table's cell components and therefore DESTROYS the combo that called
  // this, so nothing may be touched afterwards.
  notifyBindingChanged();
  resetCells();
}

// ===== Abbrev label =====

CSTAbbrevLabel::CSTAbbrevLabel(ChannelStripBindingTable &o)
    : owner(o), m_label(NULL), row(0), column(0) {
  addAndMakeVisible(m_label = new Label(String(), String()));
  m_label->setFont(Font(FontOptions(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain)));
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
  m_button->setTooltip(String("Save: store this strip FX in a file of the "
                              "Strip FXs folder (name dialog, default = abbrev).\n"
                              "Load: pick one of the existing strip FX files and "
                              "replace this strip FX with it (no confirmation).\n"
                              "Clear: reset this strip FX (plugin and VPOT mapping) "
                              "to unassigned."));
  m_button->addListener(this);
}

void CSTStripFileButton::setRowAndColumn(int r, int c) {
  row = r; column = c;
  ChannelStripMap *strip = owner.stripForRow(row);
  const bool assigned = strip && strip->isAssigned();
  // Save/Clear need an assigned strip; Load needs an existing strip file
  const bool enabled =
      (m_action == LOAD) ? owner.stripFilesExist() : assigned;
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
                        [this, mode, stripRow, dir](const String &name) {
                          const File file =
                              ChannelStripMode::stripFileForName(name, dir);
                          if (!mode->saveStripToUserFile(stripRow, file)) {
                            MCU_LOG("%s",
                                    ("CSM save strip " +
                                     String(stripRow + 1) + " to " +
                                     file.getFullPathName() + " FAILED")
                                        .toRawUTF8());
                            return false;
                          }
                          // a new file may now be loadable — grey out / un-grey
                          // the Load buttons. Deferred: the refresh recreates
                          // the cell components, including THIS button (the
                          // same use-after-free hazard as CLEAR).
                          ChannelStripBindingTable &table = owner;
                          MessageManager::callAsync([&table]() {
                            table.refreshFileButtonStates();
                          });
                          return true;
                        },
                        [dir](const String &name) {
                          const File file = dir.getChildFile(name);
                          if (!file.deleteFile()) {
                            MCU_LOG("%s",
                                    ("CSM delete strip file " +
                                     file.getFullPathName() + " FAILED")
                                        .toRawUTF8());
                            return false;
                          }
                          MCU_LOG("%s",
                                  ("CSM deleted strip file " +
                                   file.getFullPathName()).toRawUTF8());
                          return true;
                        });
    return;
  }
  const StringArray files = ChannelStripMode::listXmlFiles(dir);
  openStripLoadDialog("Load channel strip map", files,
                      [mode, stripRow, dir](const String &name) {
                        const File file = dir.getChildFile(name);
                        if (!mode->loadStripFromUserFile(stripRow, file)) {
                          MCU_LOG("%s",
                                  ("CSM load strip " +
                                   String(stripRow + 1) + " from " +
                                   file.getFullPathName() + " FAILED")
                                      .toRawUTF8());
                          return false;
                        }
                        return true;
                      },
                      [this, dir](const String &name) {
                        const File file = dir.getChildFile(name);
                        if (!file.deleteFile()) {
                          MCU_LOG("%s",
                                  ("CSM delete strip file " +
                                   file.getFullPathName() + " FAILED")
                                      .toRawUTF8());
                          return false;
                        }
                        MCU_LOG("%s",
                                ("CSM deleted strip file " +
                                 file.getFullPathName()).toRawUTF8());
                        // one file gone — the Load buttons may need to grey
                        // out. Deferred: the refresh recreates the cell
                        // components, including THIS button (the same
                        // use-after-free hazard as CLEAR).
                        ChannelStripBindingTable &table = owner;
                        MessageManager::callAsync([&table]() {
                          table.refreshFileButtonStates();
                        });
                        return true;
                      });
}
