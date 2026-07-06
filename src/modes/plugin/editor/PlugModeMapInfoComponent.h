/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  30 Nov 2009 8:52:53 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMODEMAPINFOCOMPONENT_PLUGMODEMAPINFOCOMPONENT_6D9C6790__
#define __JUCER_HEADER_PLUGMODEMAPINFOCOMPONENT_PLUGMODEMAPINFOCOMPONENT_6D9C6790__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"

class PlugModeComponent;
class PlugMap;
//[/Headers]

//==============================================================================
/**
 //[Comments]
 An auto-generated component, created by the Jucer.

 Describe your class and how it works here!
 //[/Comments]
 */
class PlugModeMapInfoComponent : public Component, public TextEditor::Listener {
public:
  //==============================================================================
  PlugModeMapInfoComponent(PlugModeComponent *pMC, PlugMap *pMap);
  ~PlugModeMapInfoComponent();

  //==============================================================================
  //[UserMethods]     -- You can add your own custom methods in this section.
  void updateEverything();
  /** TextEditor::Listener callbacks */
  void textEditorTextChanged(TextEditor &editor);
  void textEditorReturnKeyPressed(TextEditor &editor){};
  void textEditorEscapeKeyPressed(TextEditor &editor){};
  void textEditorFocusLost(TextEditor &editor){};
  //[/UserMethods]

  void paint(Graphics &g);
  void resized();

  //==============================================================================
  juce_UseDebuggingNewOperator

      private :
      //[UserVariables]   -- You can add your own custom variables in this
      //section.
      PlugMap *m_pPlugMap;
  //[/UserVariables]

  //==============================================================================
  TextEditor *m_textCreator;
  Label *m_labelCreator;
  TextEditor *m_textNotes;
  Label *m_labelNotes;

  //==============================================================================
  // (prevent copy constructor and operator= being generated..)
  PlugModeMapInfoComponent(const PlugModeMapInfoComponent &);
  const PlugModeMapInfoComponent &operator=(const PlugModeMapInfoComponent &);
};

#endif // __JUCER_HEADER_PLUGMODEMAPINFOCOMPONENT_PLUGMODEMAPINFOCOMPONENT_6D9C6790__
