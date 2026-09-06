/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripMap.h"

#define CSM_TAG_STRIP String("STRIP")
#define CSM_TAG_VPOT String("VPOT")

ChannelStripMap::ChannelStripMap() { initEmpty(); }

void ChannelStripMap::initEmpty() {
  m_fxIdent = String();
  m_shortName = String();
  m_insertPos = FIRST; // default insert position is "first"
  for (int i = 0; i < kNumVPOTs; i++) {
    m_vpotParam[i] = -1;
    m_vpotName[i] = String();
    m_vpotDiscreteCount[i] = 0;
    m_vpotStepsManual[i] = false;
  }
}

int ChannelStripMap::getParamForVPOT(int position) const {
  if (position < 0 || position >= kNumVPOTs)
    return -1;
  return m_vpotParam[position];
}

void ChannelStripMap::setParamForVPOT(int position, int paramIdx) {
  if (position < 0 || position >= kNumVPOTs)
    return;
  m_vpotParam[position] = paramIdx;
}

const String &ChannelStripMap::getVPOTName(int position) const {
  static const String empty;
  if (position < 0 || position >= kNumVPOTs)
    return empty;
  return m_vpotName[position];
}

void ChannelStripMap::setVPOTName(int position, const String &name) {
  if (position < 0 || position >= kNumVPOTs)
    return;
  m_vpotName[position] = name;
}

int ChannelStripMap::getDiscreteCountForVPOT(int position) const {
  if (position < 0 || position >= kNumVPOTs)
    return 0;
  return m_vpotDiscreteCount[position];
}

void ChannelStripMap::setDiscreteCountForVPOT(int position, int count) {
  if (position < 0 || position >= kNumVPOTs)
    return;
  m_vpotDiscreteCount[position] = count;
}

bool ChannelStripMap::isDiscreteForVPOT(int position) const {
  return getDiscreteCountForVPOT(position) > 0;
}

bool ChannelStripMap::isStepsManualForVPOT(int position) const {
  if (position < 0 || position >= kNumVPOTs)
    return false;
  return m_vpotStepsManual[position];
}

void ChannelStripMap::setStepsManualForVPOT(int position, bool manual) {
  if (position < 0 || position >= kNumVPOTs)
    return;
  m_vpotStepsManual[position] = manual;
}

int ChannelStripMap::numBoundVPOTs() const {
  int n = 0;
  for (int i = 0; i < kNumVPOTs; i++)
    if (m_vpotParam[i] >= 0)
      n++;
  return n;
}

int ChannelStripMap::fixedChainPosition() const {
  if (m_insertPos == LAST)
    return -1;
  return static_cast<int>(m_insertPos) + 1;
}

String ChannelStripMap::tokenForInsertPos(InsertPos pos) {
  if (pos == FIRST)
    return CSB_INS_FIRST;
  if (pos == LAST)
    return CSB_INS_LAST;
  return String(static_cast<int>(pos) + 1); // POS2..POS8 -> "2".."8"
}

ChannelStripMap::InsertPos
ChannelStripMap::insertPosFromToken(const String &token) {
  if (token == CSB_INS_FIRST)
    return FIRST;
  if (token == CSB_INS_LAST)
    return LAST;
  int n = token.getIntValue();
  if (n >= 2 && n <= 8)
    return static_cast<InsertPos>(n - 1);
  return LAST;
}

void ChannelStripMap::writeToXml(XmlElement *pParent, int nr) const {
  if (!pParent)
    return;
  XmlElement *pStrip = new XmlElement(CSM_TAG_STRIP);
  pStrip->setAttribute(CSB_ATT_NR, nr);
  pStrip->setAttribute(CSB_ATT_FXIDENT, m_fxIdent);
  pStrip->setAttribute(CSB_ATT_NAME, m_shortName);
  pStrip->setAttribute(CSB_ATT_INSPOS, tokenForInsertPos(m_insertPos));
  for (int i = 0; i < kNumVPOTs; i++) {
    if (m_vpotParam[i] < 0 && m_vpotName[i].isEmpty())
      continue;
    XmlElement *pV = new XmlElement(CSM_TAG_VPOT);
    pV->setAttribute(CSB_ATT_NR, i + 1);
    pV->setAttribute(CSB_ATT_PARAM, m_vpotParam[i]);
    if (!m_vpotName[i].isEmpty())
      pV->setAttribute(CSB_ATT_NAME, m_vpotName[i]);
    if (m_vpotDiscreteCount[i] > 0)
      pV->setAttribute(CSB_ATT_DISCRETE, m_vpotDiscreteCount[i]);
    if (m_vpotStepsManual[i])
      pV->setAttribute(CSB_ATT_MANUAL, 1);
    pStrip->addChildElement(pV);
  }
  pParent->addChildElement(pStrip);
}

bool ChannelStripMap::readFromXml(const XmlElement *pStrip) {
  if (!pStrip)
    return false;
  m_fxIdent = pStrip->getStringAttribute(CSB_ATT_FXIDENT);
  m_shortName = pStrip->getStringAttribute(CSB_ATT_NAME);
  m_insertPos = insertPosFromToken(pStrip->getStringAttribute(CSB_ATT_INSPOS));
  for (int i = 0; i < kNumVPOTs; i++) {
    m_vpotParam[i] = -1;
    m_vpotDiscreteCount[i] = 0;
    m_vpotStepsManual[i] = false;
  }
  forEachXmlChildElementWithTagName(*pStrip, pV, CSM_TAG_VPOT) {
    int nr = pV->getIntAttribute(CSB_ATT_NR, 0);
    if (nr >= 1 && nr <= kNumVPOTs) {
      m_vpotParam[nr - 1] = pV->getIntAttribute(CSB_ATT_PARAM, -1);
      m_vpotName[nr - 1] = pV->getStringAttribute(CSB_ATT_NAME);
      m_vpotDiscreteCount[nr - 1] =
          pV->getIntAttribute(CSB_ATT_DISCRETE, 0);
      m_vpotStepsManual[nr - 1] = (pV->getIntAttribute(CSB_ATT_MANUAL, 0) != 0);
    }
  }
  return true;
}
