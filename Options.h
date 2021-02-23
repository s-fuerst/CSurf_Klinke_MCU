/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "selector.h"
#include <stdarg.h>

class Options : public Selector {
public:
  Options(DisplayHandler *pDH);
  virtual ~Options(void);

  virtual void activateSelector();
  // returns true if selector should still be active
  virtual bool select(int index); // 0-7
  // called when the VPOT was moved, steps can be negativ, no wrap around
  virtual void move(unsigned int numOpt, int steps);

  bool isOptionSetTo(const wchar_t *optionName, const wchar_t *attribute);

protected:
  typedef std::vector<String> tOption;
  typedef std::pair<tOption, int> tOptionsAndSelection;
  typedef std::pair<String, tOptionsAndSelection> tSingleOptionSelection;
  typedef std::vector<tSingleOptionSelection> tOptionsOnPage;

  void addOption(String optionName); // must be done before activated
  void addAttribute(
      String optionName, String attribute,
      bool defaultAtt = false); // one attribute must be added before activation

  String getSelectedOptionAsString(const String &optionName);
  String getSelectedOptionAsString(int option);
  int getSelectedOption(wchar_t *optionName);
  void setOptionTo(wchar_t *optionName, int attributeId);

  // this is called after a different option was selected but before the display
  // is updated. so the concrete implementation has a chance to add option
  // dependencies
  virtual void checkAndModifyOptions(){};

  virtual void displaySelectedOptions();

  virtual String getConfigFileName() = 0;
  void writeConfigFile();
  bool readConfigFile();

  tOptionsOnPage m_optionList;

private:
  void writeOptionToXml(XmlElement *pDoc, tSingleOptionSelection &option);
  void readOptionFromXml(XmlElement *pOptionNode,
                         tSingleOptionSelection &option);
  File getConfigFile();
};
