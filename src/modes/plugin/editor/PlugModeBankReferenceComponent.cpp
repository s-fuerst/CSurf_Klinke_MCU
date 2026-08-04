/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  26 Oct 2009 10:03:46 pm

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

#include "PlugModeBankReferenceComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
PlugModeBankReferenceComponent::PlugModeBankReferenceComponent(
    PlugModeBankComponent *pPMBC, PMBank *pBank)
    : groupComponent2(0), m_nameShort(0), m_nameLong(0), m_groupReference(0),
      m_referenceTo(0), m_offset(0), m_offsetLabel(0) {
  addAndMakeVisible(groupComponent2 =
                        new GroupComponent(String("new group"), String("Name")));
  groupComponent2->setColour(GroupComponent::outlineColourId, Colour(0x0));
  groupComponent2->setColour(GroupComponent::textColourId, Colour(0x0));

  addAndMakeVisible(m_nameShort = new Label(String("Name Short"), String("Bank x")));
  m_nameShort->setTooltip(String("This short name of the selected bank"));
  m_nameShort->setFont(
      Font(FontOptions(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain)));
  m_nameShort->setJustificationType(Justification::centredLeft);
  m_nameShort->setEditable(true, true, false);
  m_nameShort->setColour(Label::backgroundColourId, Colours::white);
  m_nameShort->setColour(Label::outlineColourId, Colours::black);
  m_nameShort->setColour(TextEditor::textColourId, Colours::black);
  m_nameShort->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_nameShort->addListener(this);

  addAndMakeVisible(m_nameLong =
                        new Label(String("Name Long"), String("12345678901234567")));
  m_nameLong->setTooltip(String("This long name of the selected bank"));
  m_nameLong->setFont(
      Font(FontOptions(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain)));
  m_nameLong->setJustificationType(Justification::centredLeft);
  m_nameLong->setEditable(true, true, false);
  m_nameLong->setColour(Label::backgroundColourId, Colours::white);
  m_nameLong->setColour(Label::outlineColourId, Colours::black);
  m_nameLong->setColour(TextEditor::textColourId, Colours::black);
  m_nameLong->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_nameLong->addListener(this);

  addAndMakeVisible(m_groupReference = new GroupComponent(String("Group Reference"),
                                                          String("Reference")));
  m_groupReference->setColour(GroupComponent::outlineColourId, Colours::black);

  addAndMakeVisible(m_referenceTo = new ComboBox(String("Reference To")));
  m_referenceTo->setTooltip(
      String("Use the page mappings of the selected bank. This can be useful if the "
        "plugin has parameter groups, eq. a drum synth that has n-times the "
        "same synth engine. In this case you must map the parameters only for "
        "one synth engine and can then refer to it."));
  m_referenceTo->setEditableText(false);
  m_referenceTo->setJustificationType(Justification::centredLeft);
  m_referenceTo->setTextWhenNothingSelected(String("something must be selected"));
  m_referenceTo->setTextWhenNoChoicesAvailable(String("we have items"));
  m_referenceTo->addItem(String("Bank 1"), 1);
  m_referenceTo->addItem(String("Bank 2"), 2);
  m_referenceTo->addItem(String("Bank 3"), 3);
  m_referenceTo->addItem(String("Bank 4"), 4);
  m_referenceTo->addItem(String("Bank 5"), 5);
  m_referenceTo->addItem(String("Bank 6"), 6);
  m_referenceTo->addItem(String("Bank 7"), 7);
  m_referenceTo->addItem(String("Bank 8"), 8);
  m_referenceTo->addListener(this);

  addAndMakeVisible(m_offset = new Label(String("Offset"), String("-20")));
  m_offset->setTooltip(String("The offset is determined by the difference of the "
                         "ids of repeating parameter groups. The ReaEQ map "
                         "uses this feature, look at this as an example."));
  m_offset->setFont(
      Font(FontOptions(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain)));
  m_offset->setJustificationType(Justification::centred);
  m_offset->setEditable(true, true, false);
  m_offset->setColour(Label::backgroundColourId, Colours::white);
  m_offset->setColour(Label::outlineColourId, Colours::black);
  m_offset->setColour(TextEditor::textColourId, Colours::black);
  m_offset->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_offset->addListener(this);

  addAndMakeVisible(m_offsetLabel = new Label(String("Offset Label"), String("Offset:")));
  m_offsetLabel->setFont(
      Font(FontOptions(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain)));
  m_offsetLabel->setJustificationType(Justification::centredLeft);
  m_offsetLabel->setEditable(false, false, false);
  m_offsetLabel->setColour(TextEditor::textColourId, Colours::black);
  m_offsetLabel->setColour(TextEditor::backgroundColourId, Colour(0x0));

  //[UserPreSize]
  //[/UserPreSize]

  setSize(600, 50);

  //[Constructor] You can add your own custom stuff here..
  m_pBank = pBank;
  m_pPlugModeBankComponent = pPMBC;
  //[/Constructor]
}

PlugModeBankReferenceComponent::~PlugModeBankReferenceComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(groupComponent2);
  deleteAndZero(m_nameShort);
  deleteAndZero(m_nameLong);
  deleteAndZero(m_groupReference);
  deleteAndZero(m_referenceTo);
  deleteAndZero(m_offset);
  deleteAndZero(m_offsetLabel);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeBankReferenceComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeBankReferenceComponent::resized() {
  groupComponent2->setBounds(0, 0, 280, 50);
  m_nameShort->setBounds(25, 16, 60, 24);
  m_nameLong->setBounds(95, 16, 160, 24);
  m_groupReference->setBounds(332, 0, 270, 50);
  m_referenceTo->setBounds(349, 16, 100, 24);
  m_offset->setBounds(534, 16, 54, 24);
  m_offsetLabel->setBounds(465, 16, 73, 24);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

void PlugModeBankReferenceComponent::labelTextChanged(
    Label *labelThatHasChanged) {
  //[UserlabelTextChanged_Pre]
  //[/UserlabelTextChanged_Pre]

  if (labelThatHasChanged == m_nameShort) {
    //[UserLabelCode_m_nameShort] -- add your label text handling code here..
    bool setLongAlso = (m_pBank->getNameShort() == m_pBank->getNameLong());
    m_nameShort->setText(m_nameShort->getText().substring(0, 6), dontSendNotification);
    if (setLongAlso)
      m_nameLong->setText(m_nameShort->getText(), sendNotification);

    m_pBank->setNameShort(m_nameShort->getText());
    m_pPlugModeBankComponent->updateBankNames();
    //[/UserLabelCode_m_nameShort]
  } else if (labelThatHasChanged == m_nameLong) {
    //[UserLabelCode_m_nameLong] -- add your label text handling code here..
    m_nameLong->setText(m_nameLong->getText().substring(0, 17), dontSendNotification);
    m_pBank->setNameLong(m_nameLong->getText());
    //[/UserLabelCode_m_nameLong]
  } else if (labelThatHasChanged == m_offset) {
    //[UserLabelCode_m_offset] -- add your label text handling code here..
    m_pBank->setParamIDOffset(m_offset->getText().getIntValue());
    //[/UserLabelCode_m_offset]
  }

  //[UserlabelTextChanged_Post]
  //[/UserlabelTextChanged_Post]
}

void PlugModeBankReferenceComponent::comboBoxChanged(
    ComboBox *comboBoxThatHasChanged) {
  //[UsercomboBoxChanged_Pre]
  //[/UsercomboBoxChanged_Pre]

  if (comboBoxThatHasChanged == m_referenceTo) {
    //[UserComboBoxCode_m_referenceTo] -- add your combo box handling code
    //here..
    m_pBank->setReferTo(m_referenceTo->getSelectedItemIndex());
    updateEverything();
    //[/UserComboBoxCode_m_referenceTo]
  }

  //[UsercomboBoxChanged_Post]
  //[/UsercomboBoxChanged_Post]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeBankReferenceComponent::updateEverything() {
  m_nameShort->setText(m_pBank->getNameShort(), dontSendNotification);
  m_nameLong->setText(m_pBank->getNameLong(), dontSendNotification);

  int referTo = m_pBank->referTo();
  if (m_pBank->doesRefer()) {
    m_referenceTo->setSelectedItemIndex(m_pBank->referTo(), dontSendNotification);
    m_offset->setVisible(true);
    m_offsetLabel->setVisible(true);
    m_pPlugModeBankComponent->setTabVisible(false);
    m_offset->setText(String::formatted(String("%d"), m_pBank->getParamIDOffset()),
                      dontSendNotification);
  } else {
    m_referenceTo->setSelectedItemIndex(m_pBank->getId(), dontSendNotification);
    m_offset->setVisible(false);
    m_offsetLabel->setVisible(false);
    m_pPlugModeBankComponent->setTabVisible(true);
  }
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeBankReferenceComponent"
		componentName="" parentClasses="public Component" constructorParams="PlugModeBankComponent* pPMBC, PMBank* pBank"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="1" initialWidth="600"
		initialHeight="50">
		<BACKGROUND backgroundColour="ffffffff"/>
		<GROUPCOMPONENT name="new group" id="350437b4cc297690" memberName="groupComponent2"
		virtualName="" explicitFocusOrder="0" pos="0 0 280 50" outlinecol="0"
		textcol="0" title="Name"/>
		<LABEL name="Name Short" id="253263c1a1416b79" memberName="m_nameShort"
		virtualName="" explicitFocusOrder="0" pos="25 16 60 24" tooltip="This short name of the selected bank"
		bkgCol="ffffffff" outlineCol="ff000000" edTextCol="ff000000"
		edBkgCol="0" labelText="Bank x" editableSingleClick="1" editableDoubleClick="1"
		focusDiscardsChanges="0" fontname="Default monospaced font" fontsize="15"
		bold="0" italic="0" justification="33"/>
		<LABEL name="Name Long" id="85f211a6a04f5249" memberName="m_nameLong"
		virtualName="" explicitFocusOrder="0" pos="95 16 160 24" tooltip="This long name of the selected bank"
		bkgCol="ffffffff" outlineCol="ff000000" edTextCol="ff000000"
		edBkgCol="0" labelText="12345678901234567" editableSingleClick="1"
		editableDoubleClick="1" focusDiscardsChanges="0" fontname="Default monospaced font"
		fontsize="15" bold="0" italic="0" justification="33"/>
		<GROUPCOMPONENT name="Group Reference" id="f0104bbe507446c7" memberName="m_groupReference"
		virtualName="" explicitFocusOrder="0" pos="332 0 270 50" outlinecol="ff000000"
		title="Reference"/>
		<COMBOBOX name="Reference To" id="704535d77dd226ab" memberName="m_referenceTo"
		virtualName="" explicitFocusOrder="0" pos="349 16 100 24" tooltip="Use the page mappings of the selected bank. This can be useful if the plugin has parameter groups, eq. a drum synth that has n-times the same synth engine. In this case you must map the parameters only for one synth engine and can then refer to it."
		editable="0" layout="33" items="Bank 1&#10;Bank 2&#10;Bank 3&#10;Bank 4&#10;Bank 5&#10;Bank 6&#10;Bank 7&#10;Bank 8"
		textWhenNonSelected="something must be selected" textWhenNoItems="we have items"/>
		<LABEL name="Offset" id="232683126c8543fb" memberName="m_offset" virtualName=""
		explicitFocusOrder="0" pos="534 16 54 24" tooltip="The offset is determined by the difference of the ids of repeating parameter groups. The ReaEQ map uses this feature, look at this as an example."
		bkgCol="ffffffff" outlineCol="ff000000" edTextCol="ff000000"
		edBkgCol="0" labelText="-20" editableSingleClick="1" editableDoubleClick="1"
		focusDiscardsChanges="0" fontname="Default monospaced font" fontsize="15"
		bold="0" italic="0" justification="36"/>
		<LABEL name="Offset Label" id="dae7396c6c3947f" memberName="m_offsetLabel"
		virtualName="" explicitFocusOrder="0" pos="465 16 73 24" edTextCol="ff000000"
		edBkgCol="0" labelText="Offset:" editableSingleClick="0" editableDoubleClick="0"
		focusDiscardsChanges="0" fontname="Default monospaced font" fontsize="15"
		bold="0" italic="0" justification="33"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
