/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include <boost/smart_ptr/scoped_ptr.hpp>
#include "Display.h"
#include "McuAssert.h"
#include "csurf_mcu.h"

Display::Display(DisplayHandler *pDisplayHandler, int numRows) {
  m_pDisplayHandler = pDisplayHandler;
  m_ppText = new char *[numRows];
  m_numRows = numRows;
  m_wait = false;

  for (int iRow = 0; iRow < numRows; iRow++) {
    m_ppText[iRow] = new char[getRowLength(iRow)];
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
  int textlen = std::min(pad, (int)strnlen(text, std::min(pad, getRowLength(row))));
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

  safe_delete_array(pCenteredText);
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
  changeText(row, 0, text, getRowLength(row), centered);
}

void Display::changeTextAutoPad(int row, int pos, const char *text,
                                bool centered) {
  changeText(row, pos, text,
             static_cast<int>(strnlen(text, getRowLength(row))), centered);
}

void Display::clearLine(int row) { changeTextFullLine(row, ""); }

void Display::changeField(int row, int field, const char *text, bool centered) {
	if (row < 2) {
		ASSERT(field > 0 && field < 9);
		changeText(row, (field - 1) * 7, text, 6);
	} else {
		ASSERT(field > 0 && field < 10);
		changeText(row,
							 (field - 1) * 6 + ((field > 4) ? 1 : 0),
							 text,
							 5 + ((field > 8) ? 2 : 0));
	}
}

void Display::forwardRowTo(int sourceRow, Display *pDisplay, int targetRow) {
  m_ppForwardToDisplay[sourceRow] = pDisplay;
  m_pForwardToRow[sourceRow] = targetRow;
  m_ppForwardToDisplay[sourceRow]->changeTextFullLine(
      m_pForwardToRow[sourceRow], m_ppText[sourceRow]);
}

void Display::writeToBuffer(int row, int pos, const char *text, int pad) {
  if (pad + pos > getRowLength(row))
    pad = getRowLength(row) - pos;

  int l = static_cast<int>(strnlen(text, getRowLength(row)));
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

void Display::showDB(int row, int id, double volume) {
  char text[7];
  double asDB = VAL2DB(volume);
  if (id > 0) {
    if (asDB > -100)
      sprintf(text, "%5.1f", VAL2DB(volume));
    else
      sprintf(text, " -inf");
    changeField(row, id, text);
  }
}

void Display::showPan(int row, int id, double pan) {
  char text[7];
	int i = (int) (pan * 100);
	char side = i < 0 ? 'L' : 'R';
  if (id > 0) {
		if (i != 0)
			sprintf(text, "%3d%%%c", abs(i), side);
		else
			sprintf(text, "center");
    changeField(row, id, text);
  }
}
