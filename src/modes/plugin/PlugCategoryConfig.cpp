/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PlugCategoryConfig.h"
#include "ConfigPath.h"
#include "InstalledFx.h"
#include "McuDebugLog.h"

#define PCC_ATT_VERSION String("version")
#define PCC_TAG_ROOT String("KLINKE_PLUGIN_CATEGORIES")
#define PCC_TAG_UNIT String("UNIT")
#define PCC_TAG_CATEGORY String("CATEGORY")
#define PCC_TAG_PLUGIN String("PLUGIN")
#define PCC_ATT_INDEX String("index")
#define PCC_ATT_NAME String("name")
#define PCC_ATT_SLOT String("slot")
#define PCC_ATT_IDENT String("ident")
#define PCC_ATT_SHORT String("short")
#define PCC_FILE_NAME String("PluginCategories.xml")

namespace {
const PlugCategoryConfig::Category &emptyCategory() {
  static const PlugCategoryConfig::Category c;
  return c;
}

const PlugCategoryConfig::PluginEntry &emptyEntry() {
  static const PlugCategoryConfig::PluginEntry e;
  return e;
}

bool validUnit(int unit) {
  return unit >= 0 && unit < PlugCategoryConfig::kNumUnits;
}
bool validCategory(int c) {
  return c >= 0 && c < PlugCategoryConfig::kNumCategories;
}
bool validSlot(int s) {
  return s >= 0 && s < PlugCategoryConfig::kNumPluginsPerCategory;
}

// Move arr[from] to arr[to], shifting everything in between (insert
// semantics). Used by the editor's drag & drop reordering.
template <typename T, int N> void moveInArray(T (&arr)[N], int from, int to) {
  if (from < 0 || from >= N || to < 0 || to >= N || from == to)
    return;
  T tmp = arr[from];
  if (from < to) {
    for (int i = from; i < to; i++)
      arr[i] = arr[i + 1];
  } else {
    for (int i = from; i > to; i--)
      arr[i] = arr[i - 1];
  }
  arr[to] = tmp;
}
} // namespace

bool PlugCategoryConfig::Category::isEmpty() const {
  if (name.isNotEmpty())
    return false;
  for (int i = 0; i < kNumPluginsPerCategory; i++)
    if (plugins[i].isAssigned())
      return false;
  return true;
}

PlugCategoryConfig::PlugCategoryConfig() : m_loaded(false) {
  // Loaded on first use (the static below is created lazily).
  load();
}

PlugCategoryConfig *PlugCategoryConfig::instance() {
  static PlugCategoryConfig s_instance;
  return &s_instance;
}

File PlugCategoryConfig::file() { return getMcuConfigFile(PCC_FILE_NAME); }

String PlugCategoryConfig::clampName(const String &name) {
  return name.substring(0, kMaxLabelLength);
}

const PlugCategoryConfig::Category &
PlugCategoryConfig::category(int unit, int categoryIndex) const {
  if (!validUnit(unit) || !validCategory(categoryIndex))
    return emptyCategory();
  return m_categories[unit][categoryIndex];
}

String PlugCategoryConfig::categoryName(int unit, int categoryIndex) const {
  return category(unit, categoryIndex).name;
}

void PlugCategoryConfig::setCategoryName(int unit, int categoryIndex,
                                         const String &name) {
  if (!validUnit(unit) || !validCategory(categoryIndex))
    return;
  String n = clampName(name);
  if (m_categories[unit][categoryIndex].name == n)
    return;
  m_categories[unit][categoryIndex].name = n;
  save();
}

int PlugCategoryConfig::findCategoryByName(int unit,
                                           const String &name) const {
  String want = name.trim();
  if (want.isEmpty())
    return -1;
  for (int i = 0; i < kNumCategories; i++)
    if (m_categories[unit][i].name.compareIgnoreCase(want) == 0)
      return i;
  return -1;
}

const PlugCategoryConfig::PluginEntry &
PlugCategoryConfig::plugin(int unit, int categoryIndex, int slot) const {
  if (!validUnit(unit) || !validCategory(categoryIndex) || !validSlot(slot))
    return emptyEntry();
  return m_categories[unit][categoryIndex].plugins[slot];
}

void PlugCategoryConfig::setPlugin(int unit, int categoryIndex, int slot,
                                   const String &ident,
                                   const String &shortName) {
  if (!validUnit(unit) || !validCategory(categoryIndex) || !validSlot(slot))
    return;
  PluginEntry &e = m_categories[unit][categoryIndex].plugins[slot];
  String s = clampName(shortName);
  if (e.ident == ident && e.shortName == s)
    return;
  e.ident = ident;
  e.shortName = s;
  save();
}

void PlugCategoryConfig::setPickedPlugin(int unit, int categoryIndex, int slot,
                                         const String &ident,
                                         const String &displayName) {
  if (!validUnit(unit) || !validCategory(categoryIndex) || !validSlot(slot))
    return;
  if (ident.isEmpty()) {
    setPlugin(unit, categoryIndex, slot, String(), String());
    return;
  }
  // A newly picked plugin overwrites a user short name (the old one would
  // describe the wrong plugin). The derived default is truncated by
  // labelFor().
  String derived = InstalledFxCache::shortLabel(displayName);
  setPlugin(unit, categoryIndex, slot, ident, clampName(derived));
}

void PlugCategoryConfig::moveCategory(int unit, int from, int to) {
  if (!validUnit(unit) || !validCategory(from) || !validCategory(to) ||
      from == to)
    return;
  MCU_LOG("PCC moveCategory unit=%d %d -> %d", unit + 1, from, to);
  moveInArray(m_categories[unit], from, to);
  save();
}

void PlugCategoryConfig::movePlugin(int unit, int categoryIndex, int from,
                                    int to) {
  if (!validUnit(unit) || !validCategory(categoryIndex) || !validSlot(from) ||
      !validSlot(to) || from == to)
    return;
  MCU_LOG("PCC movePlugin unit=%d cat=%d %d -> %d", unit + 1, categoryIndex,
          from, to);
  moveInArray(m_categories[unit][categoryIndex].plugins, from, to);
  save();
}

bool PlugCategoryConfig::categoryHasPlugins(int unit,
                                           int categoryIndex) const {
  if (!validUnit(unit) || !validCategory(categoryIndex))
    return false;
  const Category &c = m_categories[unit][categoryIndex];
  for (int s = 0; s < kNumPluginsPerCategory; s++)
    if (c.plugins[s].isAssigned())
      return true;
  return false;
}

bool PlugCategoryConfig::unitHasData(int unit) const {
  if (!validUnit(unit))
    return false;
  for (int c = 0; c < kNumCategories; c++)
    if (!m_categories[unit][c].isEmpty())
      return true;
  return false;
}

String PlugCategoryConfig::labelFor(const PluginEntry &entry) {
  if (!entry.isAssigned())
    return String();
  if (entry.shortName.isNotEmpty())
    return entry.shortName.substring(0, kMaxLabelLength);
  String name = InstalledFxCache::nameForIdent(entry.ident);
  if (name.isEmpty()) {
    // Not installed (or another machine): fall back to the file name of the
    // stored ident (VST/VST3/CLAP store a path).
    File f(entry.ident);
    name = f.getFileNameWithoutExtension();
  }
  if (name.isEmpty())
    name = entry.ident;
  return InstalledFxCache::shortLabel(name).substring(0, kMaxLabelLength);
}

String PlugCategoryConfig::pluginLabel(int unit, int categoryIndex,
                                       int slot) const {
  return labelFor(plugin(unit, categoryIndex, slot));
}

// --- persistence -----------------------------------------------------------

void PlugCategoryConfig::load() {
  for (int u = 0; u < kNumUnits; u++)
    for (int c = 0; c < kNumCategories; c++)
      m_categories[u][c] = Category();

  File f = file();
  if (!f.existsAsFile()) {
    MCU_LOG("PCC load: no file at [%s]", f.getFullPathName().toRawUTF8());
    m_loaded = true;
    return;
  }
  std::unique_ptr<XmlElement> xml(XmlDocument::parse(f));
  if (!xml || !xml->hasTagName(PCC_TAG_ROOT)) {
    MCU_LOG("PCC load: parse failed [%s]", f.getFullPathName().toRawUTF8());
    m_loaded = true;
    return;
  }
  int nPlugs = 0;
  int nCats = 0;
  for (auto *unitEl : xml->getChildWithTagNameIterator(PCC_TAG_UNIT)) {
    int u = unitEl->getIntAttribute(PCC_ATT_INDEX, -1);
    if (!validUnit(u))
      continue;
    for (auto *catEl : unitEl->getChildWithTagNameIterator(PCC_TAG_CATEGORY)) {
      int c = catEl->getIntAttribute(PCC_ATT_INDEX, -1);
      if (!validCategory(c))
        continue;
      m_categories[u][c].name =
          clampName(catEl->getStringAttribute(PCC_ATT_NAME));
      nCats++;
      for (auto *plugEl :
           catEl->getChildWithTagNameIterator(PCC_TAG_PLUGIN)) {
        int slot = plugEl->getIntAttribute(PCC_ATT_SLOT, -1);
        if (!validSlot(slot))
          continue;
        PluginEntry &e = m_categories[u][c].plugins[slot];
        e.ident = plugEl->getStringAttribute(PCC_ATT_IDENT);
        e.shortName = clampName(plugEl->getStringAttribute(PCC_ATT_SHORT));
        nPlugs++;
      }
    }
  }
  m_loaded = true;
  MCU_LOG("PCC load: %d categor(ies), %d plugin(s) from [%s]", nCats, nPlugs,
          f.getFullPathName().toRawUTF8());
  // The counters only exist for the (optional) debug log.
  juce::ignoreUnused(nCats, nPlugs);
}

void PlugCategoryConfig::save() const {
  XmlElement root(PCC_TAG_ROOT);
  root.setAttribute(PCC_ATT_VERSION, 1);
  for (int u = 0; u < kNumUnits; u++) {
    bool anyUnitData = false;
    for (int c = 0; c < kNumCategories && !anyUnitData; c++)
      anyUnitData = !m_categories[u][c].isEmpty();
    if (!anyUnitData)
      continue;
    XmlElement *unitEl = root.createNewChildElement(PCC_TAG_UNIT);
    unitEl->setAttribute(PCC_ATT_INDEX, u);
    for (int c = 0; c < kNumCategories; c++) {
      const Category &cat = m_categories[u][c];
      if (cat.isEmpty())
        continue;
      XmlElement *catEl = unitEl->createNewChildElement(PCC_TAG_CATEGORY);
      catEl->setAttribute(PCC_ATT_INDEX, c);
      if (cat.name.isNotEmpty())
        catEl->setAttribute(PCC_ATT_NAME, cat.name);
      for (int s = 0; s < kNumPluginsPerCategory; s++) {
        const PluginEntry &e = cat.plugins[s];
        if (!e.isAssigned())
          continue;
        XmlElement *plugEl = catEl->createNewChildElement(PCC_TAG_PLUGIN);
        plugEl->setAttribute(PCC_ATT_SLOT, s);
        plugEl->setAttribute(PCC_ATT_IDENT, e.ident);
        if (e.shortName.isNotEmpty())
          plugEl->setAttribute(PCC_ATT_SHORT, e.shortName);
      }
    }
  }
  File f = file();
  if (!root.writeTo(f)) {
    MCU_LOG("PCC save FAILED [%s]", f.getFullPathName().toRawUTF8());
    return;
  }
  MCU_LOG("PCC save ok [%s]", f.getFullPathName().toRawUTF8());
}
