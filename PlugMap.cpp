/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "boost\foreach.hpp"
#include "PlugMap.h"

//////////////////////////////////////////////////
// PMParam
//////////////////////////////////////////////////

#define PMAP_ATT_SNAME JUCE_T("short")
#define PMAP_ATT_LNAME JUCE_T("long")
#define PMAP_ATT_PARAMID JUCE_T("paramId")

void PMParam::initValues() {
  m_nameShort = String::empty;
  m_nameLong = String::empty;

  m_paramID = NOT_ASSIGNED;
}

void PMParam::writeToXml(XmlElement *pElement) {
  if (m_nameShort.containsNonWhitespaceChars()) {
    pElement->setAttribute(PMAP_ATT_SNAME, m_nameShort);
  }
  if (m_nameLong.containsNonWhitespaceChars()) {
    pElement->setAttribute(PMAP_ATT_LNAME, m_nameLong);
  }
  pElement->setAttribute(PMAP_ATT_PARAMID, m_paramID);
}

bool PMParam::readFromXml(XmlElement *pElement) {
  if (!pElement)
    return false;

  m_nameShort = pElement->getStringAttribute(PMAP_ATT_SNAME);
  m_nameLong = pElement->getStringAttribute(PMAP_ATT_LNAME);
  m_paramID = pElement->getIntAttribute(PMAP_ATT_PARAMID);

  return true;
}

//////////////////////////////////////////////////
// PMFader
//////////////////////////////////////////////////

#define PMAP_NODE_FADER JUCE_T("FADER")

void PMFader::writeToXml(XmlElement *pElement) {
  XmlElement *pFaderNode = new XmlElement(PMAP_NODE_FADER);
  pElement->addChildElement(pFaderNode);
  PMParam::writeToXml(pFaderNode);
}

bool PMFader::readFromXml(XmlElement *pElement) {
  return PMParam::readFromXml(pElement);
}

//////////////////////////////////////////////////
// PMVPot
//////////////////////////////////////////////////

#define PMAP_NODE_VPOT JUCE_T("VPOT")
#define PMAP_NODE_STEP JUCE_T("STEP")
#define PMAP_ATT_STEPVALUE JUCE_T("value")

PMVPot::PMVPot() { m_pStepsMap = new tSteps(); }

PMVPot::~PMVPot() { safe_delete(m_pStepsMap); }

void PMVPot::setParamID(int newID) {
  if (newID != m_paramID) {
    m_pStepsMap->clear();
  }
  PMParam::setParamID(newID);
}

void PMVPot::writeToXml(XmlElement *pElement) {
  XmlElement *pVPotNode = new XmlElement(PMAP_NODE_VPOT);
  pElement->addChildElement(pVPotNode);
  PMParam::writeToXml(pVPotNode);

  for (tSteps::const_iterator iterSteps = m_pStepsMap->begin();
       iterSteps != m_pStepsMap->end(); ++iterSteps) {
    XmlElement *pStepNode = new XmlElement(PMAP_NODE_STEP);
    pVPotNode->addChildElement(pStepNode);
    pStepNode->setAttribute(PMAP_ATT_STEPVALUE, (*iterSteps).first);
    pStepNode->setAttribute(PMAP_ATT_SNAME, (*iterSteps).second.get<0>());
    pStepNode->setAttribute(PMAP_ATT_LNAME, (*iterSteps).second.get<1>());
  }
}

bool PMVPot::readFromXml(XmlElement *pElement) {
  if (!pElement)
    return false;

  if (!PMParam::readFromXml(pElement))
    return false;

  forEachXmlChildElement(*pElement, pStep) {
    double val = pStep->getDoubleAttribute(PMAP_ATT_STEPVALUE);
    String shortName = pStep->getStringAttribute(PMAP_ATT_SNAME);
    String longName = pStep->getStringAttribute(PMAP_ATT_LNAME);
    m_pStepsMap->insert(
        tSteps::value_type(val, tStepsValue(shortName, longName)));
  }

  return true;
}

void PMVPot::initValues() {
  PMParam::initValues();
  m_pStepsMap->clear();
}
//////////////////////////////////////////////////
// PMPage
//////////////////////////////////////////////////

#define PMAP_NODE_PAGE JUCE_T("PAGE")
#define PMAP_ATT_ID JUCE_T("id")
#define PMAP_ATT_REFER_TO JUCE_T("referTo")
#define PMAP_ATT_OFFSET JUCE_T("offset")

void PMPage::initValues(int id) {
  m_id = id;
  m_nameShort = String::empty;
  m_nameLong = String::empty;
  m_referTo = DOESNT_REFER;
  m_offset = 0;

  BOOST_FOREACH (PMFader &fader, m_faders) { fader.initValues(); }

  BOOST_FOREACH (PMVPot &vpot, m_vpots) { vpot.initValues(); }
}

bool PMPage::isUsed() {
  if (doesRefer()) {
    return true;
  }

  for (int i = 0; i < 8; i++) {
    if (m_faders[i].getParamID() != NOT_ASSIGNED)
      return true;
    if (m_vpots[i].getParamID() != NOT_ASSIGNED)
      return true;
  }
  return false;
}

void PMPage::writeToXml(XmlElement *pElement) {
  XmlElement *pPageNode = new XmlElement(PMAP_NODE_PAGE);
  pElement->addChildElement(pPageNode);
  pPageNode->setAttribute(PMAP_ATT_ID, m_id);
  pPageNode->setAttribute(PMAP_ATT_SNAME, m_nameShort);
  pPageNode->setAttribute(PMAP_ATT_LNAME, m_nameLong);
  pPageNode->setAttribute(PMAP_ATT_REFER_TO, m_referTo);
  pPageNode->setAttribute(PMAP_ATT_OFFSET, m_offset);

  BOOST_FOREACH (PMFader &fader, m_faders)
    fader.writeToXml(pPageNode);

  BOOST_FOREACH (PMVPot &vpot, m_vpots)
    vpot.writeToXml(pPageNode);
}

bool PMPage::readFromXml(XmlElement *pPageNode) {
  if (!pPageNode || pPageNode->getTagName() != PMAP_NODE_PAGE)
    return false;

  m_nameShort = pPageNode->getStringAttribute(PMAP_ATT_SNAME);
  m_nameLong = pPageNode->getStringAttribute(PMAP_ATT_LNAME);
  m_referTo = pPageNode->getIntAttribute(PMAP_ATT_REFER_TO);
  m_offset = pPageNode->getIntAttribute(PMAP_ATT_OFFSET);

  XmlElement *pChild = pPageNode->getFirstChildElement();
  BOOST_FOREACH (PMFader &fader, m_faders) {
    if (!fader.readFromXml(pChild)) {
      return false;
    }
    pChild = pChild->getNextElement();
  }

  BOOST_FOREACH (PMVPot &vpot, m_vpots) {
    if (!vpot.readFromXml(pChild)) {
      return false;
    }
    pChild = pChild->getNextElement();
  }

  return true;
}
//////////////////////////////////////////////////
// PMBank
//////////////////////////////////////////////////

#define PMAP_NODE_BANK JUCE_T("BANK")

void PMBank::initValues(int id) {
  m_id = id;
  m_nameShort = String::empty;
  m_nameLong = String::empty;
  m_referTo = DOESNT_REFER;
  m_offset = 0;

  for (int i = 0; i < 8; i++) {
    m_pages[i].initValues(i);
  }
}

bool PMBank::isUsed() {
  if (doesRefer()) {
    return true;
  }

  BOOST_FOREACH (PMPage &page, m_pages) {
    if (page.isUsed())
      return true;
  }

  return false;
}

void PMBank::writeToXml(XmlElement *pElement) {
  XmlElement *pBankNode = new XmlElement(PMAP_NODE_BANK);
  pElement->addChildElement(pBankNode);
  pBankNode->setAttribute(PMAP_ATT_ID, m_id);
  pBankNode->setAttribute(PMAP_ATT_SNAME, m_nameShort);
  pBankNode->setAttribute(PMAP_ATT_LNAME, m_nameLong);
  pBankNode->setAttribute(PMAP_ATT_REFER_TO, m_referTo);
  pBankNode->setAttribute(PMAP_ATT_OFFSET, m_offset);

  BOOST_FOREACH (PMPage &page, m_pages) {
    if (page.isUsed()) {
      page.writeToXml(pBankNode);
    }
  }
}

bool PMBank::readFromXml(XmlElement *pBankNode) {
  if (!pBankNode || pBankNode->getTagName() != PMAP_NODE_BANK)
    return false;

  m_nameShort = pBankNode->getStringAttribute(PMAP_ATT_SNAME);
  m_nameLong = pBankNode->getStringAttribute(PMAP_ATT_LNAME);
  m_referTo = pBankNode->getIntAttribute(PMAP_ATT_REFER_TO);
  m_offset = pBankNode->getIntAttribute(PMAP_ATT_OFFSET);

  forEachXmlChildElement(*pBankNode, pPage) {
    int id = pPage->getIntAttribute(PMAP_ATT_ID);
    if (!m_pages[id].readFromXml(pPage)) {
      return false;
    }
  }
  return true;
}

//////////////////////////////////////////////////
// PlugMap
//////////////////////////////////////////////////

PlugMap::PlugMap(void)
// m_formatValues(true)
{
  initValues();
}

void PlugMap::initValues() {
  m_creator = String::empty;
  m_info = String::empty;

  for (int i = 0; i < 8; i++) {
    m_banks[i].initValues(i);
  }
}

PlugMap::~PlugMap(void) {}

#define PMAP_NODE_MAPINFO JUCE_T("MAPINFO")
#define PMAP_ATT_CREATOR JUCE_T("creator")

void PlugMap::writeToXml(XmlElement *pElement) {
  XmlElement *pInfoNode = new XmlElement(PMAP_NODE_MAPINFO);
  pElement->addChildElement(pInfoNode);
  pInfoNode->setAttribute(PMAP_ATT_CREATOR, m_creator);
  pInfoNode->addTextElement(m_info);

  BOOST_FOREACH (PMBank &bank, m_banks) {
    if (bank.isUsed()) {
      bank.writeToXml(pElement);
    }
  }
}

bool PlugMap::readFromXml(XmlElement *pRootElement) {
  forEachXmlChildElement(*pRootElement, pChild) {
    if (pChild->getTagName() == PMAP_NODE_MAPINFO) {
      m_creator = pChild->getStringAttribute(PMAP_ATT_CREATOR);
      m_info = pChild->getAllSubText();

    } else {
      int id = pChild->getIntAttribute(PMAP_ATT_ID);
      if (!m_banks[id].readFromXml(pChild)) {
        return false;
      }
    }
  }

  return true;
}
