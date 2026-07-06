/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
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

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "CommandModeMainComponent.h"
#include "CommandModePageComponent.h"
#include "CommandModePageComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
CommandModeMainComponent::CommandModeMainComponent(CommandMode *pCommandMode)
    : Component(String("CommandModeMain")), tabbedPages(0) {
  addAndMakeVisible(tabbedPages =
                        new TabbedComponent(TabbedButtonBar::TabsAtTop));
  tabbedPages->setTabBarDepth(30);
  tabbedPages->addTab(
      String("Page 1"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(0), this), true);
  tabbedPages->addTab(
      String("Page 2"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(1), this), true);
  tabbedPages->addTab(
      String("Page 3"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(2), this), true);
  tabbedPages->addTab(
      String("Page 4"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(3), this), true);
  tabbedPages->addTab(
      String("Page 5"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(4), this), true);
  tabbedPages->addTab(
      String("Page 6"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(5), this), true);
  tabbedPages->addTab(
      String("Page 7"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(6), this), true);
  tabbedPages->addTab(
      String("Page 8"), Colours::white,
      new CommandModePageComponent(pCommandMode->getPage(7), this), true);
  tabbedPages->setCurrentTabIndex(0);

  //[UserPreSize]
  m_pCommandMode = pCommandMode;
  updateTabNames();
  //[/UserPreSize]

  setSize(810, 380);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

CommandModeMainComponent::~CommandModeMainComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  m_pCommandMode->writeConfigFile();
  //[/Destructor_pre]

  deleteAndZero(tabbedPages);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void CommandModeMainComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void CommandModeMainComponent::resized() {
  tabbedPages->setBounds(0, 0, 810, 380);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

void CommandModeMainComponent::focusLost(FocusChangeType cause) {
  //[UserCode_focusLost] -- Add your code here...
  //[/UserCode_focusLost]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void CommandModeMainComponent::updateTabNames() {
  for (int i = 0; i < 8; i++) {
    tabbedPages->setTabName(i, m_pCommandMode->getPage(i)->m_strPageName);
  }
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="CommandModeMainComponent"
		componentName="CommandModeMain" parentClasses="public Component"
		constructorParams="CommandMode* pCommandMode" variableInitialisers=""
		snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330000013"
		fixedSize="1" initialWidth="810" initialHeight="380">
		<METHODS>
    <METHOD name="focusLost (FocusChangeType cause)"/>
		</METHODS>
		<BACKGROUND backgroundColour="ffffffff"/>
		<TABBEDCOMPONENT name="pages" id="1206c291312b441" memberName="tabbedPages" virtualName=""
		explicitFocusOrder="0" pos="0 0 810 380" orientation="top" tabBarDepth="30"
		initialTab="0">
    <TAB name="Page 1" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(0), this" jucerComponentFile="CommandModePageComponent.cpp"/>
    <TAB name="Page 2" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(1), this" jucerComponentFile="CommandModePageComponent.cpp"/>
    <TAB name="Page 3" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(2), this" jucerComponentFile="CommandModePageComponent.cpp"/>
    <TAB name="Page 4" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(3), this" jucerComponentFile="CommandModePageComponent.cpp"/>
    <TAB name="Page 5" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(4), this" jucerComponentFile="CommandModePageComponent.cpp"/>
    <TAB name="Page 6" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(5), this" jucerComponentFile="CommandModePageComponent.cpp"/>
    <TAB name="Page 7" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(6), this" jucerComponentFile="CommandModePageComponent.cpp"/>
    <TAB name="Page 8" colour="ffffffff" useJucerComp="1" contentClassName=""
		constructorParams="pCommandMode-&gt;getPage(7), this" jucerComponentFile="CommandModePageComponent.cpp"/>
		</TABBEDCOMPONENT>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
