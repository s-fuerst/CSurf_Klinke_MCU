/**
 * Copyright (C) 2009-2013 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#ifdef _WIN32
#include <windows.h>
#else
#include "swell.h"
#endif

#include "Actions.h"
#include "ButtonManager.h"

extern reaper_plugin_info_t *g_rec;

bool hookCommandProc(int command, int flag) {
  return Actions::instance()->commandCallback(command, flag);
}

Actions *Actions::s_instance = NULL;

Actions::Actions() {}

Actions::Action::Action(const char *description, const char *id, int buttonId,
                        bool isButtonAction) {
  m_isButtonAction = isButtonAction;
  m_buttonId = buttonId;
  // if (! g_rec->Register("command_id_lookup",(void*)id)){
  gaccel_register_t acreg = {{0, 0, 0}, description};
  memcpy(&m_acreg, &acreg, sizeof(gaccel_register_t));
  m_acreg.accel.cmd = m_registered_command =
      g_rec->Register("command_id", (void *)id);
  g_rec->Register("gaccel", &m_acreg);
  //}
  m_buttonIsPressed = false;
}

Actions *Actions::instance() {
  if (s_instance == NULL) {
    s_instance = new Actions();
    s_instance->addActions();
  }
  return s_instance;
}

Actions::~Actions() {
  BOOST_FOREACH (char *pLit, m_literals)
    delete (pLit);

  BOOST_FOREACH (Action *pAction, m_actions)
    delete (pAction);

  s_instance = NULL;
}

void Actions::init(CSurf_MCU *pMCU) {
  m_pMCU = pMCU;
  g_rec->Register("hookcommand", (void *)hookCommandProc);
}

void Actions::addActions() {
  add8KeyActions("Rec %i", 0x00);
  add8KeyActions("Solo %i", 0x08);
  add8KeyActions("Mute %i", 0x10);
  add8KeyActions("Select %i", 0x18);
  add8KeyActions("VPot push %i", 0x20);
  addKeyAction("VPot Assign Track", 0x28);
  addKeyAction("VPot Assign Send", 0x29);
  addKeyAction("VPot Assign Pan/Surround", 0x2a);
  addKeyAction("VPot Assign Plug-in", 0x2b);
  addKeyAction("VPot Assign EQ", 0x2c);
  addKeyAction("VPot Assign Instrument", 0x2d);
  addKeyAction("Bank down", 0x2e);
  addKeyAction("Bank up", 0x2f);
  addKeyAction("Channel down", 0x30);
  addKeyAction("Channel up", 0x31);
  addKeyAction("Flip", 0x32);
  addKeyAction("Global View", 0x33);
  addButtonAction("Name/Value Display", 0x34);
  addKeyAction("SMTPE Beats", 0x35);
  add8KeyActions("F%i", 0x36);
  add8KeyActions("Global View %i", 0x3e);
  addButtonAction("Shift modifier", 0x46);
  addButtonAction("Option modifier", 0x47);
  addButtonAction("Control modifier", 0x48);
  addButtonAction("Alt modifier", 0x49);
  addKeyAction("Automation Read", 0x4a);
  addKeyAction("Automation Write", 0x4b);
  addKeyAction("Automation Trim", 0x4c);
  addKeyAction("Automation Touch", 0x4d);
  addKeyAction("Automation Latch", 0x4e);
  addKeyAction("Cancel", 0x52);
  addKeyAction("Marker", 0x54);
  addKeyAction("Nudge", 0x55);
  addKeyAction("Cycle", 0x56);
  addKeyAction("Drop", 0x57);
  addKeyAction("Click", 0x59);
  addKeyAction("Solo in Transport section", 0x5a);
  addKeyAction("Rewind", 0x5b);
  addKeyAction("Fast Fwd", 0x5c);
  addKeyAction("Stop", 0x5d);
  addKeyAction("Play", 0x5e);
  addKeyAction("Record", 0x5f);
  addKeyAction("Up", 0x60);
  addKeyAction("Down", 0x61);
  addKeyAction("Left", 0x62);
  addKeyAction("Right", 0x63);
  addKeyAction("Zoom", 0x64);
  addKeyAction("Scrub", 0x65);
  add8ButtonActions("Fader touch", 0x68);
  addButtonAction("Master fader touch", 0x70);
	addKeyAction("Reset all fader touch", 0x71);
}

bool Actions::commandCallback(int command, int flag) {
  BOOST_FOREACH (Action *pAction, m_actions) {
    if (command == pAction->getCommand()) {
      MIDI_event_t evt_down = {0, 3, {0x90, pAction->getButtonId(), 0x7F}};
      MIDI_event_t evt_up = {0, 3, {0x90, pAction->getButtonId(), 0x00}};
      if (pAction->isButtonAction()) {
        pAction->setButtonPressed(!pAction->isButtonPressed());
        m_pMCU->GetButtonManager()->dispatchMidiEvent(
            pAction->isButtonPressed() ? &evt_down : &evt_up);
      } else {
        m_pMCU->GetButtonManager()->dispatchMidiEvent(&evt_down);
        m_pMCU->GetButtonManager()->dispatchMidiEvent(&evt_up);
      }
      return true;
    }
  }
  return false;
}

// int Actions::toggleCallback(int command_id) {
// if (command_id && command_id==g_registered_command02)
//   return g_togglestate;
// // -1 if action not provided by this extension or is not togglable
//  return -1;
//}

void Actions::addKeyAction(const char *description, int buttonId) {
  // the description and id literals must created on the heap
  char *_description = new char[200];
  char *_id = new char[100];
#ifdef EXT_B
  sprintf(_description, "Mackie Control Klinke B: %s (key)", description);
  sprintf(_id, "MCUKLINKE%ikeyB", buttonId);
#else
  sprintf(_description, "Mackie Control Klinke: %s (key)", description);
  sprintf(_id, "MCUKLINKE%ikey", buttonId);
#endif
  m_literals.push_front(_description);
  m_literals.push_front(_id);
  m_actions.push_front(new Actions::Action(_description, _id, buttonId, false));

  addButtonAction(description, buttonId);
}

void Actions::addButtonAction(const char *description, int buttonId) {
  // the description and id literals must created on the heap
  char *_description = new char[200];
  char *_id = new char[100];
#ifdef EXT_B
  sprintf(_description, "Mackie Control Klinke B: %s (button)", description);
  sprintf(_id, "MCUKLINKE%ibuttonB", buttonId);
#else
  sprintf(_description, "Mackie Control Klinke: %s (button)", description);
  sprintf(_id, "MCUKLINKE%ibutton", buttonId);
#endif
  m_literals.push_front(_description);
  m_literals.push_front(_id);
  m_actions.push_front(new Actions::Action(_description, _id, buttonId, true));
}

void Actions::add8KeyActions(const char *description, int buttonId) {
  for (int i = 0; i < 8; i++) {
    char *_description = new char[80];
    sprintf(_description, description, i + 1);
    addKeyAction(_description, buttonId + i);
  }
}

void Actions::add8ButtonActions(const char *description, int buttonId) {
  for (int i = 0; i < 8; i++) {
    char *_description = new char[80];
    sprintf(_description, description, i + 1);
    addButtonAction(_description, buttonId + i);
  }
}
