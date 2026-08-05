/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

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

// --- Optional Run() phase timing probes ------------------------------------
// Enabled together with MCU_DEBUG_LOG by the MCU_TIMING CMake switch. Emits
// "[TIMING] <phase> <ms>" lines to the same debug log so the dominant phase
// in CSurf_MCU::Run() can be found by grepping the log. Inert (zero cost)
// when MCU_TIMING is not defined.
#ifdef MCU_TIMING
#include <chrono>
namespace McuTiming {
inline void logMs(const char *tag, double ms) {
  mcu_log_write("[TIMING] %-22s %8.3f ms", tag, ms);
}
// RAII scope that measures wall time from construction to destruction and
// logs it under the given tag. Usage: MCU_TIMING_SCOPE(adjust);
struct Scope {
  const char *name;
  std::chrono::steady_clock::time_point t0;
  Scope(const char *n) : name(n), t0(std::chrono::steady_clock::now()) {}
  ~Scope() {
    logMs(name, std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count());
  }
};
}  // namespace McuTiming
#define MCU_TIMING_SCOPE(name) McuTiming::Scope _mcu_tscope_##name(#name)
#define MCU_TIMING_LOG(...) mcu_log_write(__VA_ARGS__)
#else
#define MCU_TIMING_SCOPE(name) ((void)0)
#define MCU_TIMING_LOG(...) ((void)0)
#endif  // MCU_TIMING
