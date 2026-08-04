/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ActionsDisplay.h"
#include "DisplayHandler.h"
#include "McuAssert.h"
#include "csurf_mcu.h"
#include "ConfigPath.h"

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
    changeText(0, i * 14, m_strLabel[m_shownModifier][i].toRawUTF8(), 13);
    changeText(1, i * 14, m_strLabel[m_shownModifier][i+4].toRawUTF8(), 13);
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

File ActionsDisplay::getConfigFile() {
  // P2: shared cross-platform helper (matches Options::getConfigFile()).
  return getMcuConfigFile("GlobalActions.xml");
}

#define GA_ACTION String("ACTION")
#define GA_ATT_MOD String("mod")
#define GA_ATT_NR String("nr")
#define GA_ATT_LABEL String("label")
#define GA_ATT_VERSION String("version")

bool ActionsDisplay::readConfigFile() {
  XmlDocument* pXmlFile = new XmlDocument(getConfigFile());
  if (!pXmlFile)
    return false;

  auto docRoot = pXmlFile->getDocumentElement();
  XmlElement* pRootElement = docRoot.get();
  if (!pRootElement) 
    return false;

  for (auto* pNode : pRootElement->getChildIterator()) {
    if (pNode->getTagName() != GA_ACTION)
      continue;
    int mod = pNode->getIntAttribute(GA_ATT_MOD);
    int nr = pNode->getIntAttribute(GA_ATT_NR);
    if (mod < 0 || mod >= 16 || nr < 0 || nr >= 8)
      continue;
    m_strLabel[mod][nr] = pNode->getStringAttribute(GA_ATT_LABEL);
  } 

  delete(pXmlFile);
  return true;
}

void ActionsDisplay::writeConfigFile() {
  XmlElement* pRootElement = new XmlElement("GLOBAL_ACTIONS_CONFIG");
  pRootElement->setAttribute(GA_ATT_VERSION, 1); // P1: version attr for future migration

  for (int mod = 0; mod < 16; mod++) {
    for (int nr = 0; nr < 8; nr++) {
      XmlElement* pNode = new XmlElement(GA_ACTION);
      pNode->setAttribute(GA_ATT_MOD, mod);
      pNode->setAttribute(GA_ATT_NR, nr);
      pNode->setAttribute(GA_ATT_LABEL, m_strLabel[mod][nr]);
      pRootElement->addChildElement(pNode);
    }
  }
 
  pRootElement->writeTo(getConfigFile());

  delete(pRootElement);
}
