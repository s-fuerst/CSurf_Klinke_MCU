/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * WP-B: SurfaceConfig parser, serializer, and unit type helpers.
 */

#include "SurfaceConfig.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// --- Unit type token ↔ config conversion ---

static const char *s_typeTokens[] = {
    "mackie-main", // 0
    "mackie-ext",  // 1
    "prox-main",   // 2
    "prox-ext",    // 3
    "disabled",    // 4
};

const char *unitTypeToken(const UnitConfig &cfg) {
  // Disabled: both MIDI devices are -1 and not a main unit
  if (cfg.midiInDev == -1 && cfg.midiOutDev == -1 && !cfg.isMain)
    return s_typeTokens[4];
  if (cfg.isMain)
    return (cfg.model == QConProX) ? s_typeTokens[2] : s_typeTokens[0];
  else
    return (cfg.model == QConProX) ? s_typeTokens[3] : s_typeTokens[1];
}

UnitConfig unitConfigFromType(int typeIndex, int midiIn, int midiOut) {
  UnitConfig cfg;
  cfg.midiInDev = midiIn;
  cfg.midiOutDev = midiOut;
  switch (typeIndex) {
    case UNIT_TYPE_MACKIE_MAIN: cfg.isMain = true;  cfg.model = Mackie;   break;
    case UNIT_TYPE_MACKIE_EXT:  cfg.isMain = false; cfg.model = Mackie;   break;
    case UNIT_TYPE_PROX_MAIN:   cfg.isMain = true;  cfg.model = QConProX; break;
    case UNIT_TYPE_PROX_EXT:    cfg.isMain = false; cfg.model = QConProX; break;
    case UNIT_TYPE_DISABLED:
    default:                    cfg.isMain = false; cfg.model = Mackie;
                                cfg.midiInDev = -1; cfg.midiOutDev = -1;  break;
  }
  return cfg;
}

bool unitIsDisabled(const UnitConfig &cfg) {
  return (cfg.midiInDev == -1 && cfg.midiOutDev == -1 && !cfg.isMain);
}

bool hasDenseUnitTopology(const SurfaceConfig &cfg) {
  // Unit 0 must never be Disabled.
  if (unitIsDisabled(cfg.units[0]))
    return false;
  // Once a unit is Disabled, all later units must be Disabled.
  bool foundDisabled = false;
  for (int i = 1; i < MAX_SURFACE_UNITS; i++) {
    if (foundDisabled) {
      if (!unitIsDisabled(cfg.units[i]))
        return false;
    } else {
      if (unitIsDisabled(cfg.units[i]))
        foundDisabled = true;
    }
  }
  return true;
}

// --- Default config ---

SurfaceConfig makeDefaultSurfaceConfig() {
  SurfaceConfig cfg;
  cfg.flags = 0;
  cfg.valid = true;
  // Unit 1 (index 0): Mackie main, MIDI None
  cfg.units[0] = unitConfigFromType(UNIT_TYPE_MACKIE_MAIN, -1, -1);
  // Units 2–8: Disabled by default
  for (int i = 1; i < MAX_SURFACE_UNITS; i++)
    cfg.units[i] = unitConfigFromType(UNIT_TYPE_DISABLED, -1, -1);
  return cfg;
}

// --- Tokenizing helpers ---

// Split a string into whitespace-delimited tokens.
static std::vector<std::string> tokenize(const char *str) {
  std::vector<std::string> tokens;
  if (!str) return tokens;

  const char *p = str;
  while (*p) {
    // skip whitespace
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;

    const char *start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    tokens.push_back(std::string(start, p - start));
  }
  return tokens;
}

// Split a string by comma delimiter.
static std::vector<std::string> splitComma(const std::string &s) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= s.size()) {
    size_t end = s.find(',', start);
    if (end == std::string::npos) end = s.size();
    parts.push_back(s.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

// --- KLINKE2 parser ---

static int parseTypeToken(const std::string &token) {
  for (int i = 0; i < 5; i++) {
    if (token == s_typeTokens[i]) return i;
  }
  return -1; // unknown
}

static SurfaceConfig makeFailedConfig() {
  SurfaceConfig cfg = makeDefaultSurfaceConfig();
  cfg.valid = false;
  return cfg;
}

static SurfaceConfig parseKlinke2(const std::vector<std::string> &tokens) {
  // Need at least 10 tokens: "KLINKE2" + "flags=N" + 8 unit entries
  if (tokens.size() < 10)
    return makeFailedConfig();

  // First token is already consumed ("KLINKE2"). Next must be flags=N.
  if (tokens[1].compare(0, 6, "flags=") != 0)
    return makeFailedConfig();

  int cfgFlags = atoi(tokens[1].c_str() + 6);

  // Parse exactly 8 unit entries
  SurfaceConfig cfg;
  cfg.flags = cfgFlags;
  cfg.valid = true;

  for (int i = 0; i < MAX_SURFACE_UNITS; i++) {
    int tokIdx = 2 + i;
    if (tokIdx >= (int)tokens.size())
      return makeFailedConfig();

    std::vector<std::string> parts = splitComma(tokens[tokIdx]);
    if (parts.size() != 3)
      return makeFailedConfig();

    int inDev = atoi(parts[0].c_str());
    int outDev = atoi(parts[1].c_str());
    int typeIdx = parseTypeToken(parts[2]);

    // Unknown type → use Disabled (safe default)
    if (typeIdx < 0) {
      typeIdx = UNIT_TYPE_DISABLED;
    }

    cfg.units[i] = unitConfigFromType(typeIdx, inDev, outDev);
  }

  // Derive PROX flag from unit 1's model (backward compat shim)
  if (cfg.units[0].model == QConProX)
    cfg.flags |= 16; // CONFIG_FLAG_PROX
  else
    cfg.flags &= ~16;

  return cfg;
}

// --- Legacy 5-int parser ---

static SurfaceConfig parseLegacy(const std::vector<std::string> &tokens) {
  SurfaceConfig cfg = makeDefaultSurfaceConfig();
  cfg.valid = true;

  // Parse up to 5 ints: offset size indev outdev flags
  int parms[5] = {0, 8, -1, -1, 0};
  for (size_t i = 0; i < tokens.size() && i < 5; i++)
    parms[i] = atoi(tokens[i].c_str());

  // offset and size are forced (v0.8+ no longer supports extenders)
  // but we keep the parse for backward compat

  // Populate unit 1 from legacy params
  DeviceModel model = (parms[4] & 16) ? QConProX : Mackie;
  cfg.units[0] = unitConfigFromType(
      model == QConProX ? UNIT_TYPE_PROX_MAIN : UNIT_TYPE_MACKIE_MAIN,
      parms[2], parms[3]);
  cfg.flags = parms[4];

  // Units 2–8 stay at defaults (Disabled)
  // cfg.valid is true — no error for empty strings or simple digit strings

  return cfg;
}

// --- Public API ---

SurfaceConfig parseSurfaceConfig(const char *str) {
  // Empty or null → default config
  if (!str || !*str)
    return makeDefaultSurfaceConfig();

  // Trim leading whitespace
  while (*str == ' ' || *str == '\t') str++;
  if (!*str)
    return makeDefaultSurfaceConfig();

  std::vector<std::string> tokens = tokenize(str);
  if (tokens.empty())
    return makeDefaultSurfaceConfig();

  // Detect format by first token
  if (tokens[0] == "KLINKE2")
    return parseKlinke2(tokens);

  // Legacy: first token is a digit or hypen (negative number)
  if ((tokens[0][0] >= '0' && tokens[0][0] <= '9') || tokens[0][0] == '-')
    return parseLegacy(tokens);

  // Unknown format → safe default
  SurfaceConfig cfg = makeDefaultSurfaceConfig();
  cfg.valid = false;
  return cfg;
}

std::string serializeSurfaceConfig(const SurfaceConfig &cfg) {
  char buf[1024];
  int pos = snprintf(buf, sizeof(buf), "KLINKE2 flags=%d", cfg.flags);
  for (int i = 0; i < MAX_SURFACE_UNITS; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, " %d,%d,%s",
                    cfg.units[i].midiInDev,
                    cfg.units[i].midiOutDev,
                    unitTypeToken(cfg.units[i]));
  }
  return std::string(buf);
}
