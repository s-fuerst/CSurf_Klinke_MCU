/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  29 Aug 2010 9:53:47 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.12

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
#include "PlugMode.h"
#include "PluginWatcher.h"
#include "boost/bind.hpp"
#include "PlugMapManager.h"
//[/Headers]

#include "PlugModeComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
#include "PlugMapSaveDialog.h"
//[/MiscUserDefs]

//==============================================================================
PlugModeComponent::PlugModeComponent(PlugAccess *pPA)
    : m_pPlugAccess(0), m_groupFile(0), m_mappingFile(0), m_save(0),
      m_autosave(0), m_bankComponent(0), m_learn(0), m_saveAs(0),
      m_useParamName(0), m_clear(0), m_local(0) {
  addAndMakeVisible(
      m_groupFile = new GroupComponent(String("mapping file"), String("mapping file")));
  m_groupFile->setColour(GroupComponent::outlineColourId, Colours::black);

  addAndMakeVisible(m_mappingFile =
                        new Label(String("Mapping File"), String("no mapping file \n")));
  m_mappingFile->setTooltip(String("The used mapping file"));
  m_mappingFile->setFont(Font(15.0000f, Font::plain));
  m_mappingFile->setJustificationType(Justification::centredLeft);
  m_mappingFile->setEditable(false, false, false);
  m_mappingFile->setColour(TextEditor::textColourId, Colours::black);
  m_mappingFile->setColour(TextEditor::backgroundColourId, Colour(0x0));

  addAndMakeVisible(m_save = new TextButton(String("Save")));
  m_save->setTooltip(String("Save the current mapping to the current file"));
  m_save->setExplicitFocusOrder(6);
  m_save->addListener(this);

  addAndMakeVisible(m_autosave = new ToggleButton(String("Autosave")));
  m_autosave->setTooltip(String("Save the mapping file automatical when the editor "
                           "get closed or another mapping file is loaded."));
  m_autosave->setExplicitFocusOrder(8);
  m_autosave->addListener(this);

  addAndMakeVisible(m_bankComponent =
                        new PlugModeBankComponent(pPA->getMap(), this));
  m_bankComponent->setExplicitFocusOrder(9);
  addAndMakeVisible(m_learn = new ToggleButton(String("Learn")));
  m_learn->setTooltip(
      String("If enabled, you can assign a parameter to a fader or vpot by changing "
        "the parameter value (e.g. in the plugin user interface)"));
  m_learn->setExplicitFocusOrder(3);
  m_learn->addListener(this);

  addAndMakeVisible(m_saveAs = new TextButton(String("SaveAs")));
  m_saveAs->setTooltip(String("Save the current mapping to a new file"));
  m_saveAs->setExplicitFocusOrder(7);
  m_saveAs->setButtonText(String("As"));
  m_saveAs->addListener(this);

  addAndMakeVisible(m_useParamName = new ToggleButton(String("Use Param Name")));
  m_useParamName->setTooltip(
      String("If enabled and a parameter assignment gets changed then the user "
        "given parameter names will be overwritten with the names as given "
        "from the FX."));
  m_useParamName->setExplicitFocusOrder(4);
  m_useParamName->setButtonText(String("Autoname"));
  m_useParamName->addListener(this);

  addAndMakeVisible(m_clear = new TextButton(String("Clear")));
  m_clear->setTooltip(String("Clear the parameter map"));
  m_clear->setExplicitFocusOrder(2);
  m_clear->addListener(this);

  addAndMakeVisible(m_local = new ToggleButton(String("Local")));
  m_local->setTooltip(
      String("If enabled, the map is bind to the actual selected plugin. This "
        "allows to create different map for different instances of the same "
        "plugin. Eq. it possible to use the IX/Mixer script and name the "
        "channels to mix for each instance."));
  m_local->setExplicitFocusOrder(5);
  m_local->addListener(this);

  //[UserPreSize]
  //[/UserPreSize]

  setSize(620, 524);

  //[Constructor] You can add your own custom stuff here..
  m_pPlugAccess = pPA;
  updateEverything();
  m_learnFader = true;
  m_useParamName->setToggleState(true, false);
  //[/Constructor]
}

PlugModeComponent::~PlugModeComponent() {
  //[Destructor_pre]. You can add your own custom destruction code here..
  if (m_paramChangedConnectionId)
    m_pPlugAccess->getPlugWatcher()->disconnectParamChange(
        m_paramChangedConnectionId);
  //[/Destructor_pre]

  deleteAndZero(m_groupFile);
  deleteAndZero(m_mappingFile);
  deleteAndZero(m_save);
  deleteAndZero(m_autosave);
  deleteAndZero(m_bankComponent);
  deleteAndZero(m_learn);
  deleteAndZero(m_saveAs);
  deleteAndZero(m_useParamName);
  deleteAndZero(m_clear);
  deleteAndZero(m_local);

  //[Destructor]. You can add your own custom destruction code here..
  //[/Destructor]
}

//==============================================================================
void PlugModeComponent::paint(Graphics &g) {
  //[UserPrePaint] Add your own custom painting code here..
  //[/UserPrePaint]

  g.fillAll(Colours::white);

  //[UserPaint] Add your own custom painting code here..
  //[/UserPaint]
}

void PlugModeComponent::resized() {
  m_groupFile->setBounds(221, 5, 395, 50);
  m_mappingFile->setBounds(293, 22, 140, 24);
  m_save->setBounds(438, 22, 45, 24);
  m_autosave->setBounds(528, 22, 83, 24);
  m_bankComponent->setBounds(0, 68, 620, 464);
  m_learn->setBounds(65, 22, 60, 24);
  m_saveAs->setBounds(484, 22, 45, 24);
  m_useParamName->setBounds(130, 22, 90, 24);
  m_clear->setBounds(13, 22, 45, 24);
  m_local->setBounds(227, 22, 60, 24);
  //[UserResized] Add your own custom resize handling here..
  //[/UserResized]
}

void PlugModeComponent::buttonClicked(Button *buttonThatWasClicked) {
  //[UserbuttonClicked_Pre]
  //[/UserbuttonClicked_Pre]

  if (buttonThatWasClicked == m_save) {
    //[UserButtonCode_m_save] -- add your button handler code here..
    m_pPlugAccess->getMapManager()->rewriteMapFile();
    //[/UserButtonCode_m_save]
  } else if (buttonThatWasClicked == m_autosave) {
    //[UserButtonCode_m_autosave] -- add your button handler code here..
    m_pPlugAccess->getMapManager()->setAutoSave(m_autosave->getToggleState());
    //[/UserButtonCode_m_autosave]
  } else if (buttonThatWasClicked == m_learn) {
    //[UserButtonCode_m_learn] -- add your button handler code here..
    if (m_learn->getToggleState()) {
      m_paramChangedConnectionId =
          m_pPlugAccess->getPlugWatcher()->connect2ParamChanged(
              boost::bind(&PlugModeComponent::watchedPluginParameterChanged,
                          this, _1, _2, _3, _4, _5));
    } else {
      m_pPlugAccess->getPlugWatcher()->disconnectParamChange(
          m_paramChangedConnectionId);
      m_paramChangedConnectionId = 0;
    }
    updateLearnStatus();
    //[/UserButtonCode_m_learn]
  } else if (buttonThatWasClicked == m_saveAs) {
    //[UserButtonCode_m_saveAs] -- add your button handler code here..
    PlugMapManager *pMM = m_pPlugAccess->getMapManager();

    DialogWindow *pDialog =
        new SaveDialogWindow("Save Map", Colours::white, true, true);
    pMM->rescanDirs();
    pDialog->setContentComponent(new PlugMapSaveDialog(pMM), false, true);
    pDialog->setVisible(true);
    pDialog->setAlwaysOnTop(true);
    pDialog->runModalLoop();
    delete pDialog;

    updateEverything();
    //[/UserButtonCode_m_saveAs]
  } else if (buttonThatWasClicked == m_useParamName) {
    //[UserButtonCode_m_useParamName] -- add your button handler code here..
    //[/UserButtonCode_m_useParamName]
  } else if (buttonThatWasClicked == m_clear) {
    //[UserButtonCode_m_clear] -- add your button handler code here..
    m_pPlugAccess->getMapManager()->initMap();
    updateEverything();
    //[/UserButtonCode_m_clear]
  } else if (buttonThatWasClicked == m_local) {
    //[UserButtonCode_m_local] -- add your button handler code here..
    m_pPlugAccess->getMapManager()->setLocalMap(m_local->getToggleState());
    updateEverything();
    //[/UserButtonCode_m_local]
  }

  //[UserbuttonClicked_Post]
  //[/UserbuttonClicked_Post]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any
// other code here...

void PlugModeComponent::changePlug(PlugAccess* pPA) {
  m_pPlugAccess = pPA;
  updateEverything();
}

void PlugModeComponent::updateEverything() {
  selectedBankChanged(m_pPlugAccess->getSelectedBank());
  selectedPageChanged(m_pPlugAccess->getSelectedPageInSelectedBank());

  m_bankComponent->updateEverything();

  PlugMapManager::eMapType mapType =
      m_pPlugAccess->getMapManager()->getMapType();

  if (m_pPlugAccess->getMapManager()->getMapName().isEmpty() &&
      mapType != PlugMapManager::LOCAL_MAP) {
    m_mappingFile->setText(String("no mapping file"), dontSendNotification);
  } else {
    if (mapType == PlugMapManager::USER_MAP)
      m_mappingFile->setText(m_pPlugAccess->getMapManager()->getMapName(),
                             dontSendNotification);
    else if (mapType == PlugMapManager::INSTALLED_MAP)
      m_mappingFile->setText(m_pPlugAccess->getMapManager()->getMapName() +
                                 String(" (factory)"),
                             dontSendNotification);
    else
      m_mappingFile->setText(String(""), dontSendNotification);
  }

  m_save->setEnabled(mapType == PlugMapManager::USER_MAP);

  m_saveAs->setEnabled(mapType != PlugMapManager::LOCAL_MAP);
  m_autosave->setEnabled(mapType != PlugMapManager::LOCAL_MAP);

  m_local->setToggleState(m_pPlugAccess->getMapManager()->getMapType() ==
                              PlugMapManager::LOCAL_MAP,
                          false);

  m_autosave->setToggleState(m_pPlugAccess->getMapManager()->getAutoSave(),
                             false);

  updateLearnStatus();
}

void PlugModeComponent::selectedBankChanged(int iBank) {
  m_bankComponent->selectedBankChanged(iBank);
  selectedPageChanged(m_pPlugAccess->getSelectedPageInSelectedBank());

  updateLearnStatus();
}

void PlugModeComponent::selectedPageChanged(int iPage) {
  // The active unit may show no page (empty unit) → -1; the page component
  // cannot select a tab with a negative index.
  if (iPage < 0)
    iPage = 0;
  safe_call(m_bankComponent->getSelectedBankComponent(),
            getPageComponent()->selectedPageChanged(iPage))

      updateLearnStatus();
}

void PlugModeComponent::selectedChannelChanged(int iChannel, bool fader) {
  if (m_bankComponent->getSelectedBankComponent() &&
      m_bankComponent->getSelectedBankComponent()->getSelectedPageComponent() &&
      m_bankComponent->getSelectedBankComponent()
          ->getSelectedPageComponent()
          ->getChannelComponent()) {
    m_bankComponent->getSelectedBankComponent()
        ->getSelectedPageComponent()
        ->getChannelComponent()
        ->selectedChannelChanged(iChannel);
  }

  m_learnFader = fader;

  updateLearnStatus();
}

void PlugModeComponent::selectedPluginChanged(MediaTrack *pMediaTrack,
                                              int iSlot) {
  updateEverything();
}

void PlugModeComponent::watchedPluginParameterChanged(MediaTrack *pMediaTrack,
                                                      int iSlot, int iParameter,
                                                      double dValue,
                                                      String strValue) {
  if (m_learn->getToggleState() &&
      m_bankComponent->getSelectedBankComponent()) {
    PlugModeSingleChannelComponent *pChannel =
        m_bankComponent->getSelectedBankComponent()
            ->getSelectedPageComponent()
            ->getSelectedChannelComponent();
    if (m_learnFader) {
      pChannel->getFader()->getParamComponent()->changeParamId(iParameter);
    } else {
      pChannel->getVPOT()->changeParamId(iParameter, dValue, strValue);
    }
  }
}

void PlugModeComponent::updateLearnStatus() {
  if (m_bankComponent) {
    PlugModeSingleBankComponent *bank =
        m_bankComponent->getSelectedBankComponent();
    if (bank) {
      PlugModeSinglePageComponent *page = bank->getSelectedPageComponent();
      if (page) {
        PlugModeSingleChannelComponent *pChannel =
            page->getSelectedChannelComponent();
        if (pChannel) {
          pChannel->getFader()->getParamComponent()->setLearn(
              m_learn->getToggleState() && m_learnFader);
          pChannel->getVPOT()->getParamComponent()->setLearn(
              m_learn->getToggleState() && !m_learnFader);
        }
      }
    }
  }
}

void PlugModeComponent::paramComponentPressed(PlugModeParamComponent *pPC) {
  PlugModeSingleChannelComponent *pChannel =
      m_bankComponent->getSelectedBankComponent()
          ->getSelectedPageComponent()
          ->getSelectedChannelComponent();
  if (pChannel) {
    m_learnFader = (pChannel->getFader()->getParamComponent() == pPC);

    updateLearnStatus();
  }
}
//[/MiscUserCode]

//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

		BEGIN_JUCER_METADATA

		<JUCER_COMPONENT documentType="Component" className="PlugModeComponent" componentName=""
		parentClasses="public Component" constructorParams="PlugAccess* pPA"
		variableInitialisers="m_pPlugAccess(0)" snapPixels="8" snapActive="1"
		snapShown="1" overlayOpacity="0.330000013" fixedSize="1" initialWidth="620"
		initialHeight="524">
		<BACKGROUND backgroundColour="ffffffff"/>
		<GROUPCOMPONENT name="mapping file" id="b89d9ec4d1f807cc" memberName="m_groupFile"
		virtualName="" explicitFocusOrder="0" pos="221 5 395 50" outlinecol="ff000000"
		title="mapping file"/>
		<LABEL name="Mapping File" id="145c67a7941f7d0b" memberName="m_mappingFile"
		virtualName="" explicitFocusOrder="0" pos="293 22 140 24" tooltip="The used mapping file"
		edTextCol="ff000000" edBkgCol="0" labelText="no mapping file &#10;"
		editableSingleClick="0" editableDoubleClick="0" focusDiscardsChanges="0"
		fontname="Default font" fontsize="15" bold="0" italic="0" justification="33"/>
		<TEXTBUTTON name="Save" id="ee6c9fa4dead0f64" memberName="m_save" virtualName=""
		explicitFocusOrder="6" pos="438 22 45 24" tooltip="Save the current mapping to the current file"
		buttonText="Save" connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		<TOGGLEBUTTON name="Autosave" id="2796bf07d8e27611" memberName="m_autosave"
		virtualName="" explicitFocusOrder="8" pos="528 22 83 24" tooltip="Save the mapping file automatical when the editor get closed or another mapping file is loaded."
		buttonText="Autosave" connectedEdges="0" needsCallback="1" radioGroupId="0"
		state="0"/>
		<JUCERCOMP name="Bank Component" id="cae73bd070feaa77" memberName="m_bankComponent"
		virtualName="" explicitFocusOrder="9" pos="0 68 620 464" sourceFile="PlugModeBankComponent.cpp"
		constructorParams="pPA-&gt;getMap(), this"/>
		<TOGGLEBUTTON name="Learn" id="7ac7803fa538a4f5" memberName="m_learn" virtualName=""
		explicitFocusOrder="3" pos="65 22 60 24" tooltip="If enabled, you can assign a parameter to a fader or vpot by changing the parameter value (e.g. in the plugin user interface)"
		buttonText="Learn" connectedEdges="0" needsCallback="1" radioGroupId="0"
		state="0"/>
		<TEXTBUTTON name="SaveAs" id="bd1a9793f16081aa" memberName="m_saveAs" virtualName=""
		explicitFocusOrder="7" pos="484 22 45 24" tooltip="Save the current mapping to a new file"
		buttonText="As" connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		<TOGGLEBUTTON name="Use Param Name" id="add64b8160f57f06" memberName="m_useParamName"
		virtualName="" explicitFocusOrder="4" pos="130 22 90 24" tooltip="If enabled and a parameter assignment gets changed then the user given parameter names will be overwritten with the names as given from the FX."
		buttonText="Autoname" connectedEdges="0" needsCallback="1" radioGroupId="0"
		state="0"/>
		<TEXTBUTTON name="Clear" id="42df879b01c427bb" memberName="m_clear" virtualName=""
		explicitFocusOrder="2" pos="13 22 45 24" tooltip="Clear the parameter map"
		buttonText="Clear" connectedEdges="0" needsCallback="1" radioGroupId="0"/>
		<TOGGLEBUTTON name="Local" id="177ed2b6145410b8" memberName="m_local" virtualName=""
		explicitFocusOrder="5" pos="227 22 60 24" tooltip="If enabled, the map is bind to the actual selected plugin. This allows to create different map for different instances of the same plugin. Eq. it possible to use the IX/Mixer script and name the channels to mix for each instance."
		buttonText="Local" connectedEdges="0" needsCallback="1" radioGroupId="0"
		state="0"/>
		</JUCER_COMPONENT>

		END_JUCER_METADATA
*/
#endif
