/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  29 Sep 2009 1:09:20 am

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_COMMANDMODEMAINCOMPONENT_COMMANDMODEMAINCOMPONENT_FA758AC3__
#define __JUCER_HEADER_COMMANDMODEMAINCOMPONENT_COMMANDMODEMAINCOMPONENT_FA758AC3__

//[Headers]     -- You can add your own extra header files here --
#include "csurf_mcu.h"
#include "JuceHeader.h"

#include  "CommandMode.h"
//[/Headers]



//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class CommandModeMainComponent : public Component {
public:
  //==============================================================================
  CommandModeMainComponent(CommandMode *pCommandMode);
  ~CommandModeMainComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  void updateTabNames();
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();
  void focusLost(FocusChangeType cause);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      CommandMode *m_pCommandMode;
  //[/UserVariables]

  //==============================================================================
  TabbedComponent *tabbedPages;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  CommandModeMainComponent(const CommandModeMainComponent &);
  const CommandModeMainComponent &operator=(const CommandModeMainComponent &);
};

#endif // __JUCER_HEADER_COMMANDMODEMAINCOMPONENT_COMMANDMODEMAINCOMPONENT_FA758AC3__
