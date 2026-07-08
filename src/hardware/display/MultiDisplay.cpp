/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * WP-A stub. Fleshed out in Step 7.
 */
#include "MultiDisplay.h"

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
  for (size_t i = 0; i < m_children.size(); i++)
    m_children[i]->changeField(row, field, text, centered);
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
  // Step 7 wires the per-child handler switch.
}
