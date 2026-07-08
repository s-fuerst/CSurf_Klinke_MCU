/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * WP-A Step 2: HardwareUnit owns MIDI I/O + reset SysEx + raw send.
 * CSurf_MCU holds one unit (N=1) and forwards via shims.
 */
#include "HardwareUnit.h"
#include "McuAssert.h"

#ifndef _WIN32
#include <unistd.h>
#endif

HardwareUnit::HardwareUnit(int unitIndex, const UnitConfig &cfg,
                           CSurf_MCU *pMCU, int *errStats)
    : m_unitIndex(unitIndex), m_cfg(cfg), m_deviceId(cfg.isMain ? 0x14 : 0x15),
      m_midiout(NULL), m_midiin(NULL), m_display(NULL), m_pMCU(pMCU),
      m_pListener(NULL) {
  for (int i = 0; i < 128; i++)
    m_led_state[i] = LED_OFF;
  for (int i = 0; i < 9; i++)
    m_faderPos[i] = 0;

  // create midi hardware access (moved verbatim from CSurf_MCU ctor)
  m_midiin = cfg.midiInDev >= 0 ? CreateMIDIInput(cfg.midiInDev) : NULL;
  m_midiout = cfg.midiOutDev >= 0
                  ? CreateThreadedMIDIOutput(
                        CreateMIDIOutput(cfg.midiOutDev, false, NULL))
                  : NULL;

  if (errStats) {
    if (cfg.midiInDev >= 0 && !m_midiin)
      *errStats |= 1;
    if (cfg.midiOutDev >= 0 && !m_midiout)
      *errStats |= 2;
  }

  // WORKAROUND: PipeWire JACK may crash (SIGSEGV in process_empty) when
  // MIDI is sent immediately after opening a JACK MIDI output port.
  // The data-loop thread accesses buffers that are not yet allocated.
  // (MEMD 2026-06-27; N>1 variant unverified — WP-A is N=1.)
  if (m_midiout) {
#ifdef _WIN32
    //    Sleep(500);
#else
    usleep(200000);
#endif
  }
}

HardwareUnit::~HardwareUnit() {
  // delete the raw MIDI ports exactly as CSurf_MCU used to
  DELETE_ASYNC(m_midiout);
  DELETE_ASYNC(m_midiin);
}

void HardwareUnit::startInput() {
  if (m_midiin)
    m_midiin->start();
}

void HardwareUnit::sendStripFader(int local, int value) {
  if (m_midiout)
    m_midiout->Send(0xe0 + local, value & 0x7f, (value >> 7) & 0x7f, -1);
}

void HardwareUnit::setMasterFader(int value) {
  if (m_midiout)
    m_midiout->Send(0xe8, value & 0x7f, (value >> 7) & 0x7f, -1);
}

void HardwareUnit::sendMidi(unsigned char status, unsigned char d1,
                            unsigned char d2, int frame_offset) {
  if (m_midiout)
    m_midiout->Send(status, d1, d2, frame_offset);
}

void HardwareUnit::sendMsg(MIDI_event_t *msg, int frame_offset) {
  if (m_midiout)
    m_midiout->SendMsg(msg, frame_offset);
}

void HardwareUnit::setLED(int button_nr, int led_state) {
  // filled in Step 4 (per-unit LED dedup + ProX quirk)
  sendMidi(0x90, button_nr, led_state, -1);
}

void HardwareUnit::emulateBlinkingLEDs(DWORD now) {
  // filled in Step 4 (per-unit blink emulation)
  (void)now;
}

void HardwareUnit::reset() {
  // F0 00 00 66 <devId> 08 00 F7  : reset this MCU unit (per-device-id)
  if (!m_midiout)
    return;
  struct {
    MIDI_event_t evt;
    char data[5];
  } poo;
  poo.evt.frame_offset = 0;
  poo.evt.size = 8;
  poo.evt.midi_message[0] = 0xF0;
  poo.evt.midi_message[1] = 0x00;
  poo.evt.midi_message[2] = 0x00;
  poo.evt.midi_message[3] = 0x66;
  poo.evt.midi_message[4] = m_deviceId;
  poo.evt.midi_message[5] = 0x08;
  poo.evt.midi_message[6] = 0x00;
  poo.evt.midi_message[7] = 0xF7;
  m_midiout->SendMsg(&poo.evt, -1);
}

bool HardwareUnit::onMCUReset(MIDI_event_t *evt) {
  // handshake matcher (moved here in Step 5); for Step 2 still unused.
  (void)evt;
  return false;
}
