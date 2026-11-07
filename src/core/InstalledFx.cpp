/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "InstalledFx.h"
#include "csurf.h" // EnumInstalledFX (vendored)

void InstalledFxCache::enumerate(InstalledFxList &out) {
  out.clear();
  if (!EnumInstalledFX)
    return;
  // index = -1 re-reads JSFX info (REAPER 7.42+); ignore its boolean return.
  EnumInstalledFX(-1, NULL, NULL);
  for (int i = 0;; i++) {
    const char *name = NULL;
    const char *ident = NULL;
    if (!EnumInstalledFX(i, &name, &ident) || !ident)
      break;
    InstalledFx fx;
    fx.name = name ? String(name) : String();
    fx.ident = String(ident);
    out.push_back(fx);
  }
}

namespace {
InstalledFxList &cachedList() {
  static InstalledFxList list;
  return list;
}

bool &cacheValid() {
  static bool valid = false;
  return valid;
}
} // namespace

const InstalledFxList &InstalledFxCache::get() {
  if (!cacheValid()) {
    enumerate(cachedList());
    cacheValid() = true;
  }
  return cachedList();
}

void InstalledFxCache::refresh() {
  enumerate(cachedList());
  cacheValid() = true;
}

String InstalledFxCache::nameForIdent(const String &ident) {
  if (ident.isEmpty())
    return String();
  for (const auto &fx : get())
    if (fx.ident == ident)
      return fx.name;
  return String();
}

String InstalledFxCache::normalizeName(const String &nameOrIdent) {
  String s = nameOrIdent.trim();
  // Strip a leading "TYPE:" prefix (e.g. "VST3:", "JS:", "AU:").
  int colon = s.indexOfChar(':');
  if (colon >= 0)
    s = s.substring(colon + 1);
  return s.trim();
}

String InstalledFxCache::shortLabel(const String &nameOrIdent) {
  String base = normalizeName(nameOrIdent);
  if (base.isEmpty())
    base = nameOrIdent.trim();
  int paren = base.indexOfChar('(');
  if (paren > 0)
    base = base.substring(0, paren).trimEnd();
  return base;
}
