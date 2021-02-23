/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once

#include "csurf_mcu.h"
#include "Assert.h"
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include <boost/tuple/tuple.hpp>
#include <map>
#define NOT_ASSIGNED -1
#define DOESNT_REFER -1

//////////////////////////////////////////////////
// PMParam
//////////////////////////////////////////////////

class PMParam {
public:
  virtual void initValues();

  virtual void writeToXml(XmlElement *pElement);
  virtual bool readFromXml(XmlElement *pElement);

  virtual String getNameShort() { return m_nameShort; }
  virtual String getNameLong() { return m_nameLong; }

  virtual int getParamID() {
    return m_paramID;
  } // Reaper id for parameter (called param in function signatures), can be
    // NOT_ASSIGNED

  virtual void setNameShort(String newName) { m_nameShort = newName; }
  virtual void setNameLong(String newName) { m_nameLong = newName; }

  virtual void setParamID(int newID) { m_paramID = newID; }

protected:
  String m_nameShort;
  String m_nameLong;

  int m_paramID; // is NOT_ASSIGNED if no parameter is controlled via this
                 // element
};

class PMFader : public PMParam {
public:
  void writeToXml(XmlElement *pElement);
  bool readFromXml(XmlElement *pElement);
};

class PMVPot : public PMParam {
public:
  PMVPot();
  virtual ~PMVPot();

  void initValues();

  void writeToXml(XmlElement *pElement);
  bool readFromXml(XmlElement *pElement);

  typedef boost::tuple<String, String> tStepsValue;
  typedef std::map<double, tStepsValue> tSteps;
  tSteps *getStepsMap() { return m_pStepsMap; }

  void setParamID(int newID);

private:
  tSteps *m_pStepsMap;
};

//////////////////////////////////////////////////
// PMPage
//////////////////////////////////////////////////

class PMPage {
public:
  void initValues(int id);
  int getId() { return m_id; }

  void writeToXml(XmlElement *pElement);
  bool readFromXml(XmlElement *pElement);

  String getNameShort() { return m_nameShort; }
  String getNameLong() { return m_nameLong; }

  void setNameShort(String newName) { m_nameShort = newName; }
  void setNameLong(String newName) { m_nameLong = newName; }

  bool
  isUsed(); // the page is used if a fader or vpot have an assigned parameter
  bool doesRefer() { return m_referTo != DOESNT_REFER; }
  int referTo() {
    return m_referTo;
  } // can be DOESNT_REFER, if page doesn't refer to another page
  void setReferTo(int referTo) {
    m_referTo = (referTo == m_id) ? DOESNT_REFER : referTo;
  }

  int getParamIDOffset() {
    ASSERT(doesRefer());
    return m_offset;
  } // call this only, if doesRefer() is true
  void setParamIDOffset(int offset) { m_offset = offset; }

  PMFader *getFader(int channel) { return &m_faders[channel]; }
  PMVPot *getVPot(int channel) { return &m_vpots[channel]; }

private:
  String m_nameShort;
  String m_nameLong;

  int m_referTo;
  int m_offset;

  int m_id;

  PMFader m_faders[8];
  PMVPot m_vpots[8];
};

//////////////////////////////////////////////////
// PMBank
//////////////////////////////////////////////////

class PMBank {
public:
  void initValues(int id);
  int getId() { return m_id; }

  void writeToXml(XmlElement *pElement);
  bool readFromXml(XmlElement *pElement);

  String getNameShort() { return m_nameShort; }
  String getNameLong() { return m_nameLong; }

  void setNameShort(String newName) { m_nameShort = newName; }
  void setNameLong(String newName) { m_nameLong = newName; }

  bool isUsed(); // the bank is used if one or more pages are used

  bool doesRefer() { return m_referTo != DOESNT_REFER; }
  int referTo() {
    return m_referTo;
  } // can be DOESNT_REFER, if bank doesn't refer to another ban
  void setReferTo(int referTo) {
    m_referTo = (referTo == m_id) ? DOESNT_REFER : referTo;
  }

  int getParamIDOffset() {
    ASSERT(doesRefer());
    return m_offset;
  } // call this only, if doesRefer() is true
  void setParamIDOffset(int offset) { m_offset = offset; }

  PMPage *getPage(int page) { return &m_pages[page]; }

private:
  String m_nameShort;
  String m_nameLong;

  int m_referTo;
  int m_offset;

  int m_id;

  PMPage m_pages[8];
};

//////////////////////////////////////////////////
// PlugMap
//////////////////////////////////////////////////

class PlugMap {
public:
  PlugMap(void);
  ~PlugMap(void);

  void initValues();

  String getCreator() { return m_creator; }
  void setCreator(String creator) { m_creator = creator; }

  String getInfo() { return m_info; }
  void setInfo(String info) { m_info = info; }

  void writeToXml(XmlElement *pElement);
  bool readFromXml(XmlElement *pElement);

  PMBank *getBank(int bank) { return &m_banks[bank]; }

private:
  PMBank m_banks[8];
  String m_creator;
  String m_info;
};
