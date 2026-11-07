/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * On-screen editor for the per-unit plugin categories used by the PlugMode
 * "insert FX" command. Opened from the hardware with CTRL+ALT+VPOT4 while
 * PlugMode is active (also reachable through PlugMode::openCategoryEditor()).
 *
 *   +-----------------------------------------------------------------------+
 *   | Unit: [Unit 1 v]                                                      |
 *   +--------------------------+--------------------------------------------+
 *   | Category (max 6)         | Plugins of the selected category           |
 *   | :: VPOT 1   [Drums    ]  | :: VPOT 1   [VST3: Pro-Q 3 ...]  [ProQ ]   |
 *   | :: VPOT 2   [Bass     ]  | :: VPOT 2   [                 ]  [      ]   |
 *   | ...                      | ...                                        |
 *   | :: Shift 1  [Guitar   ]  | :: Shift 1  [                 ]  [      ]   |
 *   +--------------------------+--------------------------------------------+
 *
 * Every row is one 6-character VPOT field: rows 0-7 are the unshifted VPOTs,
 * rows 8-15 the SHIFT+VPOT positions. Selecting a category row on the left
 * fills the right side with that category's 16 plugin slots.
 *
 * REORDERING: both lists can be reordered with drag & drop (grab a row at
 * the "::" grip on its left edge and drop it at the wanted position). The
 * entries in between move up/down (insert semantics), the dragged entry is
 * never swapped. Everything is written through to
 * <MCU config>/PluginCategories.xml immediately.
 */
#pragma once
#include "JuceHeader.h"
#include "InstalledFx.h"
#include "PluginComboField.h"
#include "PlugCategoryConfig.h"

class PlugMode;
// Both lists are ReorderListBox instances (defined in the .cpp): a ListBox
// whose rows are custom components and which is the drag & drop target for
// the reordering.
class ReorderListBox;

class PlugCategoryEditor : public Component,
                           public DragAndDropContainer,
                           public ComboBox::Listener,
                           public PluginComboFieldOwner {
public:
  PlugCategoryEditor(PlugMode *pMode);
  ~PlugCategoryEditor() override;

  void resized() override;
  void paint(Graphics &g) override;
  void comboBoxChanged(ComboBox *) override; // unit selection

  // Label shown in the row/V-Pot description column ("VPOT 1" .. "Shift 8").
  static String rowName(int row);

  // --- reordering drag & drop (only inside this editor) ---
  enum EDragKind { DRAG_NONE = 0, DRAG_CATEGORY, DRAG_PLUGIN };

  // Called by a row component when the user drags it at the grip. `e` is
  // that drag event: its mouse-down position anchors the drag image under
  // the cursor (otherwise JUCE centres the image on the mouse and the row
  // jumps away from the grip the user grabbed).
  void startRowDrag(EDragKind kind, int row, Component *source,
                    const MouseEvent &e);
  bool isDragging(EDragKind kind) const { return m_dragKind == kind; }
  // Insertion index (0..16) the running drag would drop at; -1 = none.
  int dropIndex(EDragKind kind) const;
  // Called by the list the drag currently hovers over.
  void dragMoved(EDragKind kind, int insertionIndex);
  void dragExited(EDragKind kind);
  void dropAt(EDragKind kind, int insertionIndex);
  // DragAndDropContainer: end of every drag (also a cancelled one).
  void dragOperationEnded(
      const DragAndDropTarget::SourceDetails &) override;

  // --- edits coming from the cell components ---
  void selectCategory(int row);
  void categoryNameEdited(int row, const String &text);
  void pluginShortNameEdited(int row, const String &text);

  // --- PluginComboFieldOwner (right side plugin cells) ---
  const InstalledFxList &installedFxList() const override {
    return m_installedFX;
  }
  void pluginPicked(int row, int column, const String &ident,
                    const String &displayName) override;
  void pluginPickFinished(int row, int column) override;

  // --- data accessors used by the row/cell components ---
  int getUnit() const { return m_unit; }
  int getCategory() const { return m_category; }
  String categoryName(int row) const;
  String pluginText(int row) const;
  // The user's short label ("" when not set).
  String pluginShortName(int row) const;
  // The label the hardware shows: the short label, else the derived plugin
  // name. Used as the grey placeholder of an empty Short cell.
  String pluginLabel(int row) const;
  PlugCategoryConfig *config() { return m_pConfig; }

private:
  void setUnit(int unit);
  void setCategory(int category);
  void reloadCategoryList();
  void reloadPluginList();
  void moveCategoryRow(int from, int insertIndex);
  void movePluginRow(int from, int insertIndex);
  void repaintLists();

  PlugMode *m_pMode;
  PlugCategoryConfig *m_pConfig;
  ComboBox *m_unitCombo;
  Label *m_headerCategory;
  Label *m_headerPlugin;
  ReorderListBox *m_categoryList;
  ReorderListBox *m_pluginList;
  InstalledFxList m_installedFX;

  int m_unit;
  int m_category;

  // drag & drop reordering state
  EDragKind m_dragKind;
  int m_dragRow;
  int m_dropIndexCategory;
  int m_dropIndexPlugin;
};
