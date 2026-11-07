/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Per-unit plugin categories for the PlugMode "insert FX" command
 * (CTRL+VPOT4). Each hardware unit owns 16 categories (8 VPOTs x 2 with
 * SHIFT), each category holds up to 16 plugins (again 8 x 2 with SHIFT).
 *
 * Data is GLOBAL (not per project) and stored in the shared MCU config
 * directory:
 *   <MCU config>/PluginCategories.xml
 * Every change is written through immediately (no explicit save button).
 *
 * XML layout ("EMPTY" attributes are omitted):
 *   <KLINKE_PLUGIN_CATEGORIES version="1">
 *     <UNIT index="0">
 *       <CATEGORY index="0" name="Dyn">
 *         <PLUGIN slot="0" ident="VST3:..." short="Comp"/>
 *         ...
 *       </CATEGORY>
 *     </UNIT>
 *   </KLINKE_PLUGIN_CATEGORIES>
 */
#pragma once
#include "JuceHeader.h"

class PlugCategoryConfig {
public:
  static const int kNumUnits = 8;
  static const int kNumCategories = 16;         // 8 VPOTs x 2 (SHIFT)
  static const int kNumPluginsPerCategory = 16; // 8 VPOTs x 2 (SHIFT)
  // The MCU display field below a VPOT is 6 characters wide; category names
  // and short plugin labels must fit.
  static const int kMaxLabelLength = 6;

  struct PluginEntry {
    String ident;     // EnumInstalledFX ident ("" = empty slot)
    String shortName; // user label (<= 6 chars); "" = derive from the plugin
    bool isAssigned() const { return ident.isNotEmpty(); }
  };

  struct Category {
    String name; // <= 6 characters, "" = unnamed
    PluginEntry plugins[kNumPluginsPerCategory];
    bool isEmpty() const;
  };

  static PlugCategoryConfig *instance();

  // --- access (indices are clamped: out-of-range returns static empties) ---
  const Category &category(int unit, int categoryIndex) const;
  String categoryName(int unit, int categoryIndex) const;
  void setCategoryName(int unit, int categoryIndex, const String &name);
  // First category index whose name matches `name` (case-insensitive,
  // trimmed), or -1.
  int findCategoryByName(int unit, const String &name) const;

  const PluginEntry &plugin(int unit, int categoryIndex, int slot) const;
  void setPlugin(int unit, int categoryIndex, int slot, const String &ident,
                 const String &shortName);
  // Store a picked plugin and derive the short name when it is empty.
  void setPickedPlugin(int unit, int categoryIndex, int slot,
                       const String &ident, const String &displayName);

  // --- reordering (editor drag & drop) ---
  // Move the category at `from` to index `to`: every entry in between is
  // shifted by one (insert semantics, nothing is lost or swapped).
  void moveCategory(int unit, int from, int to);
  // Same for the plugin slots inside one category.
  void movePlugin(int unit, int categoryIndex, int from, int to);

  // At least one plugin entry in this category (the category can be opened
  // by the hardware insert flow).
  bool categoryHasPlugins(int unit, int categoryIndex) const;

  // True when the unit has at least one category name or plugin entry.
  bool unitHasData(int unit) const;

  // Label for the 6-character VPOT field: the user's short name, else the
  // plugin name (or, for a missing plugin, the file name of the stored
  // ident), truncated to kMaxLabelLength. "" for an empty slot.
  String pluginLabel(int unit, int categoryIndex, int slot) const;
  static String labelFor(const PluginEntry &entry);

  // --- persistence ---
  static File file();
  void load();
  void save() const;

private:
  PlugCategoryConfig();
  static String clampName(const String &name);

  Category m_categories[kNumUnits][kNumCategories];
  bool m_loaded;
};
