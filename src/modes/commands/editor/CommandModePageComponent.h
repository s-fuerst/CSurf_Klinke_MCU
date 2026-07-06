/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  29 Sep 2009 1:07:43 am

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_COMMANDMODEPAGECOMPONENT_COMMANDMODEPAGECOMPONENT_AF13CAB8__
#define __JUCER_HEADER_COMMANDMODEPAGECOMPONENT_COMMANDMODEPAGECOMPONENT_AF13CAB8__

//[Headers]     -- You can add your own extra header files here --
#include "csurf_mcu.h"
#include "JuceHeader.h"
#include "CommandMode.h"
#include "CommandModeVPOTComponent.h"
//[/Headers]



//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class CommandModePageComponent : public Component, public Label::Listener {
public:
  //==============================================================================
  CommandModePageComponent(CommandMode::Page *pPage,
                           CommandModeMainComponent *pMain);
  ~CommandModePageComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();
  void labelTextChanged(Label *labelThatHasChanged);

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      CommandModeVPOTComponent *vpotComponent[2][8];
  CommandMode::Page *m_pPage;
  CommandModeMainComponent *m_pMain;
  //[/UserVariables]

  //==============================================================================
  Label *pageNameLabel;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  CommandModePageComponent(const CommandModePageComponent &);
  const CommandModePageComponent &operator=(const CommandModePageComponent &);
};

#endif // __JUCER_HEADER_COMMANDMODEPAGECOMPONENT_COMMANDMODEPAGECOMPONENT_AF13CAB8__
