/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Row reordering by drag & drop for a TableListBox.
 *
 *   +------------------------------------------------------------+
 *   | VPOT  | Plugin            | Abbrev | ...                   |  <- header
 *   | :: 1  | VST3: Pro-Q 3 ... | ProQ   | ...                   |  <- RowDragGrip
 *   | :: 2  | ...               |        |                       |
 *   +------------------------------------------------------------+
 *
 * The grip cell component (RowDragGrip) is placed in the table's first
 * column; it shows the row number (or whatever label the model passes in)
 * and starts the drag when the user drags it. ReorderTableListBox is at the
 * same time the DragAndDropContainer, the drag source (the grip is only the
 * component the drag starts from) and the drop target: JUCE's drag image is
 * a child of the container and targets are searched inside it, so the table
 * must be the container itself.
 *
 * While a drag is running a blue insertion line is shown at the drop
 * position (a 2px overlay child, no mouse interception). On drop the owner
 * gets onReorder(from, to) with the ALREADY ADJUSTED target index (insert
 * semantics: entries in between shift, nothing is swapped).
 */
#pragma once
#include "JuceHeader.h"
#include <functional>

// (TableListBox already is a SettableTooltipClient — no extra base needed.)
class ReorderTableListBox : public TableListBox,
                            public DragAndDropContainer,
                            public DragAndDropTarget {
public:
  ReorderTableListBox(const String &componentName,
                      TableListBoxModel *model = nullptr);
  ~ReorderTableListBox() override;

  // Called when a row was dropped: from = dragged row, to = target index
  // after removing the dragged row. Owners move their data and rebuild the
  // table (updateContent()/setModel reset).
  std::function<void(int from, int to)> onReorder;

  // Starts dragging `row` (called by the RowDragGrip of that row). `e` is
  // that drag event: its mouse-down position anchors the drag image under
  // the cursor (otherwise JUCE centres the image on the mouse and the row
  // jumps away from the grip the user grabbed).
  void startRowDrag(int row, Component *sourceComponent, const MouseEvent &e);

  // --- DragAndDropTarget ---
  bool isInterestedInDragSource(const SourceDetails &) override;
  void itemDragEnter(const SourceDetails &) override;
  void itemDragMove(const SourceDetails &) override;
  void itemDragExit(const SourceDetails &) override;
  void itemDropped(const SourceDetails &) override;

  // --- DragAndDropContainer: end of every drag (also a cancelled one) ---
  void dragOperationEnded(
      const DragAndDropTarget::SourceDetails &) override;

private:
  int rowCount() const;
  int insertionIndexAt(const SourceDetails &details);
  void updateDropLine(int insertionIndex);
  void hideDropLine();

  // 2px insertion line, drawn on top of the table's viewport.
  class DropLine;
  DropLine *m_pDropLine;
  int m_dragRow;
};

/**
 * First-column cell component of a ReorderTableListBox: renders the row
 * label (e.g. "1".."8" / "Shift 1".."Shift 8") with a drag grip and starts
 * the reorder drag. A click selects the row (so the row-click behaviour of
 * the tables, e.g. arming a Learn row, is unchanged).
 */
class RowDragGrip : public Component, public SettableTooltipClient {
public:
  explicit RowDragGrip(ReorderTableListBox &table);

  void setRow(int row, const String &label);
  void paint(Graphics &g) override;
  void mouseDown(const MouseEvent &) override;
  void mouseDrag(const MouseEvent &e) override;

private:
  ReorderTableListBox &m_table;
  int m_row;
  String m_label;
};
