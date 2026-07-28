/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripBinding.h"

ChannelStripBinding::ChannelStripBinding()
    : m_paramIndex(-1), m_insertPos(LAST) {}

int ChannelStripBinding::fixedChainPosition() const {
  // FIRST -> position 1, POS2 -> 2, ... POS8 -> 8
  if (m_insertPos == LAST)
    return -1;
  return static_cast<int>(m_insertPos) + 1;
}

String ChannelStripBinding::tokenForInsertPos(InsertPos pos) {
  if (pos == FIRST)
    return CSB_INS_FIRST;
  if (pos == LAST)
    return CSB_INS_LAST;
  // POS2..POS8 -> "2".."8"
  return String(static_cast<int>(pos) + 1);
}

ChannelStripBinding::InsertPos
ChannelStripBinding::insertPosFromToken(const String &token) {
  if (token == CSB_INS_FIRST)
    return FIRST;
  if (token == CSB_INS_LAST)
    return LAST;
  int n = token.getIntValue();
  if (n >= 2 && n <= 8)
    return static_cast<InsertPos>(n - 1);
  return LAST;
}

void ChannelStripBinding::writeToXml(XmlElement *pSlotElement) const {
  pSlotElement->setAttribute(CSB_ATT_FXIDENT, m_fxIdent);
  pSlotElement->setAttribute(CSB_ATT_FXGUID, m_fxGUID);
  pSlotElement->setAttribute(CSB_ATT_PARAM, m_paramIndex);
  pSlotElement->setAttribute(CSB_ATT_NAME, m_shortName);
  pSlotElement->setAttribute(CSB_ATT_INSPOS, tokenForInsertPos(m_insertPos));
}

bool ChannelStripBinding::readFromXml(const XmlElement *pSlotElement) {
  if (!pSlotElement)
    return false;
  m_fxIdent = pSlotElement->getStringAttribute(CSB_ATT_FXIDENT);
  m_fxGUID = pSlotElement->getStringAttribute(CSB_ATT_FXGUID);
  m_paramIndex = pSlotElement->getIntAttribute(CSB_ATT_PARAM, -1);
  m_shortName = pSlotElement->getStringAttribute(CSB_ATT_NAME);
  m_insertPos =
      insertPosFromToken(pSlotElement->getStringAttribute(CSB_ATT_INSPOS));
  return true;
}
