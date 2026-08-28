/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
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
  DisplayHandler* getDisplayHandler() const { return m_pDisplayHandler; }

  virtual void activate();
  virtual void clear();

  virtual void resendRow(int iRow);
  virtual void resendAllRows();

  virtual void forwardRowTo(int sourceRow, Display *pDisplay, int targetRow);
  //      virtual static const char* getName() = 0;

	// Logical row width: 56 for every row. Rows 0/1 address the main panel,
	// whose hardware rows are 55 visible chars plus one unused byte each
	// (see DisplayHandler::sendToHardware); the extra logical column (index
	// 55) is what the emulated level meter uses for local channel 8. On
	// controllers with a 56-char LCD (iCON/Behringer) that column is
	// visible, on the original 55-char Mackie Control it maps to the unused
	// byte (that model uses the native hardware meters instead).
	virtual int getRowLength(int row) { return 56; }

	void showDB(int row, int channel, double volume);
  void showPan(int row, int channel, double pan);

protected:
  void writeToBuffer(int row, int pos, const char *text, int pad);
};

#endif
