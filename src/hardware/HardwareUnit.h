/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Represents one physical MCU unit's hardware concerns.
 *
 * CSurf_MCU owns N HardwareUnits and becomes the single *logical* surface.
 *
 *   outgoing  : CSurf_MCU translates global -> local before calling these
 *   incoming  : the unit parses raw MIDI and emits GLOBAL channel events
 *               (1..N*8) by adding its own m_unitIndex*8 offset
 */
#ifndef MCU_HARDWARE_UNIT
#define MCU_HARDWARE_UNIT

#include "csurf.h"
#include "mcu_button_defines.h" // LED_ON / LED_OFF / LED_BLINK / LED_BLINK_BYPASSED

class DisplayHandler;
class CSurf_MCU;

// QConProX => 2 LCD panels plus the device-specific protocol quirks
// (VPOT-ring byte3=0, meter-bridge master path, LED-blink handling,
// assignment-display suppression, ...). lcdPanels = isProX()?2:1.
enum DeviceModel { Mackie, QConProX };

// Per-unit option bits (persisted in the KLINKE2 config string as the 4th
// field of every unit entry).
// Bit 0 (value 1) was UNIT_FLAG_FADER_TOUCH_FAKE — removed; do not reuse.
// Bit 1 (value 2) was UNIT_FLAG_EMULATE_BLINKING — removed; LED blink is
// always software-emulated now (see emulateBlinkingLEDs). Do not reuse.
#define UNIT_FLAG_METERS_ON_DISPLAY 4
#define UNIT_FLAG_SWITCH_ROWS 8

struct UnitConfig {
  int midiInDev;
  int midiOutDev;
  bool isMain;      // main => devId 0x14 + transport; extender => 0x15
  DeviceModel model; // isProX() == (model == QConProX)
  int unitFlags;     // UNIT_FLAG_* bits
};

// Input-side listener (CSurf_MCU implements). HardwareUnit emits GLOBAL
// channel indices (1..N*8) by adding its own m_unitIndex*8 offset.
class HardwareEventListener {
public:
  virtual ~HardwareEventListener() {}
  // per-strip, GLOBAL channel (1..N*8)
  virtual void stripFaderMoved(int globalChannel, int value) = 0;
  // per-unit master slot; surface treats master as logical channel 0
  virtual void masterFaderMoved(int unitIndex, int value) = 0;
  virtual void vpotMoved(int globalChannel, int delta) = 0;
  virtual void vpotPressed(int globalChannel, bool pressed) = 0;
  virtual void stripButton(int button, int globalChannel, bool pressed,
                           bool doubleClick, bool longPress) = 0;
  // global (transport/F-keys/modifiers/...) forwarded as raw MIDI
  virtual void globalMidiEvent(MIDI_event_t *evt) = 0;
};

class HardwareUnit {
public:
  // pMCU is the owning logical surface for callbacks and compatibility paths.
  // Constructed: midiOut/midiIn + usleep (ctor body start) →
  //   DisplayHandler (ctor body end). See HardwareUnit.cpp.
  HardwareUnit(int unitIndex, const UnitConfig &cfg, CSurf_MCU *pMCU,
               int *errStats);
  ~HardwareUnit();

  bool isMain() const { return m_cfg.isMain; }
  bool isProX() const { return m_cfg.model == QConProX; }
  int unitIndex() const { return m_unitIndex; }
  int stripBase() const { return m_unitIndex * 8; } // first global channel
  unsigned char deviceId() const { return m_deviceId; }
  const UnitConfig &cfg() const { return m_cfg; }

  // --- per-unit options (were global CONFIG_FLAG_* bits) ---
  bool unitFlagSet(int flag) const { return (m_cfg.unitFlags & flag) != 0; }
  bool metersOnDisplay() const { return unitFlagSet(UNIT_FLAG_METERS_ON_DISPLAY); }
  bool switchRows() const { return unitFlagSet(UNIT_FLAG_SWITCH_ROWS); }

#ifdef KLINKE
  // Hand-off gate: while this unit is disabled (i.e. the iCON controllers
  // belong to Schaltmix), ALL outgoing MIDI is suppressed — faders, LEDs,
  // LCD and sysex. Unit 1 is never disabled.
  void setUnitEnabled(bool enabled) { m_unitEnabled = enabled; }
  bool unitEnabled() const { return m_unitEnabled; }
#endif
  // LED blink is always software-emulated: the per-unit
  // UNIT_FLAG_EMULATE_BLINKING opt-in was removed (see HardwareUnit.h).

  midi_Output *midiOutput() { return m_midiout; }
  midi_Input *midiInput() { return m_midiin; }
  DisplayHandler *displayHandler() { return m_display; }
  void startInput(); // begin reading m_midiin (called after the splash/reset)

  void setListener(HardwareEventListener *l) { m_pListener = l; }
  CSurf_MCU *surface() const { return m_pMCU; } // for DisplayHandler::getUnit()->surface()

  // --- outgoing, LOCAL indices (CSurf_MCU translates global->local first) ---
  void sendStripFader(int local, int value); // 0xE0 + local  (local 0..7)
  void setMasterFader(int value);            // 0xE8
  int getFaderPos(int local) const {
    return local >= 0 && local < 9 ? m_faderPos[local] : 0;
  }
  void setFaderPos(int local, int value) {
    if (local >= 0 && local < 9)
      m_faderPos[local] = value;
  }
  // raw send (used by everything that today called m_midiout->Send / SendMsg)
  void sendMidi(unsigned char status, unsigned char d1, unsigned char d2,
                int frame_offset);
  void sendMsg(MIDI_event_t *msg, int frame_offset);
  // per-unit LED dedup + ProX quirk + blink emulation
  void setLED(int button_nr, int led_state);
  void emulateBlinkingLEDs(DWORD now);
  void forceAllLEDsOff(); // send LED_OFF to all 128 notes, reset cache
  void invalidateFaderCache();   // set m_faderPos[0..8] to -1 (sentinel)
  void invalidateLEDCache();     // set m_led_state[128] to LED_UNKNOWN

  // reset (F0 00 00 66 <devId> 08 00 F7) + host-query handshake, per unit
  void reset();
  bool onMCUReset(MIDI_event_t *evt); // host handshake (per devId)

private:
  int m_unitIndex;
  UnitConfig m_cfg;
  unsigned char m_deviceId; // isMain ? 0x14 : 0x15

#ifdef KLINKE
  bool m_unitEnabled = true; // set false by CSurf_MCU::setSurfaceEnabled for units 2+
#endif

  midi_Output *m_midiout;
  midi_Input *m_midiin;
  DisplayHandler *m_display; // THIS unit's center LCD(s)

  int m_led_state[128]; // per-unit LED dedup
  int m_faderPos[9];    // local 0..7 strips + [8]=master (per-unit cache)

  short m_lastNowMod2;
  short m_blinkSometimes;

  CSurf_MCU *m_pMCU; // owning logical surface
  HardwareEventListener *m_pListener;
};

#endif
