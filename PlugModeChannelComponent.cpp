/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:31:33 pm

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
#include "PlugModeSingleChannelComponent.h"
#include "PlugModeComponent.h"
//[/Headers]

#include "PlugModeChannelComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeChannelComponent::PlugModeChannelComponent(PlugModeComponent *pMC,
                                                   PMPage *pPage)
    : m_tabbedChannels(0), m_pMainComponent(pMC) {
  addAndMakeVisible(m_tabbedChannels = new TabbedComponentWithCallback(
                        TabbedButtonBar::TabsAtTop, this));
  m_tabbedChannels->setTabBarDepth(30);
  m_tabbedChannels->setCurrentTabIndex(-1);

  //[UserPreSize]
  for (int i = 0; i < 8; i++) {
    m_tabbedChannels->addTab(String::formatted(T("Channel %d"), i + 1),
                             Colours::white,
                             new PlugModeSingleChannelComponent(
                                 pMC, pPage->getFader(i), pPage->getVPot(i)),
                             true);
  }
  //[/UserPreSize]

  setSize(604, 284);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

PlugModeChannelComponent::~PlugModeChannelComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_tabbedChannels);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeChannelComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeChannelComponent::resized() {
  m_tabbedChannels->setBounds(0, 0, 604, 284);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeChannelComponent::updateEverything() {
  getSelectedChannelComponent()->updateEverything();
}

void PlugModeChannelComponent::selectedChannelChanged(int iChannel) {
  if (iChannel != m_tabbedChannels->getCurrentTabIndex()) {
    m_tabbedChannels->setCurrentTabIndex(iChannel);
    getSelectedChannelComponent()->updateEverything();
  }

  safe_call(m_pMainComponent, updateLearnStatus())
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeChannelComponent"
		componentName="" parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PMPage* pPage"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="604"
		initialHeight="284">
		<BACKGROUND backgroundColour="ffffffff"/>
		<TABBEDCOMPONENT name="Tabbed Channels" id="1206c291312b441" memberName="m_tabbedChannels"
		virtualName="" explicitFocusOrder="0" pos="0 0 604 284" orientation="top"
		tabBarDepth="30" initialTab="-1"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
