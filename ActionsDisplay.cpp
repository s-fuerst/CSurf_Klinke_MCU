/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "ActionsDisplay.h"
#include "DisplayHandler.h"
#include "Assert.h"
#include "csurf_mcu.h"

ActionsDisplay::ActionsDisplay(DisplayHandler* pDH) : Display(pDH, 4),
m_pDisplayHandler(pDH),
m_pOtherDisplay(NULL),
m_shownModifier(0)
{
  readConfigFile();
}

ActionsDisplay::~ActionsDisplay(void)
{
}

void ActionsDisplay::activate(int nr)
{
  switchTo(nr);

  m_pOtherDisplay = m_pDisplayHandler->getDisplay();
  m_pDisplayHandler->switchTo(this);
}

void ActionsDisplay::deactivate()
{
  if (m_pDisplayHandler->getDisplay() == this) {
    m_pDisplayHandler->switchTo(m_pOtherDisplay);
  }
}

void ActionsDisplay::switchTo( int nr )
{
  ASSERT(nr >= 0 && nr < 16);

  m_shownModifier = nr;

  updateDisplay();
}

void ActionsDisplay::updateDisplay() {
  for (int i = 0; i < 4; i++) {
    changeText(0, i * 14, m_strLabel[m_shownModifier][i].toCString(), 13);
    changeText(1, i * 14, m_strLabel[m_shownModifier][i+4].toCString(), 13);
  }
}

String ActionsDisplay::getLabel( int modifiers, int nr )
{
  ASSERT(modifiers >= 0 && modifiers < 16);
  ASSERT(nr >= 0 && nr < 8);
  return m_strLabel[modifiers][nr];
}

void ActionsDisplay::setLabel( int modifiers, int nr, String& newText )
{
  ASSERT(modifiers >= 0 && modifiers < 16);
  ASSERT(nr >= 0 && nr < 8);
  m_strLabel[modifiers][nr] = newText;
  updateDisplay();
}

//-------------------------------------------------------------------
// Read/Write XML files
//-------------------------------------------------------------------

File ActionsDisplay::getConfigFile(boolean bLookAtProgramDir) {
  File configDir = File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() + JUCE_T("\\Reaper\\MCU\\Config\\");
  if (!configDir.exists()/* || !File(configDir.getFullPathName() + JUCE_T("\\GlobalActions.xml")).exists()*/) {
//    if (bLookAtProgramDir) {
//      File configDirDll = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getFullPathName() + JUCE_T("\\Plugins\\MCU\\Config\\");
//      if (configDirDll.exists()) {
//        return File(configDirDll.getFullPathName() + JUCE_T("\\ActionMode.xml")); 
//      }
//    }
    configDir.createDirectory();
  }
#ifdef EXT_B
  return File(configDir.getFullPathName() + JUCE_T("\\GlobalActionsB.xml")); 
#else
  return File(configDir.getFullPathName() + JUCE_T("\\GlobalActions.xml")); 
#endif
}

#define GA_ACTION JUCE_T("ACTION")
#define GA_ATT_MOD JUCE_T("mod")
#define GA_ATT_NR JUCE_T("nr")
#define GA_ATT_LABEL JUCE_T("label")

bool ActionsDisplay::readConfigFile() {
  XmlDocument* pXmlFile = new XmlDocument(getConfigFile(true));
  if (!pXmlFile)
    return false;

  XmlElement* pRootElement = pXmlFile->getDocumentElement();
  if (!pRootElement) 
    return false;

  forEachXmlChildElement (*pRootElement, pNode) {
    int mod = pNode->getIntAttribute(GA_ATT_MOD);
    int nr = pNode->getIntAttribute(GA_ATT_NR);
    m_strLabel[mod][nr] = pNode->getStringAttribute(GA_ATT_LABEL);
  } 

  delete(pRootElement);  
  delete(pXmlFile);
  return true;
}

void ActionsDisplay::writeConfigFile() {
  XmlElement* pRootElement = new XmlElement("GLOBAL_ACTIONS_CONFIG");

  for (int mod = 0; mod < 16; mod++) {
    for (int nr = 0; nr < 8; nr++) {
      XmlElement* pNode = new XmlElement(GA_ACTION);
      pNode->setAttribute(GA_ATT_MOD, mod);
      pNode->setAttribute(GA_ATT_NR, nr);
      pNode->setAttribute(GA_ATT_LABEL, m_strLabel[mod][nr]);
      pRootElement->addChildElement(pNode);
    }
  }
 
  pRootElement->writeToFile(getConfigFile(false), "", JUCE_T("UTF-8"));

  delete(pRootElement);
}
