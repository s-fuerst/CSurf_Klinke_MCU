/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  15 Dec 2009 12:19:27 am

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
#include "PlugAccess.h"
#include "PlugModeComponent.h"
#include "PlugMode.h"
//[/Headers]

#include "PlugModeParamComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
#define PARAM_COMP_ID_OFFSET 10
//[/MiscUserDefs]

//==============================================================================
PlugModeParamComponent::PlugModeParamComponent(PlugModeComponent *pMC,
                                               PMParam *pParam)
    : m_nameShort(0), m_parameter(0), m_nameLong(0) {
  addAndMakeVisible(m_nameShort = new Label(String("Name Short"), String("FilCut")));
  m_nameShort->setTooltip(String("This short name of the selected parameter"));
  m_nameShort->setFont(
      Font(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain));
  m_nameShort->setJustificationType(Justification::centredLeft);
  m_nameShort->setEditable(true, true, false);
  m_nameShort->setColour(Label::backgroundColourId, Colours::white);
  m_nameShort->setColour(Label::outlineColourId, Colours::black);
  m_nameShort->setColour(TextEditor::textColourId, Colours::black);
  m_nameShort->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_nameShort->addListener(this);

  addAndMakeVisible(m_parameter = new ComboBox(String("Parameter")));
  m_parameter->setTooltip(
      String("The parameter that is controlled by this fader/V-Pot"));
  m_parameter->setEditableText(false);
  m_parameter->setJustificationType(Justification::centredLeft);
  m_parameter->setTextWhenNothingSelected(String("unknown"));
  m_parameter->setTextWhenNoChoicesAvailable(String("Plug has no parameters"));
  m_parameter->addItem(String("unknown"), 1);
  m_parameter->addListener(this);

  addAndMakeVisible(m_nameLong =
                        new Label(String("Name Long"), String("12345678901234567")));
  m_nameLong->setTooltip(String("This long name of the selected parameter"));
  m_nameLong->setFont(
      Font(Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain));
  m_nameLong->setJustificationType(Justification::centredLeft);
  m_nameLong->setEditable(true, true, false);
  m_nameLong->setColour(Label::backgroundColourId, Colours::white);
  m_nameLong->setColour(Label::outlineColourId, Colours::black);
  m_nameLong->setColour(TextEditor::textColourId, Colours::black);
  m_nameLong->setColour(TextEditor::backgroundColourId, Colour(0x0));
  m_nameLong->addListener(this);

  //[UserPreSize]
  //[/UserPreSize]

  setSize(468, 28);

  //[Constructor] You can add your own custom stuff here..
  m_pParam = pParam;
  m_pMainComponent = pMC;
  //[/Constructor]
}

PlugModeParamComponent::~PlugModeParamComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  //[/Destructor_pre]

  deleteAndZero(m_nameShort);
  deleteAndZero(m_parameter);
  deleteAndZero(m_nameLong);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeParamComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeParamComponent::resized() {
  m_nameShort->setBounds(7, 4, 60, 24);
  m_parameter->setBounds(300, 4, 268, 24);
  m_nameLong->setBounds(77, 4, 160, 24);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

void PlugModeParamComponent::labelTextChanged(Label *labelThatHasChanged) {
  //[UserlabelTextChanged_Pre]
  //[/UserlabelTextChanged_Pre]

  if (labelThatHasChanged == m_nameShort) {
    //[UserLabelCode_m_nameShort] -- add your label text handling code here..
    bool setLongAlso = (m_pParam->getNameShort() == m_pParam->getNameLong());
    m_nameShort->setText(m_nameShort->getText().substring(0, 6), dontSendNotification);
    if (setLongAlso)
      m_nameLong->setText(m_nameShort->getText(), sendNotification);

    m_pParam->setNameShort(m_nameShort->getText());
    //[/UserLabelCode_m_nameShort]
  } else if (labelThatHasChanged == m_nameLong) {
    //[UserLabelCode_m_nameLong] -- add your label text handling code here..
    m_nameLong->setText(m_nameLong->getText().substring(0, 17), dontSendNotification);
    m_pParam->setNameLong(m_nameLong->getText());
    //[/UserLabelCode_m_nameLong]
  }

  //[UserlabelTextChanged_Post]
  //[/UserlabelTextChanged_Post]
}

void PlugModeParamComponent::comboBoxChanged(ComboBox *comboBoxThatHasChanged) {
  //[UsercomboBoxChanged_Pre]
  //[/UsercomboBoxChanged_Pre]

  if (comboBoxThatHasChanged == m_parameter) {
    //[UserComboBoxCode_m_parameter] -- add your combo box handling code here..
    changeParamId(m_parameter->getSelectedId() - PARAM_COMP_ID_OFFSET);

    //[/UserComboBoxCode_m_parameter]
  }

  //[UsercomboBoxChanged_Post]
  //[/UsercomboBoxChanged_Post]
}

void PlugModeParamComponent::mouseDown(const MouseEvent &e) {
  //[UserCode_mouseDown] -- Add your code here...
  m_pMainComponent->paramComponentPressed(this);
  //[/UserCode_mouseDown]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
//other code here...
void PlugModeParamComponent::updateEverything() {
  updateParameterList();

  m_nameShort->setText(m_pParam->getNameShort(), dontSendNotification);
  m_nameLong->setText(m_pParam->getNameLong(), dontSendNotification);

  m_pMainComponent->updateLearnStatus();
}

void PlugModeParamComponent::updateParameterList() {
  m_parameter->clear();
  m_parameter->addItem(String("No Parameter selected"),
                       NOT_ASSIGNED + PARAM_COMP_ID_OFFSET);
  PlugAccess *pPA = m_pMainComponent->getPlugAccess();
  if (pPA == NULL)
    return;
  int numParams = pPA->getNumParams();
  for (int i = 0; i < numParams; i++) {
    char paramName[80];
    bool valid = TrackFX_GetParamName(pPA->getPlugTrack(), pPA->getPlugSlot(),
                                      i, paramName, 79);
    if (valid && (strnlen(paramName, 80) > 0)) {
      m_parameter->addItem(String::formatted(String("%04d: "), i + 1) + paramName,
                           i + PARAM_COMP_ID_OFFSET);
    } else {
      m_parameter->addItem(String::formatted(String("%04d"), i + 1),
                           i + PARAM_COMP_ID_OFFSET);
    }
  }

  m_parameter->setSelectedId(m_pParam->getParamID() + PARAM_COMP_ID_OFFSET);
}

void PlugModeParamComponent::changeParamId(int paramId) {
  if (paramId == m_pParam->getParamID())
    return;

  m_pParam->setParamID(paramId);
  m_parameter->setSelectedId(paramId + PARAM_COMP_ID_OFFSET, true);
  if (m_pMainComponent->isUseParamName()) {
    if (paramId == NOT_ASSIGNED) {
      m_pParam->setNameShort(String());
      m_pParam->setNameLong(String());
      updateEverything();
    } else {
      PlugAccess *pPA = m_pMainComponent->getPlugAccess();
      if (pPA) {
        char paramName[80];
        bool valid =
            TrackFX_GetParamName(pPA->getPlugTrack(), pPA->getPlugSlot(),
                                 m_pParam->getParamID(), paramName, 79);
        if (valid) {
          m_pParam->setNameShort(PlugAccess::shortNameFromCString(paramName));
          m_pParam->setNameLong(PlugAccess::longNameFromCString(paramName));
          updateEverything();
        }
      }
    }
  }

  if (dynamic_cast<PMVPot *>(m_pParam)) {
    dynamic_cast<PMVPot *>(m_pParam)->getStepsMap()->clear();
    m_pMainComponent->updateEverything();
  }
}

void PlugModeParamComponent::setLearn(bool learn) {
  m_parameter->setColour(ComboBox::backgroundColourId,
                         learn ? Colour(255, 0, 0) : Colour(255, 255, 255));
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeParamComponent" componentName=""
		parentClasses="public Component" constructorParams="PlugModeComponent* pMC, PMParam* pParam"
		variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
		overlayOpacity="0.330000013" fixedSize="0" initialWidth="468"
		initialHeight="28">
		<METHODS>
    <METHOD name="mouseDown (const MouseEvent&amp; e)"/>
		</METHODS>
		<BACKGROUND backgroundColour="ffffffff"/>
		<LABEL name="Name Short" id="6012381fb70017e2" memberName="m_nameShort"
		virtualName="" explicitFocusOrder="0" pos="7 4 60 24" tooltip="This short name of the selected parameter"
		bkgCol="ffffffff" outlineCol="ff000000" edTextCol="ff000000"
		edBkgCol="0" labelText="FilCut" editableSingleClick="1" editableDoubleClick="1"
		focusDiscardsChanges="0" fontname="Default monospaced font" fontsize="15"
		bold="0" italic="0" justification="33"/>
		<COMBOBOX name="Parameter" id="704535d77dd226ab" memberName="m_parameter"
		virtualName="" explicitFocusOrder="0" pos="300 4 268 24" tooltip="The parameter that is controlled by this fader/V-Pot"
		editable="0" layout="33" items="unknown" textWhenNonSelected="unknown"
		textWhenNoItems="Plug has no parameters"/>
		<LABEL name="Name Long" id="6ef07a6c8efd5610" memberName="m_nameLong"
		virtualName="" explicitFocusOrder="0" pos="77 4 160 24" tooltip="This long name of the selected parameter"
		bkgCol="ffffffff" outlineCol="ff000000" edTextCol="ff000000"
		edBkgCol="0" labelText="12345678901234567" editableSingleClick="1"
		editableDoubleClick="1" focusDiscardsChanges="0" fontname="Default monospaced font"
		fontsize="15" bold="0" italic="0" justification="33"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
