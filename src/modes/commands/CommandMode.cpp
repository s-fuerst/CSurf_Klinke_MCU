/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "csurf.h"
#include "CommandMode.h"
#include "CCSManager.h"
#include "csurf_mcu.h"
#include "Display.h"
#include "MultiDisplay.h"
#ifdef _WIN32
#include "swell\swell.h"
#endif
#include "JuceHeader.h"
#include "CommandModeMainComponent.h"
#include "ConfigPath.h"

CommandMode::Page::Page(CommandMode *pMode, int index)
    : m_pMode(pMode), m_iIndex(index) {
  m_strPageName = String("Bank ") + String(index + 1);
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 8; j++) {
      m_bRelative[i][j] = false;
      m_iNormalSpeed[i][j] = 1;
      m_iPressedSpeed[i][j] = 5;
      m_strCommandName[i][j] = juce::String();
    }
  }
}

#define CM_NODE_PAGE String("PAGE")
#define CM_NODE_VPOT String("VPOT")

#define CM_ATT_NAME String("name")
#define CM_ATT_SHIFT String("shift")
#define CM_ATT_INDEX String("index")
#define CM_ATT_RELATIVE String("relative")
#define CM_ATT_NORMAL String("normal")
#define CM_ATT_FAST String("fast")
#define CM_ATT_VERSION String("version")

void CommandMode::Page::writeToXml(XmlElement *pDoc) {
  XmlElement *pPageNode = new XmlElement(CM_NODE_PAGE);
  pDoc->addChildElement(pPageNode);
  pPageNode->setAttribute(CM_ATT_NAME, m_strPageName);
  pPageNode->setAttribute(CM_ATT_INDEX, m_iIndex);
  pPageNode->setAttribute(CM_ATT_VERSION, 1);

  for (int shift = 0; shift < 2; shift++) {
    for (int channel = 0; channel < 8; channel++) {
      XmlElement *pVpot = new XmlElement(CM_NODE_VPOT);
      pPageNode->addChildElement(pVpot);

      pVpot->setAttribute(CM_ATT_INDEX, channel);
      if (shift == 1)
        pVpot->setAttribute(CM_ATT_SHIFT, "true");

      pVpot->setAttribute(CM_ATT_NAME, m_strCommandName[shift][channel]);

      if (m_bRelative[shift][channel])
        pVpot->setAttribute(CM_ATT_RELATIVE, "true");
      pVpot->setAttribute(CM_ATT_NORMAL, m_iNormalSpeed[shift][channel]);
      pVpot->setAttribute(CM_ATT_FAST, m_iPressedSpeed[shift][channel]);
    }
  }
}

bool CommandMode::Page::readFromXML(XmlElement *pPageNode) {
  if (!pPageNode || pPageNode->getTagName() != CM_NODE_PAGE)
    return false;

  m_strPageName = pPageNode->getStringAttribute(CM_ATT_NAME);

  XmlElement *pVpot = pPageNode->getFirstChildElement();
  int nodesRead = 0;
  while (pVpot && pVpot->getTagName() == CM_NODE_VPOT) {
    int shift = pVpot->getBoolAttribute(CM_ATT_SHIFT);
    int channel = pVpot->getIntAttribute(CM_ATT_INDEX);
    m_strCommandName[shift][channel] = pVpot->getStringAttribute(CM_ATT_NAME);
    m_bRelative[shift][channel] = pVpot->getBoolAttribute(CM_ATT_RELATIVE);
    m_iNormalSpeed[shift][channel] = pVpot->getIntAttribute(CM_ATT_NORMAL);
    m_iPressedSpeed[shift][channel] = pVpot->getIntAttribute(CM_ATT_FAST);
    nodesRead++;
    pVpot = pVpot->getNextElement();
  }

  return (nodesRead == 16);
}

CommandMode::CommandMode(CCSManager *pManager) : MultiTrackMode(pManager) {
  m_bConfigLoaded = false;
  for (int i = 0; i < 8; i++) {
    m_pPage[i] = new Page(this, i);
    m_iActivePageIndex[i] = i; // P3: default unit x -> page x
  }

  readConfigFile();

  m_pSelector = new CommandPageSelector(pManager->getDisplayHandler(), this);

  m_pMainComponent = NULL;
}

CommandMode::~CommandMode(void) {
  safe_delete(m_pMainComponent); // main component must be delete first

  for (int i = 0; i < 8; i++) {
    safe_delete(m_pPage[i]);
  }

  safe_delete(m_pSelector);
}

bool CommandMode::readConfigFile() {
  XmlDocument *pXmlFile = new XmlDocument(getConfigFile());
  if (!pXmlFile)
    return false;

  auto docRoot = pXmlFile->getDocumentElement();
  XmlElement *pRootElement = docRoot.get();
  if (!pRootElement)
    return false;

  forEachXmlChildElement(*pRootElement, pPage) {
    bool success =
        m_pPage[pPage->getIntAttribute(CM_ATT_INDEX)]->readFromXML(pPage);
    if (!success) {
      safe_delete(pXmlFile);
      return false;
    }
  }

  safe_delete(pXmlFile);
  m_bConfigLoaded = true; // P1: mark a successful load so deactivate() may save
  return true;
}

void CommandMode::writeConfigFile() {
  XmlElement *pRootElement = new XmlElement("ACTIVE_MODE_CONFIG");
  pRootElement->setAttribute(CM_ATT_VERSION, 1);

  for (int iPage = 0; iPage < 8; iPage++) {
    m_pPage[iPage]->writeToXml(pRootElement);
  }

  pRootElement->writeToFile(getConfigFile(), "", String("UTF-8"));

  safe_delete(pRootElement);
}

void CommandMode::activate() {
  CCSMode::activate();
  // MultiDisplay needs switchToAll for N>1
  MultiDisplay *md = dynamic_cast<MultiDisplay *>(m_pDisplay);
  if (md)
    md->switchToAll();
  else
    m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
	m_pCCSManager->getDisplayHandler()->enableMCUMeter(false);
}

// P1: persist config when leaving the mode (covers the mode-switch path and,
// via the editor dtor, shutdown). Guarded so a failed/absent config load never
// overwrites a real config file with default-constructed data.
void CommandMode::deactivate() {
  if (m_bConfigLoaded)
    writeConfigFile();
  MultiTrackMode::deactivate();
}

// command mode: Channel 1-8, 0x06-0x0c, 0x16-0x1c, 0x26-0x2c ... 0x76-0x7c (the
// channel is defined thru the page) (0xab : a is channel, b has six different
// state,
//                       8 left while not pressed,
//                       9 right while not pressed,
//                       a left while pressed,
//                       b right while pressed,
//                       c relativ)
bool CommandMode::vpotMoved(int channel, int numSteps) {
  // P3: channel is global (1-based, 1..N*8); split into unit + local channel.
  int unit = (channel - 1) / 8;
  int localChan = (channel - 1) % 8; // 0-based local channel within the unit
  Page *pActive = m_pPage[m_iActivePageIndex[unit]];

  unsigned char midi_byte0 = 0xb0 + pActive->m_iIndex;
  int shift = m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT) ? 1 : 0;
  if (shift)
    midi_byte0 += 0x08;
  unsigned char midi_byte1 = 0x10 * localChan;
  unsigned char midi_byte2;
  VPOT_LED *pVpot = m_pCCSManager->getVPOT(channel); // global 1-based channel
  int numSends = 1;

  if (pActive->m_bRelative[shift][localChan] == true) {
    midi_byte1 += 0x0c;
    int relative =
        0x40 + numSteps * (pVpot->isPressed()
                               ? pActive->m_iPressedSpeed[shift][localChan]
                               : pActive->m_iNormalSpeed[shift][localChan]);
    midi_byte2 = (unsigned char)std::min(255, std::max(0, relative));
  } else {
    bool left = (numSteps < 0);
    midi_byte1 += left ? 0x08 : 0x09;
    if (pVpot->isPressed())
      midi_byte1 += 0x02;
    midi_byte2 = 0x7f;
    numSends = left ? -numSteps : numSteps;
  }

  MIDI_event_t evt = {0, 3, {midi_byte0, midi_byte1, midi_byte2}};
  for (int i = 0; i < numSends; i++)
    kbd_OnMidiEvent(&evt, -1);

  return true;
}

// command mode: Channel 1-8, 0x06-0x0c, 0x16-0x1c, 0x26-0x2c ... 0x76-0x7c (the
// channel is defined thru the page) (0xab : a is channel, b has six different
// state,
//                       6 vpot pressed,
//                       7 vpot released)
bool CommandMode::vpotPressed(int channel, bool pressed) {
  // P3: channel is global (1-based); split into unit + local channel.
  int unit = (channel - 1) / 8;
  int localChan = (channel - 1) % 8;
  Page *pActive = m_pPage[m_iActivePageIndex[unit]];

  unsigned char midi_byte0 = 0xb0 + pActive->m_iIndex;
  if (m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT))
    midi_byte0 += 0x08;
  unsigned char midi_byte1 = 0x10 * localChan + (pressed ? 0x06 : 0x07);

  MIDI_event_t evt = {0, 3, {midi_byte0, midi_byte1, 0x07}};
  kbd_OnMidiEvent(&evt, -1);

  return true;
}

// while the fader is touched the level is written to the display
// overwrite it with the page names
bool CommandMode::faderTouched(int channel, bool touched) {
  if (!touched)
    updateDisplay();

  return true;
}

// Disable the VPOT leds
void CommandMode::updateVPOTs() {
  m_pCCSManager->setVPOTMode(VPOT_LED::OFF);
  // widened from 8 to getNumberOfChannelStrips()
  int nStrips = Tracks::instance()->getNumberOfChannelStrips();
  for (int channel = 1; channel <= nStrips; channel++) {
    if (MediaTrack *tr = getMediaTrackForChannel(channel)) {
      m_pCCSManager->getVPOT(channel)->setBottom(
          Tracks::instance()->hasChilds(tr));
    }
  }
}

// write the page names to the second row
void CommandMode::updateDisplay() {
  MultiTrackMode::updateDisplay();

	// per-unit ProX check, widened loop
	if (m_pCCSManager->getMCU()->unitForChannel(1) &&
	    m_pCCSManager->getMCU()->unitForChannel(1)->isProX()) {
		int nStrips = Tracks::instance()->getNumberOfChannelStrips();
		for (int iChan = 1; iChan <= nStrips; iChan++) {
			MediaTrack *tr = getMediaTrackForChannel(iChan);
			if (tr) {
				if (s_flipmode) {
					m_pDisplay->showPan(3, iChan,
													*((double *)GetSetMediaTrackInfo(tr, "D_PAN", NULL)));
				}
				else {
					m_pDisplay->showDB(3, iChan,
													*((double *)GetSetMediaTrackInfo(tr, "D_VOL", NULL)));
				}
			} else {
				m_pDisplay->changeField(3, iChan, "");
			}
		}
	}

  int shift = m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT) ? 1 : 0;
  // P3: render line 1 per unit from each unit's own active page.
  int numUnits = m_pCCSManager->getMCU()->numUnits();
  if (numUnits < 1)
    numUnits = 1;

  m_pDisplay->clearLine(1);

  // If the EQ button is held, show the 8 global page names on every
  // unit — the user is in the bank-selector picking a page via VPOT.
  if (m_pCCSManager->getMCU()->IsButtonPressed(B_VPOT_EQ)) {
    for (int u = 0; u < numUnits; u++) {
      for (int lc = 0; lc < 8; lc++)
        m_pDisplay->changeField(1, u * 8 + lc + 1,
                                m_pPage[lc]->m_strPageName.toRawUTF8());
    }
    return;
  }

  MultiDisplay *md = dynamic_cast<MultiDisplay *>(m_pDisplay);
  static const char *kNoActionsHint =
      "No actions are named for this bank (press Alt+EQ).";
  for (int u = 0; u < numUnits; u++) {
    Page *pActive = m_pPage[m_iActivePageIndex[u]];

    bool anyNamed = false;
    for (int lc = 0; lc < 8; lc++) {
      if (pActive->getCommandName(shift, lc) != String()) {
        anyNamed = true;
        break;
      }
    }

    if (anyNamed) {
      for (int lc = 0; lc < 8; lc++)
        m_pDisplay->changeField(
            1, u * 8 + lc + 1, pActive->getCommandName(shift, lc).toRawUTF8());
    } else {
      // Per-unit hint: changeText pads/centers the line on this unit only.
      if (md && u < (int)md->children().size())
        md->children()[u]->changeText(1, 0, kNoActionsHint, 55, true);
      else if (!md && u == 0)
        m_pDisplay->changeText(1, 0, kNoActionsHint, 55, true);
    }
  }
}

Component **CommandMode::createEditorComponent() {
  if (!m_pMainComponent)
    m_pMainComponent = new CommandModeMainComponent(this);

  return reinterpret_cast<Component **>(&m_pMainComponent);
}

void CommandMode::deleteEditorComponent() { safe_delete(m_pMainComponent) }

File CommandMode::getConfigFile() {
  // P2: shared cross-platform helper (matches Options::getConfigFile()).
  return getMcuConfigFile("ActionMode.xml");
}
