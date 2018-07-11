/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#pragma once
#include "display.h"
#include <src/juce_WithoutMacros.h>

class DisplayHandler;

class ActionsDisplay : public Display
{
public:
  ActionsDisplay(DisplayHandler* pDH);
  virtual ~ActionsDisplay(void);
  
  void activate(int modifiers);
  void deactivate();

  void switchTo(int modifiers);
  
  String getLabel(int modifiers, int nr);
  void setLabel(int modifiers, int nr, String& newText);

  void writeConfigFile();
private:
  void updateDisplay();
  
  File getConfigFile(boolean bLookAtProgramDir);
  bool readConfigFile();
  
  String m_strLabel[16][8];

  int m_shownModifier;

  DisplayHandler* m_pDisplayHandler;
  Display* m_pOtherDisplay;
};
