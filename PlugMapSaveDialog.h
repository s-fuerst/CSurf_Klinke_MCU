/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  20 Dec 2009 1:17:38 am

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_PLUGMAPSAVEDIALOG_PLUGMAPSAVEDIALOG_2E574170__
#define __JUCER_HEADER_PLUGMAPSAVEDIALOG_PLUGMAPSAVEDIALOG_2E574170__

//[Headers]     -- You can add your own extra header files here --
#include "PlugMapManager.h"
#include "juce.h"

#define SAVEMAP_OK 11
#define SAVEMAP_CANCEL 12

class SaveDialogWindow :
        public DialogWindow
{
public:
        SaveDialogWindow(const String& name,
                const Colour& backgroundColour,
                const bool escapeKeyTriggersCloseButton,
                const bool addToDesktop) : DialogWindow(name, backgroundColour, escapeKeyTriggersCloseButton, addToDesktop){};

        ~SaveDialogWindow() {
        }

        void closeButtonPressed() {
                setVisible(false);
        }
};

class PlugMapManager;
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Jucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class PlugMapSaveDialog  : public Component,
                           public ButtonListener,
                           public LabelListener
{
public:
    //==============================================================================
    PlugMapSaveDialog (PlugMapManager* pPMM);
    ~PlugMapSaveDialog();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void buttonClicked (Button* buttonThatWasClicked);
    void labelTextChanged (Label* labelThatHasChanged);


    //==============================================================================
    juce_UseDebuggingNewOperator

private:
    //[UserVariables]   -- You can add your own custom variables in this section.
                PlugMapManager* m_pPMM;
    //[/UserVariables]

    //==============================================================================
    TextButton* m_okButton;
    TextButton* m_cancelButton;
    Label* m_labelExisting;
    TextEditor* m_mapList;
    Label* m_labelNameOfNewMap;
    Label* m_mapName;
    TextButton* m_fileDialog;
    Label* m_labelConflict;
    Label* m_labelExisting2;
    TextEditor* m_mapInstalled;

    //==============================================================================
    // (prevent copy constructor and operator= being generated..)
    PlugMapSaveDialog (const PlugMapSaveDialog&);
    const PlugMapSaveDialog& operator= (const PlugMapSaveDialog&);
};


#endif   // __JUCER_HEADER_PLUGMAPSAVEDIALOG_PLUGMAPSAVEDIALOG_2E574170__
