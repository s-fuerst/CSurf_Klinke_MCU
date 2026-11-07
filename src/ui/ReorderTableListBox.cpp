/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ReorderTableListBox.h"
#include "McuDebugLog.h"

// ===== the insertion line =====

class ReorderTableListBox::DropLine : public Component {
public:
  DropLine() {
    setInterceptsMouseClicks(false, false);
    setAlwaysOnTop(true);
  }
  void paint(Graphics &g) override {
    g.fillAll(Colours::royalblue);
  }
};

// ===== ReorderTableListBox =====

ReorderTableListBox::ReorderTableListBox(const String &componentName,
                                         TableListBoxModel *model)
    : TableListBox(componentName, model), m_pDropLine(NULL), m_dragRow(-1) {
  addChildComponent(m_pDropLine = new DropLine());
}

ReorderTableListBox::~ReorderTableListBox() {
  // Only our own child: the header/viewport belong to TableListBox/ListBox
  // (deleting them here would leave their internal pointers dangling).
  delete m_pDropLine;
  m_pDropLine = NULL;
}

int ReorderTableListBox::rowCount() const {
  auto *model = getListBoxModel();
  return model ? model->getNumRows() : 0;
}

void ReorderTableListBox::startRowDrag(int row, Component *sourceComponent,
                                       const MouseEvent &e) {
  if (!sourceComponent || m_dragRow >= 0)
    return;
  const int rows = rowCount();
  if (row < 0 || row >= rows)
    return;
  m_dragRow = row;
  MCU_LOG("RTB drag start row=%d of %d", row, rows);
  // Drag image: the row as it looks now (cells included), slightly
  // transparent like JUCE's own list/tree dragging.
  const juce::Rectangle<int> rowRect = getRowPosition(row, true);
  Image img = createComponentSnapshot(rowRect, true, 1.0f)
                  .convertedToFormat(Image::ARGB);
  img.multiplyAllAlphas(0.6f);
  // Anchor the image at the grabbed point (see PlugCategoryEditor): JUCE puts
  // the image point `-imageOffset` under the mouse, so passing the mouse-down
  // position INSIDE the row image keeps the grip (left edge) at the cursor
  // instead of centring the whole row on it.
  const Point<int> downInTable = e.getEventRelativeTo(this).getMouseDownPosition();
  const Point<int> grabPoint = downInTable - rowRect.getPosition();
  Point<int> imageOffset = -grabPoint;
  startDragging(var("row"), sourceComponent, ScaledImage(img), false,
                &imageOffset);
}

bool ReorderTableListBox::isInterestedInDragSource(
    const SourceDetails &details) {
  return details.description.toString() == "row";
}

int ReorderTableListBox::insertionIndexAt(const SourceDetails &details) {
  return getInsertionIndexForPosition(details.localPosition.getX(),
                                      details.localPosition.getY());
}

void ReorderTableListBox::itemDragEnter(const SourceDetails &details) {
  itemDragMove(details);
}

void ReorderTableListBox::itemDragMove(const SourceDetails &details) {
  if (m_dragRow < 0)
    return;
  updateDropLine(insertionIndexAt(details));
}

void ReorderTableListBox::itemDragExit(const SourceDetails &) {
  hideDropLine();
}

void ReorderTableListBox::itemDropped(const SourceDetails &details) {
  const int from = m_dragRow;
  const int rows = rowCount();
  const int insertIndex = jlimit(0, rows, insertionIndexAt(details));
  hideDropLine();
  m_dragRow = -1;
  if (from < 0 || !onReorder)
    return;
  // Insert index -> array index after removing the dragged row.
  const int to = insertIndex - (from < insertIndex ? 1 : 0);
  MCU_LOG("RTB drop from=%d insert=%d -> to=%d", from, insertIndex, to);
  if (to == from)
    return;
  onReorder(from, to);
}

void ReorderTableListBox::dragOperationEnded(
    const DragAndDropTarget::SourceDetails &) {
  // Reached for EVERY drag (also one released outside the table).
  hideDropLine();
  m_dragRow = -1;
}

void ReorderTableListBox::updateDropLine(int insertionIndex) {
  if (!m_pDropLine)
    return;
  const int rows = rowCount();
  if (rows <= 0) {
    hideDropLine();
    return;
  }
  int y;
  if (insertionIndex >= rows) {
    const juce::Rectangle<int> last = getRowPosition(rows - 1, true);
    y = last.getBottom() - 1;
  } else {
    const int row = jmax(0, insertionIndex);
    y = getRowPosition(row, true).getY();
  }
  m_pDropLine->setBounds(0, y, getWidth(), 2);
  m_pDropLine->setVisible(true);
  m_pDropLine->toFront(false);
}

void ReorderTableListBox::hideDropLine() {
  if (m_pDropLine)
    m_pDropLine->setVisible(false);
}

// ===== RowDragGrip =====

RowDragGrip::RowDragGrip(ReorderTableListBox &table)
    : m_table(table), m_row(0) {
  setTooltip(String("Drag to reorder the row, click to select it."));
}

void RowDragGrip::setRow(int row, const String &label) {
  m_row = row;
  m_label = label;
  repaint();
}

void RowDragGrip::paint(Graphics &g) {
  // drag grip: two columns of three dots
  g.setColour(Colours::grey);
  for (int i = 0; i < 3; i++) {
    g.fillRect(3, 6 + i * 4, 2, 2);
    g.fillRect(7, 6 + i * 4, 2, 2);
  }
  g.setColour(Colours::black);
  g.setFont(Font(FontOptions(Font::getDefaultSansSerifFontName(), 13.0f,
                             Font::plain)));
  g.drawText(m_label, 14, 0, getWidth() - 16, getHeight(),
             Justification::centredLeft, true);
}

void RowDragGrip::mouseDown(const MouseEvent &) {
  // Keep the row-click behaviour of the tables (the param editor arms its
  // Learn row from selectedRowsChanged).
  m_table.selectRow(m_row, false, true);
}

void RowDragGrip::mouseDrag(const MouseEvent &e) {
  if (e.getDistanceFromDragStart() < 4)
    return;
  m_table.startRowDrag(m_row, this, e);
}
