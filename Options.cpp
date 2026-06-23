/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "Options.h"
#include "McuDebugLog.h"
#ifndef _WIN32
#include "csurf_mcu.h"
#endif

#define OPT_NODE_OPTION JUCE_T("option")

#define OPT_ATT_VERSION JUCE_T("version")
#define OPT_ATT_NAME JUCE_T("name")
#define OPT_ATT_SELECTED JUCE_T("selectedPos")
#define OPT_ATT_SELECTED_NAME JUCE_T("selectedName")

Options::Options(DisplayHandler *pDH) : Selector(pDH) {}

Options::~Options(void) {}

void Options::addOption(String optionName) {
  m_optionList.push_back(make_pair(optionName, make_pair(tOption(), 0)));
}

void Options::addAttribute(String optionName, String attribute,
                           bool defaultAtt) {
  for (unsigned int i = 0; i < 4 && i < m_optionList.size(); ++i) {
    if (m_optionList[i].first.equalsIgnoreCase(optionName)) {
      m_optionList[i].second.first.push_back(attribute);
      if (defaultAtt) {
        m_optionList[i].second.second = m_optionList[i].second.first.size() - 1;
      }
    }
  }
}

void Options::activateSelector() {
  m_pDisplay->clear();
  for (unsigned int i = 0; i < 4 && i < m_optionList.size(); ++i)
    m_pDisplay->changeText(0, i * 14, m_optionList[i].first.toCString(), 13,
                           true);

  displaySelectedOptions();
  m_pDisplayHandler->switchTo(m_pDisplay);
}

bool Options::select(int index) {
  unsigned int numOpt = index / 2;

  if (numOpt >= m_optionList.size())
    return false;

  if (index != numOpt * 2) {
    m_optionList[numOpt].second.second++;
    if (m_optionList[numOpt].second.second >=
        (int)m_optionList[numOpt].second.first.size()) {
      m_optionList[numOpt].second.second = 0;
    }
  } else {
    if (m_optionList[numOpt].second.second > 0)
      m_optionList[numOpt].second.second--;
    else
      m_optionList[numOpt].second.second =
          (int)m_optionList[numOpt].second.first.size() - 1;
  }

  displaySelectedOptions();
  return true;
}

void Options::move(unsigned int index, int steps) {
  unsigned int numOpt = (index - 1) / 2;

  if (numOpt >= m_optionList.size())
    return;

  m_optionList[numOpt].second.second += steps;

  if (m_optionList[numOpt].second.second < 0) {
    m_optionList[numOpt].second.second = 0;
  } else if (m_optionList[numOpt].second.second >=
             (int)m_optionList[numOpt].second.first.size()) {
    m_optionList[numOpt].second.second =
        m_optionList[numOpt].second.first.size() - 1;
  }

  displaySelectedOptions();
}

String Options::getSelectedOptionAsString(int option) {
  return m_optionList[option].second.first[m_optionList[option].second.second];
}

String Options::getSelectedOptionAsString(const String &optionName) {
  for (unsigned int i = 0; i < 4 && i < m_optionList.size(); ++i) {
    if (m_optionList[i].first == optionName) {
      return getSelectedOptionAsString(i);
    }
  }
  return String::empty;
}

int Options::getSelectedOption(wchar_t *optionName) {
  String strOptionName = String(optionName);
  for (unsigned int i = 0; i < 4 && i < m_optionList.size(); ++i) {
    if (m_optionList[i].first == strOptionName) {
      return m_optionList[i].second.second;
    }
  }
  return -1;
}

bool Options::isOptionSetTo(const wchar_t *optionName,
                            const wchar_t *attribute) {
  return getSelectedOptionAsString(String(optionName)) == String(attribute);
}

void Options::setOptionTo(wchar_t *optionName, int attributeId) {
  for (unsigned int i = 0; i < 4 && i < m_optionList.size(); ++i) {
    if (m_optionList[i].first.equalsIgnoreCase(String(optionName))) {
      if (attributeId >= 0 &&
          attributeId < (int)m_optionList[i].second.first.size()) {
        m_optionList[i].second.second = attributeId;
      }
      return;
    }
  }
}

void Options::displaySelectedOptions() {
  checkAndModifyOptions();

  for (unsigned int i = 0; i < 4 && i < m_optionList.size(); ++i) {
    m_pDisplay->changeText(1, i * 14, getSelectedOptionAsString(i).toCString(),
                           13, true);
  }
}

void Options::writeConfigFile() {
  XmlElement *pRootElement = new XmlElement("OPTION_CONFIG");
  pRootElement->setAttribute(OPT_ATT_VERSION, 1);

  for (unsigned int i = 0; i < m_optionList.size(); i++) {
    writeOptionToXml(pRootElement, m_optionList[i]);
  }

  pRootElement->writeToFile(getConfigFile(), "", JUCE_T("UTF-8"));

  safe_delete(pRootElement);
}

void Options::writeOptionToXml(XmlElement *pDoc,
                               tSingleOptionSelection &option) {
  int selected = option.second.second;
  XmlElement *pOptionNode = new XmlElement(OPT_NODE_OPTION);
  pDoc->addChildElement(pOptionNode);
  pOptionNode->setAttribute(OPT_ATT_NAME, option.first);
  pOptionNode->setAttribute(OPT_ATT_SELECTED, selected);
  pOptionNode->setAttribute(OPT_ATT_SELECTED_NAME,
                            option.second.first[selected]);
}

bool Options::readConfigFile() {
  XmlDocument *pXmlFile = new XmlDocument(getConfigFile());
  if (!pXmlFile)
    return false;

  XmlElement *pRootElement = pXmlFile->getDocumentElement();
  if (!pRootElement)
    return false;

  unsigned int i = 0;
  forEachXmlChildElement(*pRootElement, pOption) {
    if (i < m_optionList.size()) {
      readOptionFromXml(pOption, m_optionList[i]);
    }
    i++;
  }

  safe_delete(pRootElement);
  safe_delete(pXmlFile);
  return true;
}

void Options::readOptionFromXml(XmlElement *pOptionNode,
                                tSingleOptionSelection &option) {
  if (!pOptionNode || pOptionNode->getTagName() != OPT_NODE_OPTION)
    return;

  if (option.first != pOptionNode->getStringAttribute(OPT_ATT_NAME))
    return;

  unsigned int selected = pOptionNode->getIntAttribute(OPT_ATT_SELECTED);
  if (selected >= option.second.first.size())
    return;

  if (option.second.first[selected] !=
      pOptionNode->getStringAttribute(OPT_ATT_SELECTED_NAME)) {
    return;
  }

  option.second.second = selected;
}

File Options::getConfigFile() {
#ifdef _WIN32
#ifdef EXT_B
  File configDir =
      File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() +
      JUCE_T("\\Reaper\\MCU_B\\Config\\");
#else
  File configDir =
      File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() +
      JUCE_T("\\Reaper\\MCU\\Config\\");
#endif
  if (!configDir.exists())
    configDir.createDirectory();
  return File(configDir.getFullPathName() + JUCE_T("\\") + getConfigFileName() +
              JUCE_T(".xml"));
#else
#ifdef EXT_B
  File configDir = String(GetResourcePath()) + JUCE_T("/MCU_B/Config/");
#else
  File configDir = String(GetResourcePath()) + JUCE_T("/MCU/Config/");
#endif
  if (!configDir.exists())
    configDir.createDirectory();
  return File(configDir.getFullPathName() + JUCE_T("/") + getConfigFileName() +
              JUCE_T(".xml"));
#endif
}
