/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "boost\smart_ptr\scoped_ptr.hpp"
#include "Display.h"
#include "Assert.h"
#include "csurf_mcu.h"

Display::Display(DisplayHandler *pDisplayHandler, int numRows) {
  m_pDisplayHandler = pDisplayHandler;
  m_ppText = new char *[numRows];
  m_numRows = numRows;
  m_wait = false;

  for (int iRow = 0; iRow < numRows; iRow++) {
    m_ppText[iRow] = new char[DISPLAY_ROW_LENGTH];
  }

  m_ppForwardToDisplay = new Display *[numRows];
  memset(m_ppForwardToDisplay, 0, numRows * sizeof(Display *));
  m_pForwardToRow = new int[numRows];

  clear();
}

Display::~Display() {
  for (int iRow = 0; iRow < m_numRows; iRow++) {
    delete[](m_ppText[iRow]);
  }
  delete[] m_ppText;
  delete[] m_ppForwardToDisplay;
  delete[] m_pForwardToRow;
}

void Display::changeText(int row, int pos, const char *text, int pad,
                         bool centered) {
  ASSERT(row < m_numRows);

  char *pCenteredText = new char[pad + 1];
  int textlen = min(pad, (int)strnlen(text, DISPLAY_ROW_LENGTH));
  memset(pCenteredText, ' ', pad + 1);
  if (centered)
    strncpy(pCenteredText + ((pad - textlen) / 2), text, textlen);
  else
    strncpy(pCenteredText, text, textlen);

  if (textlen == 0) {
    pCenteredText[textlen] = 0;
  } else {
    ASSERT(textlen + ((pad - textlen) / 2) < (pad + 1));
    pCenteredText[textlen + ((pad - textlen) / 2)] = 0;
  }

  writeToBuffer(row, pos, pCenteredText, pad);

  if (m_ppForwardToDisplay[row])
    m_ppForwardToDisplay[row]->changeText(m_pForwardToRow[row], pos,
                                          m_ppText[row], pad);

  safe_delete(pCenteredText);
}

void Display::activate() { resendAllRows(); }

void Display::resendRow(int iRow) {
  m_pDisplayHandler->sendDifferences(this, iRow, m_ppText[iRow]);
}

void Display::resendAllRows() {
  for (int iRow = 0; iRow < m_numRows; iRow++)
    resendRow(iRow);
}

void Display::clear() {
  for (int iRow = 0; iRow < m_numRows; iRow++)
    changeTextFullLine(iRow, "");
}

void Display::changeTextFullLine(int row, const char *text, bool centered) {
  changeText(row, 0, text, DISPLAY_ROW_LENGTH, centered);
}

void Display::changeTextAutoPad(int row, int pos, const char *text,
                                bool centered) {
  changeText(row, pos, text, strnlen(text, DISPLAY_ROW_LENGTH), centered);
}

void Display::clearLine(int row) { changeTextFullLine(row, ""); }

void Display::changeField(int row, int field, const char *text, bool centered) {
  ASSERT(field > 0 && field < 9);
  changeText(row, (field - 1) * 7, text, 6);
}

void Display::forwardRowTo(int sourceRow, Display *pDisplay, int targetRow) {
  m_ppForwardToDisplay[sourceRow] = pDisplay;
  m_pForwardToRow[sourceRow] = targetRow;
  m_ppForwardToDisplay[sourceRow]->changeTextFullLine(
      m_pForwardToRow[sourceRow], m_ppText[sourceRow]);
}

void Display::writeToBuffer(int row, int pos, const char *text, int pad) {
  if (pad + pos > DISPLAY_ROW_LENGTH)
    pad = DISPLAY_ROW_LENGTH - pos;

  int l = strnlen(text, DISPLAY_ROW_LENGTH);
  if (pad < l)
    l = pad;

  int cnt = 0;
  char *cpos = m_ppText[row] + pos;
  while (cnt < l) {
    *cpos++ = *text++;
    cnt++;
  }
  while (cnt++ < pad)
    *cpos++ = ' ';
}
