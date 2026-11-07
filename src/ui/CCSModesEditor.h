/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once

#include "csurf_mcu.h"
#include "JuceHeader.h"
#include "ProjectConfig.h"

class CCSManager;
class CCSModesEditorWindow;
class CCSMode;
class CommandMode;

class CCSModesEditor {
public:
  CCSModesEditor(CCSManager *pManager);

public:
  virtual ~CCSModesEditor(void);

  void setMainComponent(Component **ppComponent, bool visible);
  void setMainComponent(CCSMode *pCommandMode, bool visible);
  // Opens a component that is NOT owned by a mode: the WINDOW owns it and
  // NULLs *ppComponent when the window closes or is rebuilt. Callers must
  // re-create their component whenever the pointer is NULL — do NOT keep a
  // second copy of the pointer (the window deletes the component itself).
  void setComponentWindow(Component **ppComponent, bool visible);
  void closeWindowAndRemoveComponent(Component *pComponent);
  void deleteWindow();

  void projectChanged(XmlElement *pXmlElement, ProjectConfig::EAction action);

private:
  // shared body of setMainComponent()/setComponentWindow(); trackPointer
  // decides whether the window takes ownership of the pointer.
  void openWindow(Component **ppComponent, bool visible, bool trackPointer);

  CCSManager *m_pManager;
  CCSModesEditorWindow *m_pWindow;
  CCSMode *m_pComponentsCommandMode;
  TooltipWindow *m_pTooltipWindow;
  Component *m_pActiveComponent;
  int m_projectChangedConnectionId;
};

class CCSModesEditorWindow : public DialogWindow {
public:
  CCSModesEditorWindow(const String &name, const Colour &backgroundColour,
                       const bool escapeKeyTriggersCloseButton,
                       const bool addToDesktop, CCSManager *pManager)
      : DialogWindow(name, backgroundColour, escapeKeyTriggersCloseButton,
                     addToDesktop) {
    m_ppLastUsedComponent = NULL;
    m_pManager = pManager;
  };

  ~CCSModesEditorWindow() {}

  void closeButtonPressed() {
    setVisible(false);
    if (m_ppLastUsedComponent)
      safe_delete(*m_ppLastUsedComponent);
    m_pManager->getEditor()->deleteWindow();
  }

  void setMainComponent(Component **ppComponent) {
    removeComponent();
    m_ppLastUsedComponent = ppComponent;
  }

  void removeComponent() {
    if (m_ppLastUsedComponent)
      safe_delete(*m_ppLastUsedComponent);
    setContentNonOwned(NULL, false);
  }

  Component **getLastUsedComponent() { return m_ppLastUsedComponent; }

private:
  Component **m_ppLastUsedComponent;
  CCSManager *m_pManager;
};
