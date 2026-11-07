/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "CCSModesEditor.h"
#include "KlinkeLookAndFeel.h"
#include "CommandMode.h"
#include "McuDebugLog.h"
#include <boost/bind.hpp>

CCSModesEditor::CCSModesEditor(CCSManager *pManager)
    : m_pManager(pManager), m_pWindow(NULL), m_pActiveComponent(NULL) {
  //  m_pWindow = new CCSModesEditorWindow("MCU Editor", Colours::white, false,
  //  true, pManager);
  m_pTooltipWindow = new TooltipWindow();

  m_projectChangedConnectionId =
      ProjectConfig::instance()->connect2ProjectChangeSignal(
          boost::bind(&CCSModesEditor::projectChanged, this, _1, _2));
}

CCSModesEditor::~CCSModesEditor(void) {
  ProjectConfig::instance()->disconnectProjectChangeSignal(
      m_projectChangedConnectionId);

  safe_delete(m_pTooltipWindow);
}

void CCSModesEditor::projectChanged(XmlElement *pXmlElement,
                                    ProjectConfig::EAction action) {
  if (action == ProjectConfig::FREE) {
    deleteWindow();
  }
}

void CCSModesEditor::setMainComponent(CCSMode *pCommandMode, bool visible) {
  setMainComponent(pCommandMode->createEditorComponent(), visible);
  m_pComponentsCommandMode = pCommandMode;
}

void CCSModesEditor::setMainComponent(Component **ppComponent, bool visible) {
  openWindow(ppComponent, visible, false);
}

void CCSModesEditor::setComponentWindow(Component **ppComponent, bool visible) {
  openWindow(ppComponent, visible, true);
}

void CCSModesEditor::openWindow(Component **ppComponent, bool visible,
                                bool trackPointer) {
  // `trackPointer` hands the pointer to the window, which then deletes the
  // component and NULLs the caller's pointer when the window closes or is
  // rebuilt. Without it (the CCSMode editor path) the MODE stays the owner
  // and frees its component in deleteEditorComponent(). Getting this wrong
  // leaves the caller with a dangling pointer: the window owns the content
  // (setContentOwned) and deletes it either way.
	if (m_pWindow && m_pActiveComponent && m_pActiveComponent == *ppComponent) {
		if (m_pWindow->isVisible()) {
      MCU_LOG("EDITOR toggle-hide (same component)");
			m_pWindow->setVisible(false);
			return;
		}
    MCU_LOG("EDITOR re-show (same component)");
		m_pWindow->setVisible(visible);
		m_pWindow->setAlwaysOnTop(true);
		m_pWindow->setTopLeftPosition(20, 60);
		return;
  }
  MCU_LOG("EDITOR new-window visible=%d", visible);
  deleteWindow();
  m_pComponentsCommandMode = NULL;
  m_pWindow = new CCSModesEditorWindow("MCU Editor", Colours::white, false,
                                       true, m_pManager);
  m_pWindow->setUsingNativeTitleBar(true);

  // Must happen BEFORE the content is set: the window's setMainComponent()
  // first clears any previous content.
  if (trackPointer)
    m_pWindow->setMainComponent(ppComponent);

  m_pWindow->setContentOwned(*ppComponent, true);
  static KlinkeLookAndFeel klf;  // keep alive while any window uses it
  (*ppComponent)->setLookAndFeel (&klf);
  if (visible)
    m_pWindow->setVisible(visible);
  m_pWindow->setAlwaysOnTop(true);
  m_pWindow->setTopLeftPosition(20, 60);
#ifdef EASY_DEBUG
  m_pWindow->setTopLeftPosition(1280, 20);
#endif
  m_pActiveComponent = *ppComponent;
}

void CCSModesEditor::closeWindowAndRemoveComponent(Component *pComponent) {
  if (m_pWindow && m_pActiveComponent && pComponent == m_pActiveComponent) {
    m_pWindow->setVisible(false);
  }
}

void CCSModesEditor::deleteWindow() {
  if (m_pWindow) {
    safe_call(m_pComponentsCommandMode, deleteEditorComponent());
    m_pWindow->removeFromDesktop();
    // Also frees the component when the window tracks the pointer
    // (setComponentWindow): the window then deletes it and NULLs the
    // caller's Component**.
    m_pWindow->removeComponent();
    safe_delete(m_pWindow);
    m_pComponentsCommandMode = NULL;
    // Never leave a dangling comparison target behind (closeWindowAndRemove
    // Component() and the toggle check in openWindow() compare against it).
    m_pActiveComponent = NULL;
  }
}
