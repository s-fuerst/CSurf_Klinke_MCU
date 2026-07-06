/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
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

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "CommandModePageComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
CommandModePageComponent::CommandModePageComponent(
    CommandMode::Page *pPage, CommandModeMainComponent *pMain)
    : Component(String("CommandModePage")), pageNameLabel(0) {
  addAndMakeVisible(pageNameLabel = new Label(String("PageName"), String("Page x")));
  pageNameLabel->setTooltip(String("The name of the selected page (you can select a "
                              "different page with the tabs above)."));
  pageNameLabel->setFont(
      Font(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain));
  pageNameLabel->setJustificationType(Justification::centredLeft);
  pageNameLabel->setEditable(true, true, false);
  pageNameLabel->setColour(Label::backgroundColourId, Colours::white);
  pageNameLabel->setColour(Label::outlineColourId, Colours::black);
  pageNameLabel->setColour(TextEditor::textColourId, Colours::black);
  pageNameLabel->setColour(TextEditor::backgroundColourId, Colour(0x0));
  pageNameLabel->addListener(this);

  //[UserPreSize]
  m_pPage = pPage;
  m_pMain = pMain;

  for (int channel = 0; channel < 8; channel++) {
    addAndMakeVisible(vpotComponent[0][channel] =
                          new CommandModeVPOTComponent(pPage, 0, channel));
    addAndMakeVisible(vpotComponent[1][channel] =
                          new CommandModeVPOTComponent(pPage, 1, channel));
    vpotComponent[0][channel]->setExplicitFocusOrder(channel * 2 + 10);
    vpotComponent[1][channel]->setExplicitFocusOrder(channel * 2 + 11);
  }
  pageNameLabel->setText(m_pPage->m_strPageName, dontSendNotification);
  //[/UserPreSize]

  setSize(810, 380);

  //[Constructor] You can add your own custom stuff here..
  //[/Constructor]
}

CommandModePageComponent::~CommandModePageComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(pageNameLabel);

  //[Destructor]. You can add your own custom destruction code here..
  for (int channel = 0; channel < 8; channel++) {
    deleteAndZero(vpotComponent[0][channel]);
    deleteAndZero(vpotComponent[1][channel]);
  }
  //[/Destructor]
}

//==============================================================================
void CommandModePageComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Action Name"), 14, 60, 130, 24,
             Justification::centredRight, true);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Relative Mode"), 14, 90, 130, 24, Justification::centredRight,
             true);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Normal Speed"), 14, 120, 130, 24, Justification::centredRight,
             true);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Pressed Speed"), 14, 150, 130, 24, Justification::centredRight,
             true);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Action Name"), 14, 210, 130, 24,
             Justification::centredRight, true);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Relative Mode"), 14, 240, 130, 24, Justification::centredRight,
             true);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Normal Speed"), 14, 270, 130, 24, Justification::centredRight,
             true);

  g.setColour(Colours::black);
  g.setFont(Font(15.0000f, Font::bold));
  g.drawText(String("Pressed Speed"), 14, 300, 130, 24, Justification::centredRight,
             true);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void CommandModePageComponent::resized() {
  pageNameLabel->setBounds(396, 10, 60, 24);
  //[UserResized] Add your own custom resize handling here..
  for (int channel = 0; channel < 8; channel++) {
    vpotComponent[0][channel]->setBounds(146 + 80 * channel, 39, 80, 150);
    vpotComponent[1][channel]->setBounds(146 + 80 * channel, 189, 80, 150);
  }
  //[/UserResized]
}

void CommandModePageComponent::labelTextChanged(Label *labelThatHasChanged) {
  //[UserlabelTextChanged_Pre]
  //[/UserlabelTextChanged_Pre]

  if (labelThatHasChanged == pageNameLabel) {
    //[UserLabelCode_pageNameLabel] -- add your label text handling code here..
    pageNameLabel->setText(pageNameLabel->getText().substring(0, 6), dontSendNotification);
    m_pPage->m_strPageName = pageNameLabel->getText();
    m_pMain->updateTabNames();
    //[/UserLabelCode_pageNameLabel]
  }

  //[UserlabelTextChanged_Post]
  //[/UserlabelTextChanged_Post]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="CommandModePageComponent"
		componentName="CommandModePage" parentClasses="public Component"
		constructorParams="CommandMode::Page* pPage, CommandModeMainComponent* pMain"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="810"
		initialHeight="380">
		<BACKGROUND backgroundColour="ffffffff">
    <TEXT pos="14 60 130 24" fill="solid: ff000000" hasStroke="0" text="Action Name"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
    <TEXT pos="14 90 130 24" fill="solid: ff000000" hasStroke="0" text="Relative Mode"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
    <TEXT pos="14 120 130 24" fill="solid: ff000000" hasStroke="0" text="Normal Speed"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
    <TEXT pos="14 150 130 24" fill="solid: ff000000" hasStroke="0" text="Pressed Speed"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
    <TEXT pos="14 210 130 24" fill="solid: ff000000" hasStroke="0" text="Action Name"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
    <TEXT pos="14 240 130 24" fill="solid: ff000000" hasStroke="0" text="Relative Mode"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
    <TEXT pos="14 270 130 24" fill="solid: ff000000" hasStroke="0" text="Normal Speed"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
    <TEXT pos="14 300 130 24" fill="solid: ff000000" hasStroke="0" text="Pressed Speed"
		fontname="Default font" fontsize="15" bold="1" italic="0" justification="34"/>
		</BACKGROUND>
		<LABEL name="PageName" id="c61bbc2cfa171336" memberName="pageNameLabel"
		virtualName="" explicitFocusOrder="0" pos="396 10 60 24" tooltip="The name of the selected page (you can select a different page with the tabs above)."
		bkgCol="ffffffff" outlineCol="ff000000" edTextCol="ff000000"
		edBkgCol="0" labelText="Page x" editableSingleClick="1" editableDoubleClick="1"
		focusDiscardsChanges="0" fontname="Default monospaced font" fontsize="15"
		bold="0" italic="0" justification="33"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
