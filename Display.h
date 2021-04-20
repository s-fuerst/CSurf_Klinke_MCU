/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifndef MCU_DISPLAY
#define MCU_DISPLAY

#include "csurf.h"

#include "DisplayHandler.h"

class Display {
protected:
  DisplayHandler *m_pDisplayHandler;
  char **m_ppText;
  int m_numRows;
  Display **m_ppForwardToDisplay;
  int *m_pForwardToRow;
  bool m_wait;

public:
  Display(DisplayHandler *pDisplayHandler, int numRows);
  virtual ~Display();

  virtual void changeText(int row, int pos, const char *text, int pad,
                          bool centered = false);

  virtual void changeTextFullLine(int row, const char *text,
                                  bool centered = false);
  virtual void changeTextAutoPad(int row, int pos, const char *text,
                                 bool centered = false);
  virtual void clearLine(int row);
  virtual void changeField(int row, int field, const char *text,
                           bool centered = false);

  char **getText() { return m_ppText; }

  virtual void activate();
  virtual void clear();

  virtual void resendRow(int iRow);
  virtual void resendAllRows();

  virtual void forwardRowTo(int sourceRow, Display *pDisplay, int targetRow);
  //      virtual static const char* getName() = 0;

	virtual int getRowLength(int row) { return (row < 2) ? 55 : 56; }

	void showDB(int row, int channel, double volume);
  void showPan(int row, int channel, double pan);

protected:
  void writeToBuffer(int row, int pos, const char *text, int pad);
};

#endif
