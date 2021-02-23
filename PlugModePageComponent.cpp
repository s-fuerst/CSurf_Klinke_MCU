/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:21:27 pm

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
#include "PlugModeSinglePageComponent.h"
#include "PlugModePageComponent.h"
#include "PlugModeComponent.h"
//[/Headers]

#include "PlugModePageComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
#include <src/juce_DefineMacros.h>
//[/MiscUserDefs]

//==============================================================================
PlugModePageComponent::PlugModePageComponent(PlugModeComponent *pMC,
                                             PMBank *pBank)
    : m_tabbedPages(0), m_pBank(NULL), m_pMainComponent(NULL) {
  addAndMakeVisible(m_tabbedPages = new TabbedComponentWithCallback(
                        TabbedButtonBar::TabsAtTop, this));
  m_tabbedPages->setTabBarDepth(30);
  m_tabbedPages->setCurrentTabIndex(-1);

  //[UserPreSize]
  for (int i = 0; i < 8; i++) {
    String tabName = pBank->getPage(i)->getNameShort();
    if (tabName.isEmpty()) {
      tabName = String::formatted(T("Page %d"), i + 1);
    }
    m_tabbedPages->addTab(
        tabName, Colours::white,
        new PlugModeSinglePageComponent(pMC, this, pBank->getPage(i)), true);
  }
  //[/UserPreSize]

  setSize(610, 368);

  //[Constructor] You can add your own custom stuff here..
  m_pBank = pBank;
  m_pMainComponent = pMC;
  //[/Constructor]
}

PlugModePageComponent::~PlugModePageComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_tabbedPages);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModePageComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModePageComponent::resized() {
  m_tabbedPages->setBounds(0, 0, 610, 368);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModePageComponent::updatePageNames() {
  if (m_pBank) {
    for (int i = 0; i < 8; i++) {
      if (m_pBank->getPage(i)) {
        m_tabbedPages->setTabName(i, m_pBank->getPage(i)->getNameShort());
      }
    }
  }
}

void PlugModePageComponent::updateEverything() {
  updatePageNames();
  safe_call(getSelectedPageComponent(), updateEverything())
}

void PlugModePageComponent::selectedTabHasChanged() {
  updateEverything();

  if (m_pMainComponent) {
    safe_call(
        m_pMainComponent->getPlugAccess(),
        setSelectedPageInSelectedBank(m_tabbedPages->getCurrentTabIndex()))
        safe_call(m_pMainComponent, updateLearnStatus())
  }
}

void PlugModePageComponent::setTabVisible(bool shouldBeVisible) {
  safe_call(getSelectedPageComponent(),
            makeChannelComponentVisible(shouldBeVisible))
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModePageComponent" componentName=""
		parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PMBank* pBank"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="610"
		initialHeight="368">
		<BACKGROUND backgroundColour="ffffffff"/>
		<TABBEDCOMPONENT name="Tabbed Pages" id="1206c291312b441" memberName="m_tabbedPages"
		virtualName="" explicitFocusOrder="0" pos="0 0 610 368" orientation="top"
		tabBarDepth="30" initialTab="-1"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
