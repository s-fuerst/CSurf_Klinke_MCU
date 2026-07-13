/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once
#include "MultiTrackMode.h"
#include "JuceHeader.h"
#include "Selector.h"
#include "Display.h"
#include "MultiDisplay.h"
#include "csurf_mcu.h"
#include "CCSManager.h"
#include "McuAssert.h"
#include "CommandModeMainComponent.h"

class CommandPageSelector;

// The command mode sends CCs to reaper. Using the MIDI-learn function in the
// action display, the CCs can be assign to actions like moving takes etc..
class CommandMode : public MultiTrackMode {
  friend class CommandPageSelector;

public:
  class Page {
  public:
    Page(CommandMode *pMode, int index);

    int m_iIndex;
    juce::String m_strPageName;

    bool m_bRelative[2][8];
    int m_iNormalSpeed[2][8];
    int m_iPressedSpeed[2][8];

    void setCommandName(unsigned shift, unsigned channel, juce::String name) {
      ASSERT(shift < 2 && channel < 8);
      m_strCommandName[shift][channel] = name;
      m_pMode->updateDisplay();
    }
    juce::String getCommandName(unsigned shift, unsigned channel) {
      ASSERT(shift < 2 && channel < 8);
      return m_strCommandName[shift][channel];
    }

    void writeToXml(XmlElement *);
    bool readFromXML(XmlElement *);

  private:
    juce::String m_strCommandName[2][8];
    CommandMode *m_pMode;
  };

public:
  CommandMode(CCSManager *pManager);
  virtual ~CommandMode(void);

  bool readConfigFile();
  void writeConfigFile();

  void activate();
  // P1: persist config when leaving the mode (guarded by m_bConfigLoaded).
  void deactivate();

  bool vpotMoved(int channel,
                 int numSteps); // numSteps are negativ for left rotation
  bool vpotPressed(int channel, bool pressed);

  bool faderTouched(int channel, bool touched);

  void updateVPOTs();
  void updateDisplay();

  Component **createEditorComponent();
  void deleteEditorComponent();

  Selector *getSelector() { return (Selector *)m_pSelector; }

  File getConfigFile();
  CommandMode::Page *getPage(int index) {
    ASSERT(index < 8);
    return m_pPage[index];
  }

private:
  Page *m_pPage[8];
  // P3: per-unit active page cursor (replaces the single m_pActivePage).
  // m_iActivePageIndex[unit] -> page index 0..7. Default: unit x -> page x.
  int m_iActivePageIndex[8];
  // P1: set true on successful readConfigFile(); guards deactivate() save so
  // default-constructed data never overwrites a real config file.
  bool m_bConfigLoaded;

  bool m_bVPOTPressed[8];

  CommandPageSelector *m_pSelector;

  Component *m_pMainComponent;
  //      Component* m_pEditorComponent;

  // P3: which page is active for the unit owning global channel g (1-based)?
  int activePageIndexForChannel(int g) const {
    return m_iActivePageIndex[(g - 1) / 8];
  }
};

class CommandPageSelector : public Selector {
public:
  CommandPageSelector(DisplayHandler *pDH, CommandMode *pCM) : Selector(pDH) {
    m_pCommandMode = pCM;
  }
  ~CommandPageSelector() {}

  void activateSelector() {
    for (int i = 0; i < 8; i++)
      m_pDisplay->changeField(
          1, i + 1, m_pCommandMode->m_pPage[i]->m_strPageName.toRawUTF8());

    m_pCommandMode->m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
  }
  // P3: select() is called by CCSManager with (globalChannel - 1). Split it
  // into unit + local page and set the active page for the picking unit only.
  // Returns true if the selector should stay active.
  bool select(int globalIndex) {
    int unit = globalIndex / 8;
    int localPage = globalIndex % 8;
    ASSERT(unit >= 0 && unit < 8);
    m_pCommandMode->m_iActivePageIndex[unit] = localPage;
    return false; // close the selector globally after one pick
  }

private:
  CommandMode *m_pCommandMode;
};
