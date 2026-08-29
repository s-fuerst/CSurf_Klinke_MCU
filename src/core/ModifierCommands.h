/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * ModifierCommands — per-mode "modifier key + hardware control → command"
 * dispatch table.
 *
 * Layer 2 of the modifier command scheme
 * (ai-docs/modifier-command-scheme.md). One instance per mode, populated
 * in the mode's constructor. The table only ROUTES — precondition logic
 * (strip assignment, accessed plugin present, ...) and the command bodies
 * live inside the mode's handler.
 *
 * Deliberately NOT a global registry in CCSManager: the manager would
 * have to know per-mode control semantics, and the unit/channel context
 * is already resolved inside the mode.
 */
#pragma once
#include <functional>
#include <vector>

class ModifierCommands {
public:
  // channel: GLOBAL 1-based channel of the pressed control.
  // Return value: true = the event was consumed (even when the command was
  // inactive, e.g. its precondition failed); the caller must not fall
  // through to the normal behaviour of the control.
  using Handler = std::function<bool(int channel)>;

  // Register a command. modifier: a VK_* key (VK_CONTROL today).
  // control: the MODE-LOCAL positional index of the control (e.g. the
  // unshifted 0..7 VPOT index — hardware VPOT N is control N-1).
  void add(int modifier, int control, Handler handler);

  // Try to dispatch: finds the first entry matching (modifier, control)
  // and runs its handler. Returns the handler's value, or false when NO
  // entry matches (caller falls through to the normal behaviour).
  bool dispatch(int modifier, int control, int channel);

  // Does an entry exist for (modifier, control)? Used by callers to
  // distinguish "command matched but inactive" (consumed) from "no
  // command here" (fall through).
  bool hasCommand(int modifier, int control) const;

  // For legends: does this modifier have any commands at all?
  bool hasCommands(int modifier) const;

private:
  struct Entry {
    int modifier;
    int control;
    Handler handler;
  };
  std::vector<Entry> m_entries;
};
