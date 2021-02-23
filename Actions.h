/**
 * Copyright (C) 2009-2013 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#pragma once
#include "csurf_mcu.h"
#include <string>
#include <list>
#include "boost\foreach.hpp"
#include "csurf_mcu.h"

class Actions {
  class Action {
  public:
    Action(const char *description, const char *id, int buttonId,
           bool isButtonAction);
    int getCommand() { return m_registered_command; }
    int getButtonId() { return m_buttonId; }
    bool isButtonAction() { return m_isButtonAction; }
    bool isButtonPressed() { return m_buttonIsPressed; }
    void setButtonPressed(bool isPressed) { m_buttonIsPressed = isPressed; }

  private:
    gaccel_register_t m_acreg;
    int m_registered_command;
    int m_buttonId;
    bool m_isButtonAction;
    bool m_buttonIsPressed;
  };

public:
  static Actions *instance();
  void init(CSurf_MCU *pMCU);
  virtual ~Actions();

  bool commandCallback(int command, int flag);

private:
  Actions(void);
  static Actions *s_instance;
  void addActions();
  void addKeyAction(const char *description, int buttonId);
  void addButtonAction(const char *description, int buttonId);
  void add8KeyActions(const char *description, int buttonId);
  void add8ButtonActions(const char *description, int buttonId);

  typedef std::list<Action *> tActions;
  typedef std::list<char *> tLiterals;
  tActions m_actions;
  tLiterals m_literals;
  CSurf_MCU *m_pMCU;
};
