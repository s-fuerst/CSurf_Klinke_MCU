/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Shared "installed FX" list (EnumInstalledFX) plus the name/ident helpers
 * around it.
 *
 * Two on-screen editors pick a plugin from the installed list and remember
 * its IDENT for a later TrackFX_AddByName() call:
 *   • the Channel Strip editor (ChannelStripBindingTable::pluginPicked)
 *   • the PlugMode plugin category editor (PlugCategoryEditor)
 * Both go through this module so the enumeration, the session cache and the
 * ident matching behave identically.
 *
 * Ident conventions (REAPER's EnumInstalledFX):
 *   • JS     -> "JS: <jsfx path>"
 *   • VST/VST3/CLAP/AU -> a FILE PATH (e.g. "/path/reaeq.vst.so")
 * The DISPLAY NAME on the other hand looks like "VST3: ReaEQ (Cockos)",
 * i.e. it carries a "TYPE: " prefix and a " (Manufacturer)" suffix.
 */
#pragma once
#include "JuceHeader.h"
#include <vector>

struct InstalledFx {
  String name;  // display name, e.g. "ReaEQ (Cockos)"
  String ident; // AddByName key (see above)
};

typedef std::vector<InstalledFx> InstalledFxList;

class InstalledFxCache {
public:
  // Raw enumeration of the installed list (no caching, safe to call after
  // installing a plugin at runtime). `out` is cleared first.
  static void enumerate(InstalledFxList &out);

  // Session cache of the enumeration (refreshed on the first call and by
  // refresh()). Reference stays valid until the next refresh().
  static const InstalledFxList &get();
  static void refresh();

  // Display name of the installed FX whose ident equals `ident` ("" if the
  // ident is empty or not installed). Used to match file-path idents against
  // on-track FX display names.
  static String nameForIdent(const String &ident);

  // Strip a leading "TYPE:" prefix and surrounding whitespace, so
  // "VST3: ReaEQ (Cockos)" and "ReaEQ (Cockos)" compare equal.
  static String normalizeName(const String &nameOrIdent);

  // Short label derived from a plugin/ident name: no TYPE prefix, no
  // trailing " (Manufacturer)" suffix, trimmed (e.g. "VST3: ReaEQ (Cockos)"
  // -> "ReaEQ"). Empty if nothing usable remains.
  static String shortLabel(const String &nameOrIdent);
};
