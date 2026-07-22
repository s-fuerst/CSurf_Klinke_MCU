/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "MultiDisplay.h"
#include "DisplayHandler.h"
#include "HardwareUnit.h"

MultiDisplay::MultiDisplay(DisplayHandler *pDH, int numRows)
    : Display(pDH, numRows) {}

MultiDisplay::~MultiDisplay() {}

void MultiDisplay::addChild(Display *child) {
  if (child)
    m_children.push_back(child);
}

void MultiDisplay::changeText(int row, int pos, const char *text, int pad,
                              bool centered) {
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->changeText(row, pos, text, pad, centered);
  (void)row;
}

void MultiDisplay::changeTextFullLine(int row, const char *text, bool centered) {
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->changeTextFullLine(row, text, centered);
}

void MultiDisplay::changeField(int row, int field, const char *text,
                               bool centered) {
  // Route by global field number: field 1..8 → unit 0, 9..16 → unit 1, etc.
  // Fields outside 1..numChildren()*8 are no-ops.
  int numStrips = (int)m_children.size() * 8;
  if (field < 1 || field > numStrips)
    return;

  int unitIndex = (field - 1) / 8;
  int localField = (field - 1) % 8 + 1;

  if (unitIndex < (int)m_children.size())
    m_children[unitIndex]->changeField(row, localField, text, centered);
}

void MultiDisplay::broadcastField(int row, int field, const char *text,
                                  bool centered) {
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->changeField(row, field, text, centered);
}

Display *MultiDisplay::mainChild() {
  for (size_t i = 0; i < m_children.size(); i++) {
    DisplayHandler *dh = m_children[i]->getDisplayHandler();
    if (dh && dh->getUnit() && dh->getUnit()->isMain())
      return m_children[i];
  }
  return NULL;
}

void MultiDisplay::clearLine(int row) {
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->clearLine(row);
}

void MultiDisplay::activate() { resendAllRows(); }

void MultiDisplay::clear() {
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->clear();
}

void MultiDisplay::resendRow(int iRow) {
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->resendRow(iRow);
}

void MultiDisplay::resendAllRows() {
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->resendAllRows();
}

void MultiDisplay::switchToAll() {
  for (size_t i = 0; i < m_children.size(); i++) {
    m_children[i]->getDisplayHandler()->switchTo(m_children[i]);
  }
}
