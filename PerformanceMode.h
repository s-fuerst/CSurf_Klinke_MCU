/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "CCSMode.h"

class PerformanceMode : public CCSMode {
public:
  PerformanceMode(CCSManager *pManager);

public:
  virtual ~PerformanceMode(void);

  void updateDisplay();

private:
  Display *m_pDisplay;
};

// class MyApp : public wxApp
// {
// public:
//      virtual bool OnInit(){return true;}
// };
//
// class MyThread : public wxThread
// {
// public:
//      MyThread(/*MyFrame *handler*/)
//              : wxThread(wxTHREAD_DETACHED)
//      {/* m_pHandler = handler */}
// //   ~MyThread();
//
// protected:
//      virtual ExitCode Entry(){
//              wxEntry(0, NULL);
//              return 0;
//      }
//
//      public wxThreadError Run() {
//              thread.
//              wxEntry(0, NULL);
//              return wxTHREAD_NO_ERROR;
//      }
//      //MyFrame *m_pHandler;
// };
