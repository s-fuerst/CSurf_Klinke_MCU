/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PlugCategoryEditor.h"
#include "PlugMode.h"
#include "CCSManager.h"
#include "csurf_mcu.h"
#include "McuDebugLog.h"

static const int PC_ROW_HEIGHT = 22;
static const int PC_MARGIN = 8;
static const int PC_NAME_COLUMN = 86; // ":: VPOT 1" label column
static const int PC_SHORT_COLUMN = 74; // short label column (right side)
static const int PC_GRIP_WIDTH = 16;   // drag handle at the left row edge
static const int PC_LIST_WIDTH = 210;  // compact left (category) list width
static const int PC_DIALOG_WIDTH = 820; // leave more room for plugin names

static const Font &pcFont() {
  static const Font f(
      FontOptions(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  return f;
}

// Drag handle + drop indicator shared by both row types: three dots as the
// grip on the left edge and a 2px line at the insertion position.
static void paintRowChrome(Graphics &g, Component &row, int rowIndex,
                           bool isDropAbove, bool isDropBelow) {
  // grip (drag handle)
  g.setColour(Colours::grey);
  for (int i = 0; i < 3; i++) {
    g.fillRect(4, 5 + i * 5, 2, 2);
    g.fillRect(9, 5 + i * 5, 2, 2);
  }
  juce::ignoreUnused(row, rowIndex);
  // insertion indicator
  g.setColour(Colours::royalblue);
  if (isDropAbove)
    g.fillRect(0, 0, row.getWidth(), 2);
  if (isDropBelow)
    g.fillRect(0, row.getHeight() - 2, row.getWidth(), 2);
}

// ===== right side table cell: the optional 6-character short label =====

class PlugShortLabel : public Component, public Label::Listener {
public:
  explicit PlugShortLabel(PlugCategoryEditor &owner)
      : m_owner(owner), m_row(0), m_label(NULL) {
    addAndMakeVisible(m_label = new Label(String(), String()));
    m_label->setFont(pcFont());
    m_label->setJustificationType(Justification::centredLeft);
    m_label->setEditable(true, true, false);
    m_label->setColour(Label::backgroundColourId, Colours::white);
    m_label->setColour(Label::outlineColourId, Colours::lightgrey);
    m_label->setColour(Label::textColourId, Colours::black);
    m_label->setColour(Label::textWhenEditingColourId, Colours::black);
    m_label->setColour(TextEditor::textColourId, Colours::black);
    m_label->setTooltip(
        String("Label shown on the MCU display (max 6 characters). Empty = "
               "derived from the plugin name."));
    m_label->addListener(this);
  }
  ~PlugShortLabel() override { deleteAllChildren(); }

  void resized() override { m_label->setBoundsInset(BorderSize(2)); }

  void setRow(int row) {
    m_row = row;
    const String stored = m_owner.pluginShortName(row);
    const bool derived = stored.isEmpty();
    // Empty = the hardware derives the label from the plugin name. Show that
    // derived label greyed out so the user sees what the display will show.
    m_label->setText(derived ? m_owner.pluginLabel(row) : stored,
                     dontSendNotification);
    m_label->setColour(Label::textColourId,
                       derived ? Colours::grey : Colours::black);
  }

  void labelTextChanged(Label *l) override {
    String s = l->getText().substring(0, PlugCategoryConfig::kMaxLabelLength);
    l->setText(s, dontSendNotification);
    m_owner.pluginShortNameEdited(m_row, s);
  }

private:
  PlugCategoryEditor &m_owner;
  int m_row;
  Label *m_label;
};

// ===== left list row: grip + VPOT label + 6-character category name =====

class CategoryRow : public Component, public TextEditor::Listener {
public:
  explicit CategoryRow(PlugCategoryEditor &owner)
      : m_owner(owner), m_row(0), m_label(NULL), m_edit(NULL) {
    addAndMakeVisible(m_label = new Label(String(), String()));
    m_label->setFont(pcFont());
    m_label->setJustificationType(Justification::centredLeft);
    m_label->setColour(Label::textColourId, Colours::black);
    m_label->setInterceptsMouseClicks(false, false);

    addAndMakeVisible(m_edit = new TextEditor());
    m_edit->setFont(pcFont());
    m_edit->setInputRestrictions(PlugCategoryConfig::kMaxLabelLength);
    m_edit->setColour(TextEditor::textColourId, Colours::black);
    m_edit->setColour(TextEditor::backgroundColourId, Colours::white);
    m_edit->addListener(this);
    m_edit->addMouseListener(this, true);
  }

  ~CategoryRow() override { deleteAllChildren(); }

  void setRow(int row) {
    m_row = row;
    m_label->setText(PlugCategoryEditor::rowName(row), dontSendNotification);
    m_edit->setText(m_owner.categoryName(row), dontSendNotification);
  }

  void resized() override {
    auto b = getLocalBounds();
    b.removeFromLeft(PC_GRIP_WIDTH);
    m_label->setBounds(b.removeFromLeft(PC_NAME_COLUMN - PC_GRIP_WIDTH));
    m_edit->setBounds(b.reduced(1));
  }

  void paint(Graphics &g) override {
    g.fillAll(m_owner.getCategory() == m_row ? Colours::lightblue
                                             : Colours::white);
    const int drop = m_owner.dropIndex(PlugCategoryEditor::DRAG_CATEGORY);
    paintRowChrome(g, *this, m_row,
                   m_owner.isDragging(PlugCategoryEditor::DRAG_CATEGORY) &&
                       drop == m_row,
                   m_owner.isDragging(PlugCategoryEditor::DRAG_CATEGORY) &&
                       drop == m_row + 1);
  }

  void mouseDown(const MouseEvent &) override {
    m_owner.selectCategory(m_row);
  }

  void mouseDrag(const MouseEvent &e) override {
    if (e.getDistanceFromDragStart() < 4)
      return;
    // Only the grip / name area starts a reorder: a drag that began inside
    // the name TextEditor is a text selection. Checked by POSITION (in row
    // coordinates) because the row also receives the forwarded child
    // events.
    if (e.getEventRelativeTo(this).getMouseDownPosition().getX() >=
        PC_NAME_COLUMN)
      return;
    m_owner.startRowDrag(PlugCategoryEditor::DRAG_CATEGORY, m_row, this, e);
  }

  void textEditorTextChanged(TextEditor &) override {
    m_owner.selectCategory(m_row);
    m_owner.categoryNameEdited(m_row, m_edit->getText());
  }

private:
  PlugCategoryEditor &m_owner;
  int m_row;
  Label *m_label;
  TextEditor *m_edit;
};

// ===== right list row: grip + VPOT label + plugin picker + short label =====

class PluginRow : public Component {
public:
  explicit PluginRow(PlugCategoryEditor &owner)
      : m_owner(owner), m_row(0), m_label(NULL), m_combo(NULL),
        m_short(NULL) {
    addAndMakeVisible(m_label = new Label(String(), String()));
    m_label->setFont(pcFont());
    m_label->setJustificationType(Justification::centredLeft);
    m_label->setColour(Label::textColourId, Colours::black);
    m_label->setInterceptsMouseClicks(false, false);

    addAndMakeVisible(m_combo = new PluginComboField(owner));
    addAndMakeVisible(m_short = new PlugShortLabel(owner));
  }

  ~PluginRow() override { deleteAllChildren(); }

  void setRow(int row) {
    m_row = row;
    m_label->setText(PlugCategoryEditor::rowName(row), dontSendNotification);
    m_combo->setRowAndColumn(row, 0, m_owner.pluginText(row));
    m_short->setRow(row);
    resized();
  }

  void resized() override {
    auto b = getLocalBounds();
    b.removeFromLeft(PC_GRIP_WIDTH);
    m_label->setBounds(b.removeFromLeft(PC_NAME_COLUMN - PC_GRIP_WIDTH));
    m_short->setBounds(b.removeFromRight(PC_SHORT_COLUMN).reduced(1));
    m_combo->setBounds(b.reduced(1));
  }

  void paint(Graphics &g) override {
    g.fillAll(Colours::white);
    const int drop = m_owner.dropIndex(PlugCategoryEditor::DRAG_PLUGIN);
    paintRowChrome(g, *this, m_row,
                   m_owner.isDragging(PlugCategoryEditor::DRAG_PLUGIN) &&
                       drop == m_row,
                   m_owner.isDragging(PlugCategoryEditor::DRAG_PLUGIN) &&
                       drop == m_row + 1);
  }

  void mouseDrag(const MouseEvent &e) override {
    if (e.getDistanceFromDragStart() < 4)
      return;
    // Grip / name area only: a drag inside the plugin picker or the short
    // name field belongs to those editors.
    if (e.getEventRelativeTo(this).getMouseDownPosition().getX() >=
        PC_NAME_COLUMN)
      return;
    m_owner.startRowDrag(PlugCategoryEditor::DRAG_PLUGIN, m_row, this, e);
  }

private:
  PlugCategoryEditor &m_owner;
  int m_row;
  Label *m_label;
  PluginComboField *m_combo;
  PlugShortLabel *m_short;
};

// ===== the list: ListBox + model + drag & drop target =====

class ReorderListBox : public ListBox,
                       public ListBoxModel,
                       public DragAndDropTarget {
public:
  ReorderListBox(PlugCategoryEditor &owner, PlugCategoryEditor::EDragKind kind,
                 const String &componentName)
      : m_owner(owner), m_kind(kind) {
    setName(componentName);
    setModel(this);
    setRowHeight(PC_ROW_HEIGHT);
    setColour(ListBox::outlineColourId, Colours::grey);
    setOutlineThickness(1);
    setColour(ListBox::backgroundColourId, Colours::white);
    setMultipleSelectionEnabled(false);
  }
  ~ReorderListBox() override { setModel(nullptr); }

  // force the row components to be re-created from the current data
  // (ListBox::updateContent() alone does not re-run refreshComponentForRow)
  void resetRows() {
    setModel(nullptr);
    setModel(this);
    repaint();
  }

  // --- ListBoxModel ---
  int getNumRows() override { return PlugCategoryConfig::kNumCategories; }

  void paintListBoxItem(int row, Graphics &g, int w, int h,
                        bool selected) override {
    g.fillAll(selected ? Colours::lightblue : Colours::white);
    juce::ignoreUnused(row, w, h);
  }

  Component *refreshComponentForRow(int row, bool selected,
                                    Component *existing) override {
    juce::ignoreUnused(selected);
    if (m_kind == PlugCategoryEditor::DRAG_CATEGORY) {
      auto *c = (CategoryRow *)existing;
      if (!c)
        c = new CategoryRow(m_owner);
      c->setRow(row);
      return c;
    }
    auto *c = (PluginRow *)existing;
    if (!c)
      c = new PluginRow(m_owner);
    c->setRow(row);
    return c;
  }

  // --- DragAndDropTarget ---
  bool isInterestedInDragSource(const SourceDetails &details) override {
    return dragSourceKind(details) == m_kind;
  }
  void itemDragEnter(const SourceDetails &details) override {
    itemDragMove(details);
  }
  void itemDragMove(const SourceDetails &details) override {
    m_owner.dragMoved(m_kind, insertionIndexAt(details));
  }
  void itemDragExit(const SourceDetails &) override {
    m_owner.dragExited(m_kind);
  }
  void itemDropped(const SourceDetails &details) override {
    m_owner.dropAt(m_kind, insertionIndexAt(details));
  }

private:
  static PlugCategoryEditor::EDragKind dragSourceKind(
      const SourceDetails &details) {
    const String d = details.description.toString();
    if (d == "category")
      return PlugCategoryEditor::DRAG_CATEGORY;
    if (d == "plugin")
      return PlugCategoryEditor::DRAG_PLUGIN;
    return PlugCategoryEditor::DRAG_NONE;
  }

  int insertionIndexAt(const SourceDetails &details) {
    return getInsertionIndexForPosition(details.localPosition.getX(),
                                        details.localPosition.getY());
  }

  PlugCategoryEditor &m_owner;
  PlugCategoryEditor::EDragKind m_kind;
};

// ===== PlugCategoryEditor =====

PlugCategoryEditor::PlugCategoryEditor(PlugMode *pMode)
    : m_pMode(pMode), m_pConfig(PlugCategoryConfig::instance()),
      m_unitCombo(NULL), m_headerCategory(NULL), m_headerPlugin(NULL),
      m_categoryList(NULL), m_pluginList(NULL), m_unit(0), m_category(0),
      m_dragKind(DRAG_NONE), m_dragRow(-1), m_dropIndexCategory(-1),
      m_dropIndexPlugin(-1) {
  // Newly installed plugins must appear in the picker.
  InstalledFxCache::refresh();
  m_installedFX = InstalledFxCache::get();

  addAndMakeVisible(m_unitCombo = new ComboBox());
  int nUnits = m_pMode && m_pMode->getCCSManager() &&
                       m_pMode->getCCSManager()->getMCU()
                   ? m_pMode->getCCSManager()->getMCU()->numUnits()
                   : 1;
  for (int u = 0; u < PlugCategoryConfig::kNumUnits; u++) {
    String text = "Unit " + String(u + 1);
    if (u >= nUnits)
      text += " (inactive)";
    m_unitCombo->addItem(text, u + 1);
  }
  m_unitCombo->setSelectedId(1, dontSendNotification);
  m_unitCombo->addListener(this);

  addAndMakeVisible(m_headerCategory = new Label(String(), String()));
  m_headerCategory->setFont(pcFont().boldened());
  m_headerCategory->setColour(Label::textColourId, Colours::black);
  m_headerCategory->setText("Category", dontSendNotification);

  addAndMakeVisible(m_headerPlugin = new Label(String(), String()));
  m_headerPlugin->setFont(pcFont().boldened());
  m_headerPlugin->setColour(Label::textColourId, Colours::black);
  m_headerPlugin->setText("Plugin name", dontSendNotification);

  addAndMakeVisible(
      m_categoryList = new ReorderListBox(*this, DRAG_CATEGORY, "Categories"));
  addAndMakeVisible(
      m_pluginList = new ReorderListBox(*this, DRAG_PLUGIN, "Plugin slots"));

  setSize(PC_DIALOG_WIDTH, 480);
  reloadCategoryList();
  reloadPluginList();
}

PlugCategoryEditor::~PlugCategoryEditor() { deleteAllChildren(); }

String PlugCategoryEditor::rowName(int row) {
  if (row < 0)
    return String();
  if (row < 8)
    return "VPOT " + String(row + 1);
  return "Shift " + String(row - 7);
}

void PlugCategoryEditor::resized() {
  const int m = PC_MARGIN;
  const int leftWidth = PC_LIST_WIDTH;
  const int rightX = m + leftWidth + m;
  const int rightWidth = getWidth() - rightX - m;
  if (m_unitCombo)
    m_unitCombo->setBounds(m, m, 260, 24);
  int headerY = m + 30;
  if (m_headerCategory) {
    m_headerCategory->setBounds(m, headerY, leftWidth, 20);
    m_headerPlugin->setBounds(rightX, headerY, rightWidth, 20);
  }
  int top = headerY + 22;
  int h = getHeight() - top - m;
  if (m_categoryList)
    m_categoryList->setBounds(m, top, leftWidth, h);
  if (m_pluginList)
    m_pluginList->setBounds(rightX, top, rightWidth, h);
}

void PlugCategoryEditor::paint(Graphics &g) { g.fillAll(Colours::white); }

void PlugCategoryEditor::comboBoxChanged(ComboBox *combo) {
  if (combo != m_unitCombo)
    return;
  setUnit(m_unitCombo->getSelectedId() - 1);
}

// --- state ---

String PlugCategoryEditor::categoryName(int row) const {
  return m_pConfig->categoryName(m_unit, row);
}

String PlugCategoryEditor::pluginText(int row) const {
  const PlugCategoryConfig::PluginEntry &e =
      m_pConfig->plugin(m_unit, m_category, row);
  if (!e.isAssigned())
    return String();
  String name = InstalledFxCache::nameForIdent(e.ident);
  return name.isNotEmpty() ? name : e.ident;
}

String PlugCategoryEditor::pluginShortName(int row) const {
  return m_pConfig->plugin(m_unit, m_category, row).shortName;
}

String PlugCategoryEditor::pluginLabel(int row) const {
  return m_pConfig->pluginLabel(m_unit, m_category, row);
}

void PlugCategoryEditor::setUnit(int unit) {
  if (unit < 0 || unit >= PlugCategoryConfig::kNumUnits || unit == m_unit)
    return;
  m_unit = unit;
  m_category = 0;
  MCU_LOG("PCE unit=%d", m_unit + 1);
  reloadCategoryList();
  reloadPluginList();
}

void PlugCategoryEditor::setCategory(int category) {
  if (category < 0 || category >= PlugCategoryConfig::kNumCategories ||
      category == m_category)
    return;
  m_category = category;
  reloadPluginList();
}

void PlugCategoryEditor::selectCategory(int row) {
  if (row < 0 || row >= PlugCategoryConfig::kNumCategories)
    return;
  if (m_categoryList)
    m_categoryList->selectRow(row, true, true);
  setCategory(row);
  if (m_categoryList)
    m_categoryList->repaint();
}

void PlugCategoryEditor::reloadCategoryList() {
  if (!m_categoryList)
    return;
  m_categoryList->resetRows();
  m_categoryList->selectRow(m_category, true, true);
  m_categoryList->repaint();
}

void PlugCategoryEditor::reloadPluginList() {
  if (!m_pluginList)
    return;
  m_pluginList->resetRows();
}

void PlugCategoryEditor::repaintLists() {
  if (m_categoryList)
    m_categoryList->repaint();
  if (m_pluginList)
    m_pluginList->repaint();
}

// --- reordering (drag & drop) ---

void PlugCategoryEditor::startRowDrag(EDragKind kind, int row,
                                      Component *source,
                                      const MouseEvent &e) {
  if (!source || kind == DRAG_NONE || m_dragKind != DRAG_NONE)
    return;
  if (row < 0 || row >= PlugCategoryConfig::kNumCategories)
    return;
  m_dragKind = kind;
  m_dragRow = row;
  m_dropIndexCategory = (kind == DRAG_CATEGORY) ? row : -1;
  m_dropIndexPlugin = (kind == DRAG_PLUGIN) ? row : -1;
  MCU_LOG("PCE drag start kind=%d row=%d", (int)kind, row);
  // Visual drag image: the row as it looks right now, slightly transparent
  // (like JUCE's own list/tree dragging).
  Image dragImage = source->createComponentSnapshot(
                                source->getLocalBounds(), true, 1.0f)
                                .convertedToFormat(Image::ARGB);
  dragImage.multiplyAllAlphas(0.6f);
  // Anchor the image at the grabbed point: JUCE positions the drag image so
  // that the point `-imageOffset` of the image is under the mouse. The image
  // is the row's local bounds, so the grab point is the mouse-down position
  // inside the row — that keeps the grip (left edge) at the cursor instead
  // of centring the whole row on it.
  Point<int> imageOffset = -e.getEventRelativeTo(source).getMouseDownPosition();
  startDragging(kind == DRAG_CATEGORY ? var("category") : var("plugin"), source,
                ScaledImage(dragImage), false, &imageOffset);
  repaintLists();
}

int PlugCategoryEditor::dropIndex(EDragKind kind) const {
  if (kind == DRAG_CATEGORY)
    return m_dropIndexCategory;
  if (kind == DRAG_PLUGIN)
    return m_dropIndexPlugin;
  return -1;
}

void PlugCategoryEditor::dragMoved(EDragKind kind, int insertionIndex) {
  int &stored = (kind == DRAG_CATEGORY) ? m_dropIndexCategory
                                        : m_dropIndexPlugin;
  int clamped = jlimit(0, PlugCategoryConfig::kNumCategories, insertionIndex);
  if (stored == clamped)
    return;
  stored = clamped;
  repaintLists();
}

void PlugCategoryEditor::dragExited(EDragKind kind) {
  if (kind != m_dragKind)
    return;
  int &stored = (kind == DRAG_CATEGORY) ? m_dropIndexCategory
                                        : m_dropIndexPlugin;
  if (stored < 0)
    return;
  stored = -1;
  repaintLists();
}

void PlugCategoryEditor::dropAt(EDragKind kind, int insertionIndex) {
  const int from = m_dragRow;
  const EDragKind draggedKind = m_dragKind;
  // Clear the drag state FIRST: the move rebuilds the rows, and a stale
  // insertion indicator must not be painted on the new ones.
  m_dragKind = DRAG_NONE;
  m_dragRow = -1;
  m_dropIndexCategory = -1;
  m_dropIndexPlugin = -1;
  if (from < 0 || kind != draggedKind)
    return;
  MCU_LOG("PCE drop kind=%d from=%d insert=%d", (int)kind, from,
          insertionIndex);
  if (kind == DRAG_CATEGORY)
    moveCategoryRow(from, insertionIndex);
  else
    movePluginRow(from, insertionIndex);
  repaintLists();
}

void PlugCategoryEditor::dragOperationEnded(
    const DragAndDropTarget::SourceDetails &) {
  // Reached for EVERY drag (also a cancelled one, i.e. released outside a
  // list) — clear the state so no indicator is left behind.
  m_dragKind = DRAG_NONE;
  m_dragRow = -1;
  m_dropIndexCategory = -1;
  m_dropIndexPlugin = -1;
  repaintLists();
}

void PlugCategoryEditor::moveCategoryRow(int from, int insertIndex) {
  if (from < 0 || from >= PlugCategoryConfig::kNumCategories)
    return;
  insertIndex = jlimit(0, PlugCategoryConfig::kNumCategories, insertIndex);
  // Insertion index -> array index after removing the dragged entry.
  const int to = insertIndex - (from < insertIndex ? 1 : 0);
  if (to == from)
    return;
  m_pConfig->moveCategory(m_unit, from, to);
  // The moved category (with its plugins) keeps being the selected one.
  m_category = jlimit(0, PlugCategoryConfig::kNumCategories - 1, to);
  reloadCategoryList();
  reloadPluginList();
}

void PlugCategoryEditor::movePluginRow(int from, int insertIndex) {
  if (from < 0 || from >= PlugCategoryConfig::kNumPluginsPerCategory)
    return;
  insertIndex =
      jlimit(0, PlugCategoryConfig::kNumPluginsPerCategory, insertIndex);
  const int to = insertIndex - (from < insertIndex ? 1 : 0);
  if (to == from)
    return;
  m_pConfig->movePlugin(m_unit, m_category, from, to);
  reloadPluginList();
}

// --- edits from the cell components ---

void PlugCategoryEditor::categoryNameEdited(int row, const String &text) {
  m_pConfig->setCategoryName(m_unit, row, text);
}

void PlugCategoryEditor::pluginShortNameEdited(int row, const String &text) {
  const PlugCategoryConfig::PluginEntry &e =
      m_pConfig->plugin(m_unit, m_category, row);
  m_pConfig->setPlugin(m_unit, m_category, row, e.ident, text);
}

void PlugCategoryEditor::pluginPicked(int row, int column,
                                      const String &ident,
                                      const String &displayName) {
  juce::ignoreUnused(column);
  m_pConfig->setPickedPlugin(m_unit, m_category, row, ident, displayName);
}

void PlugCategoryEditor::pluginPickFinished(int row, int column) {
  juce::ignoreUnused(row, column);
  // Rebuild the rows so the Short column shows the value derived from the
  // new plugin (setPickedPlugin() overwrites the old short name).
  reloadPluginList();
}
