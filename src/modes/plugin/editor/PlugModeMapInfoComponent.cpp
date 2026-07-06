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

//[Headers] You can add your own extra header files here...
#include "PlugMap.h"
//[/Headers]

#include "PlugModeMapInfoComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeMapInfoComponent::PlugModeMapInfoComponent(PlugModeComponent *pMC,
                                                   PlugMap *pMap)
    : m_textCreator(0), m_labelCreator(0), m_textNotes(0), m_labelNotes(0) {
  addAndMakeVisible(m_textCreator = new TextEditor(String("Creator text editor")));
  m_textCreator->setTooltip(
      String("The person how created the map can enter his name here (e.g. for "
        "request from other users, for fame etc.)"));
  m_textCreator->setMultiLine(false);
  m_textCreator->setReturnKeyStartsNewLine(false);
  m_textCreator->setReadOnly(false);
  m_textCreator->setScrollbarsShown(true);
  m_textCreator->setCaretVisible(true);
  m_textCreator->setPopupMenuEnabled(true);
  m_textCreator->setColour(TextEditor::outlineColourId, Colours::black);
  m_textCreator->setColour(TextEditor::shadowColourId, Colour(0x0));
  m_textCreator->setText(String());

  addAndMakeVisible(m_labelCreator =
                        new Label(String("Creator label"), String("Creator:\n")));
  m_labelCreator->setFont(Font(15.0000f, Font::plain));
  m_labelCreator->setJustificationType(Justification::centredLeft);
  m_labelCreator->setEditable(false, false, false);
  m_labelCreator->setColour(TextEditor::textColourId, Colours::black);
  m_labelCreator->setColour(TextEditor::backgroundColourId, Colour(0x0));

  addAndMakeVisible(m_textNotes = new TextEditor(String("notes text editor")));
  m_textNotes->setTooltip(String("A map related notepad"));
  m_textNotes->setMultiLine(true);
  m_textNotes->setReturnKeyStartsNewLine(true);
  m_textNotes->setReadOnly(false);
  m_textNotes->setScrollbarsShown(true);
  m_textNotes->setCaretVisible(true);
  m_textNotes->setPopupMenuEnabled(true);
  m_textNotes->setColour(TextEditor::outlineColourId, Colours::black);
  m_textNotes->setColour(TextEditor::shadowColourId, Colour(0x0));
  m_textNotes->setText(String());

  addAndMakeVisible(m_labelNotes = new Label(String("Notes label"), String("Notes:\n")));
  m_labelNotes->setFont(Font(15.0000f, Font::plain));
  m_labelNotes->setJustificationType(Justification::centredLeft);
  m_labelNotes->setEditable(false, false, false);
  m_labelNotes->setColour(TextEditor::textColourId, Colours::black);
  m_labelNotes->setColour(TextEditor::backgroundColourId, Colour(0x0));

  //[UserPreSize]
  m_textCreator->addListener(this);
  m_textNotes->addListener(this);
  //[/UserPreSize]

  setSize(616, 424);

  //[Constructor] You can add your own custom stuff here..
  m_pPlugMap = pMap;
  updateEverything();
  //[/Constructor]
}

PlugModeMapInfoComponent::~PlugModeMapInfoComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_textCreator);
  deleteAndZero(m_labelCreator);
  deleteAndZero(m_textNotes);
  deleteAndZero(m_labelNotes);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeMapInfoComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeMapInfoComponent::resized() {
  m_textCreator->setBounds(8, 38, 598, 24);
  m_labelCreator->setBounds(8, 12, 150, 24);
  m_textNotes->setBounds(8, 113, 598, 300);
  m_labelNotes->setBounds(8, 86, 150, 24);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeMapInfoComponent::textEditorTextChanged(TextEditor &editor) {
  if (&editor == m_textNotes) {
    m_pPlugMap->setInfo(editor.getText());
  } else if (&editor == m_textCreator) {
    m_pPlugMap->setCreator(editor.getText());
  }
}

void PlugModeMapInfoComponent::updateEverything() {
  m_textCreator->setText(m_pPlugMap->getCreator());
  m_textNotes->setText(m_pPlugMap->getInfo());
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeMapInfoComponent"
		componentName="" parentClasses="public Component, public TextEditor::Listener"
		constructorParams="PlugModeComponent* pMC, PlugMap* pMap" variableInitialisers=""
		snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330000013"
		fixedSize="1" initialWidth="616" initialHeight="424">
		<BACKGROUND backgroundColour="ffffffff"/>
		<TEXTEDITOR name="Creator text editor" id="a7f041773780a819" memberName="m_textCreator"
		virtualName="" explicitFocusOrder="0" pos="8 38 598 24" tooltip="The person how created the map can enter his name here (e.g. for request from other users, for fame etc.)"
		outlinecol="ff000000" shadowcol="0" initialText="" multiline="0"
		retKeyStartsLine="0" readonly="0" scrollbars="1" caret="1" popupmenu="1"/>
		<LABEL name="Creator label" id="281ab025744bf6bc" memberName="m_labelCreator"
		virtualName="" explicitFocusOrder="0" pos="8 12 150 24" edTextCol="ff000000"
		edBkgCol="0" labelText="Creator:&#10;" editableSingleClick="0" editableDoubleClick="0"
		focusDiscardsChanges="0" fontname="Default font" fontsize="15"
		bold="0" italic="0" justification="33"/>
		<TEXTEDITOR name="notes text editor" id="9d0f7807a1da839e" memberName="m_textNotes"
		virtualName="" explicitFocusOrder="0" pos="8 113 598 300" tooltip="A map related notepad"
		outlinecol="ff000000" shadowcol="0" initialText="" multiline="1"
		retKeyStartsLine="1" readonly="0" scrollbars="1" caret="1" popupmenu="1"/>
		<LABEL name="Notes label" id="cfd94d2aa70a830e" memberName="m_labelNotes"
		virtualName="" explicitFocusOrder="0" pos="8 86 150 24" edTextCol="ff000000"
		edBkgCol="0" labelText="Notes:&#10;" editableSingleClick="0" editableDoubleClick="0"
		focusDiscardsChanges="0" fontname="Default font" fontsize="15"
		bold="0" italic="0" justification="33"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
