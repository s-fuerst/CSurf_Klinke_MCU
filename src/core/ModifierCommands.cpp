/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * ModifierCommands — see ModifierCommands.h for the layer description.
 */
#include "ModifierCommands.h"

void ModifierCommands::add(int modifier, int control, Handler handler) {
  m_entries.push_back(Entry{modifier, control, handler});
}

bool ModifierCommands::dispatch(int modifier, int control, int channel) {
  for (const Entry &e : m_entries) {
    if (e.modifier == modifier && e.control == control)
      return e.handler(channel);
  }
  return false; // no command matched
}

bool ModifierCommands::hasCommand(int modifier, int control) const {
  for (const Entry &e : m_entries)
    if (e.modifier == modifier && e.control == control)
      return true;
  return false;
}

bool ModifierCommands::hasCommands(int modifier) const {
  for (const Entry &e : m_entries)
    if (e.modifier == modifier)
      return true;
  return false;
}
