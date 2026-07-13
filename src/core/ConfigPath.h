/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Shared config-path helper. Returns the MCU config directory in the SAME
 * location on every platform, matching the established Options::getConfigFile()
 * pattern (src/core/Options.cpp):
 *   Windows : <Documents>\\Reaper\\MCU\\Config\\
 *   others  : <GetResourcePath()>/MCU/Config/
 * All MCU XML config files (configurations.xml, GlobalActions.xml,
 * ActionMode.xml) live together here on every platform.
 */
#pragma once
#include "JuceHeader.h"
#include "csurf.h" // GetResourcePath (non-Windows)

// Returns the MCU config directory, creating it if it does not yet exist.
inline File getMcuConfigDir() {
#ifdef _WIN32
  File configDir =
      File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() +
      String("\\Reaper\\MCU\\Config\\");
#else
  File configDir = String(GetResourcePath()) + String("/MCU/Config/");
#endif
  if (!configDir.exists())
    configDir.createDirectory();
  return configDir;
}

// Returns <configDir>/<name>.
inline File getMcuConfigFile(const String &name) {
  return getMcuConfigDir().getChildFile(name);
}
