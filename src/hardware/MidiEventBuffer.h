/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#pragma once

#include "reaper_plugin.h"
#include <cstddef>
#include <cstring>
#include <new>

// REAPER's MIDI_event_t declares four message bytes but explicitly permits
// size to describe a longer message. Keep the MIDI_event_t object at the
// start of a suitably aligned raw allocation and access the extended payload
// through the allocation's byte representation, never beyond midi_message[4].
template <size_t Capacity> class MidiEventBuffer {
public:
  MidiEventBuffer() {
    static_assert(Capacity >= sizeof(MIDI_event_t::midi_message),
                  "MIDI event capacity must include the inline payload");
    std::memset(m_storage, 0, sizeof(m_storage));
    m_event = new (m_storage) MIDI_event_t;
    m_event->frame_offset = 0;
    m_event->size = 0;
  }

  MidiEventBuffer(const MidiEventBuffer &) = delete;
  MidiEventBuffer &operator=(const MidiEventBuffer &) = delete;

  MIDI_event_t *event() { return m_event; }
  const MIDI_event_t *event() const { return m_event; }

  bool append(unsigned char byte) {
    if (m_event->size < 0 || static_cast<size_t>(m_event->size) >= Capacity)
      return false;
    payload()[m_event->size++] = byte;
    return true;
  }

private:
  unsigned char *payload() {
    return m_storage + offsetof(MIDI_event_t, midi_message);
  }

  alignas(MIDI_event_t)
      unsigned char m_storage[offsetof(MIDI_event_t, midi_message) + Capacity];
  MIDI_event_t *m_event;
};
