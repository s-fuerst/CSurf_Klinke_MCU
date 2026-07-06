/**
 * Lightweight debug log for the MCU Klinke plugin.
 *
 * Define MCU_DEBUG_LOG before including this header (or in CMake) to enable.
 * Log file: <REAPER resource path>/mcu_klinke_debug.log
 *
 * Usage:
 *   MCU_LOG("OnSolo ch=%d pressed=%d", channel, pressed);
 */

#pragma once
#include "csurf.h"  // GetResourcePath
#include <cstdio>
#include <cstring>
#include <cstdarg>

#ifdef MCU_DEBUG_LOG

static inline void mcu_log_write(const char *fmt, ...) {
  static char path[512] = {};
  if (!path[0]) {
    const char *rp = GetResourcePath ? GetResourcePath() : "/tmp";
    snprintf(path, sizeof(path), "%s/mcu_klinke_debug.log", rp);
  }
  FILE *f = fopen(path, "a");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fprintf(f, "\n");
  fclose(f);
}

#define MCU_LOG(...) mcu_log_write(__VA_ARGS__)

// Call once at startup to timestamp and clear the log
static inline void mcu_log_init() {
  static char path[512] = {};
  const char *rp = GetResourcePath ? GetResourcePath() : "/tmp";
  snprintf(path, sizeof(path), "%s/mcu_klinke_debug.log", rp);
  FILE *f = fopen(path, "w");  // truncate
  if (!f) return;
  fprintf(f, "=== MCU Klinke debug log started ===\n");
  fclose(f);
}
#define MCU_LOG_INIT() mcu_log_init()

#else

#define MCU_LOG(...)    ((void)0)
#define MCU_LOG_INIT()  ((void)0)

#endif  // MCU_DEBUG_LOG
