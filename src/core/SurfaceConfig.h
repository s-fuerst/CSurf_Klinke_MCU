/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * WP-B (extender support): multi-unit surface configuration.
 *
 * Parses the surface config string (legacy 5-int or new KLINKE2 8-entry
 * format) into a typed SurfaceConfig, and serializes back to KLINKE2.
 * The dialog shows exactly 8 unit rows; the config mirrors this 1:1 with
 * 8 fixed entries.
 */

#ifndef MCU_SURFACE_CONFIG
#define MCU_SURFACE_CONFIG

#include "HardwareUnit.h" // UnitConfig, DeviceModel
#include <string>

#define MAX_SURFACE_UNITS 8

// Unit type tokens (persisted in KLINKE2 config string).
// Encoding order matches the CB_SETITEMDATA encoding used in the dialog.
#define UNIT_TYPE_MACKIE_MAIN 0
#define UNIT_TYPE_MACKIE_EXT  1
#define UNIT_TYPE_PROX_MAIN   2
#define UNIT_TYPE_PROX_EXT    3
#define UNIT_TYPE_DISABLED    4

struct SurfaceConfig {
  int flags;                             // CONFIG_FLAG_* bits; PROX derived from units[0].model
  UnitConfig units[MAX_SURFACE_UNITS];   // always 8 entries — position = unit index
  bool valid;                            // false if parse error → safe default used
};

SurfaceConfig makeDefaultSurfaceConfig();

// Parses a config string. Handles both:
//   legacy: "0 8 <midiIn> <midiOut> <flags>"
//   KLINKE2: "KLINKE2 flags=<flags> <in>,<out>,<type> ..." (8 entries)
SurfaceConfig parseSurfaceConfig(const char *str);

// Serializes to KLINKE2 format, always 8 entries.
std::string serializeSurfaceConfig(const SurfaceConfig &cfg);

// Converts isMain/model to the stable KLINKE2 type token.
const char *unitTypeToken(const UnitConfig &cfg);

// Returns the UnitConfig for a given unit type index (0..3).
UnitConfig unitConfigFromType(int typeIndex, int midiIn, int midiOut);

// The one definition of Disabled used by dialog, parser, and constructor.
bool unitIsDisabled(const UnitConfig &cfg);

// Dense topology: active units must be contiguous from position 0.
// Once a unit is Disabled, all later units must be Disabled.
// Unit 0 may never be Disabled.
bool hasDenseUnitTopology(const SurfaceConfig &cfg);

#endif
