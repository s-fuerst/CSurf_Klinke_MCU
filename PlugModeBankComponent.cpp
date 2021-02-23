/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  11 Nov 2009 11:13:09 pm

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
#include "PlugModeSingleBankComponent.h"
#include "PlugModeBankComponent.h"
#include "PlugModeComponent.h"
#include "PlugModeMapInfoComponent.h"
//[/Headers]

#include "PlugModeBankComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
#include <src/juce_DefineMacros.h>
#include "plugmodecomponent.h"
//[/MiscUserDefs]

//==============================================================================
PlugModeBankComponent::PlugModeBankComponent(PlugMap *pMap,
                                             PlugModeComponent *pMC)
    : m_tabbedBanks(0), m_pMap(NULL), m_pMainComponent(NULL) {
  addAndMakeVisible(m_tabbedBanks = new TabbedComponentWithCallback(
                        TabbedButtonBar::TabsAtTop, this));
  m_tabbedBanks->setTabBarDepth(30);
  m_tabbedBanks->setCurrentTabIndex(-1);

  //[UserPreSize]
  for (int i = 0; i < 8; i++) {
    String tabName = pMap->getBank(i)->getNameShort();
    if (tabName.isEmpty()) {
      tabName = String::formatted(T("Bank %d"), i + 1);
    }
    m_tabbedBanks->addTab(
        tabName, Colours::white,
        new PlugModeSingleBankComponent(pMC, this, pMap->getBank(i)), true);
  }
  m_tabbedBanks->addTab(JUCE_T("Map Info"), Colours::white,
                        new PlugModeMapInfoComponent(pMC, pMap), true);
  //[/UserPreSize]

  setSize(618, 453);

  //[Constructor] You can add your own custom stuff here..
  m_pMap = pMap;
  m_pMainComponent = pMC;
  //[/Constructor]
}

PlugModeBankComponent::~PlugModeBankComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_tabbedBanks);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeBankComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeBankComponent::resized() {
  m_tabbedBanks->setBounds(2, 0, 616, 453);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeBankComponent::updateBankNames() {
  if (m_pMap) {
    for (int i = 0; i < 8; i++) {
      m_tabbedBanks->setTabName(i, m_pMap->getBank(i)->getNameShort());
    }
  }
}

void PlugModeBankComponent::updateEverything() {
  updateBankNames();
  safe_call(getSelectedBankComponent(), updateEverything());
  safe_call(static_cast<PlugModeMapInfoComponent *>(
                m_tabbedBanks->getTabContentComponent(8)),
            updateEverything())
}

void PlugModeBankComponent::selectedTabHasChanged() {
  updateEverything();

  if (m_pMainComponent && m_tabbedBanks->getCurrentTabIndex() < 8) {
    safe_call(m_pMainComponent->getPlugAccess(),
              setSelectedBank(m_tabbedBanks->getCurrentTabIndex()))
        safe_call(m_pMainComponent, updateLearnStatus())
  }
}

PlugModeSingleBankComponent *
PlugModeBankComponent::getSingleBankComponent(int iBank) {
  return static_cast<PlugModeSingleBankComponent *>(
      m_tabbedBanks->getTabContentComponent(iBank));
}

void PlugModeBankComponent::setTabVisible(bool shouldBeVisible){
    safe_call(getSelectedBankComponent(),
              makePageComponentVisible(shouldBeVisible))}

PlugModeSingleBankComponent *PlugModeBankComponent::getSelectedBankComponent() {
  if (m_tabbedBanks->getCurrentTabIndex() < 8)
    return getSingleBankComponent(m_tabbedBanks->getCurrentTabIndex());

  return NULL;
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeBankComponent" componentName=""
		parentClasses="public Component" constructorParams="PlugMap* pMap, PlugModeComponent* pMC"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="618"
		initialHeight="453">
		<BACKGROUND backgroundColour="ffffffff"/>
		<TABBEDCOMPONENT name="Tabbed Banks" id="1206c291312b441" memberName="m_tabbedBanks"
		virtualName="" explicitFocusOrder="0" pos="2 0 616 453" orientation="top"
		tabBarDepth="30" initialTab="-1"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
