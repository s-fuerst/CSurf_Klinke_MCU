/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripMap.h"

#define CSM_TAG_SLOT String("SLOT")
#define CSM_ATT_CREATOR String("creator")
#define CSM_ATT_INFO String("info")

ChannelStripMap::ChannelStripMap() { initEmpty(); }

ChannelStripMap::~ChannelStripMap() {}

void ChannelStripMap::initEmpty() {
  m_slots.assign(kNumSlots, ChannelStripBinding());
  m_creator = String();
  m_info = String();
}

int ChannelStripMap::numAssigned() const {
  int n = 0;
  for (int i = 0; i < kNumSlots; i++)
    if (m_slots[i].isAssigned())
      n++;
  return n;
}

void ChannelStripMap::writeToXml(XmlElement *pParent) const {
  if (!pParent)
    return;
  pParent->setAttribute(CSM_ATT_CREATOR, m_creator);
  pParent->setAttribute(CSM_ATT_INFO, m_info);
  for (int i = 0; i < kNumSlots; i++) {
    const ChannelStripBinding &b = m_slots[i];
    if (!b.isAssigned())
      continue;
    XmlElement *pSlot = new XmlElement(CSM_TAG_SLOT);
    pSlot->setAttribute(CSB_ATT_NR, i + 1);
    b.writeToXml(pSlot);
    pParent->addChildElement(pSlot);
  }
}

bool ChannelStripMap::readFromXml(const XmlElement *pParent) {
  if (!pParent)
    return false;
  initEmpty();
  m_creator = pParent->getStringAttribute(CSM_ATT_CREATOR);
  m_info = pParent->getStringAttribute(CSM_ATT_INFO);
  forEachXmlChildElementWithTagName(*pParent, pSlot, CSM_TAG_SLOT) {
    int nr = pSlot->getIntAttribute(CSB_ATT_NR, 0);
    if (nr < 1 || nr > kNumSlots)
      continue;
    m_slots[nr - 1].readFromXml(pSlot);
  }
  return true;
}
