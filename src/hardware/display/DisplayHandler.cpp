/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "csurf_mcu.h"
#include "DisplayHandler.h"

#include "McuDebugLog.h"
#include "HardwareUnit.h"
#include "McuAssert.h"
#include "Display.h"
#include "MidiEventBuffer.h"

class MIDI_Message : public MidiEventBuffer<512> {};

DisplayHandler::DisplayHandler(HardwareUnit *pUnit, EnumMCUType mcuType,
                               bool isProX) {
  m_pUnit = pUnit;
  m_pActualDisplay = NULL;
  m_mcuType = mcuType;
  m_isProX = isProX;
  m_wait = false;
  // widened from fixed [9] to dynamic vector (9 = 8 strips + master per unit)
  m_metersEnabled.assign(9, false);

  m_pHardwareState = new Display(this, 4);
  char pInvalidText[56];
  memset(pInvalidText, 0x1, 56);
  m_pHardwareState->changeText(0, 1, pInvalidText, 55);
  m_pHardwareState->changeText(1, 1, pInvalidText, 55);
  m_pHardwareState->changeText(2, 1, pInvalidText, 56);
  m_pHardwareState->changeText(3, 1, pInvalidText, 56);
}

DisplayHandler::~DisplayHandler() { safe_delete(m_pHardwareState); }

void DisplayHandler::sendDifferences(Display *pDisplay, int row,
                                     const char *text) {
  if (!pDisplay || !text || pDisplay != m_pActualDisplay || row < 0 ||
      row >= 4)
    return;

  // m_pHardwareState mirrors what the physical LCD currently shows, but it
  // is indexed by the HARDWARE row: sendToHardware() applies switchRows()
  // before writing it, so logical row 0's last-sent content lives at index 1
  // when the unit swaps rows. Mirror that mapping here so the diff compares
  // the caller's logical-row text against the matching physical-row state.
  int hardwareRow = row;
  if (m_pUnit->switchRows() && hardwareRow < 2)
    hardwareRow = 1 - hardwareRow;

  const char *hwState = m_pHardwareState->getText()[hardwareRow];
  int rowLen = pDisplay->getRowLength(row);

  // Diff: find each maximal run of changed characters and send only those.
  // Each run is still split into CHUNK-byte SysEx messages so no single
  // message exceeds the JACK/MIDI SysEx fragmentation threshold (~4 chars
  // on the Linux JACK path, observed 2026-06-23 with iCON QConPro X; the
  // earlier pure diff was garbled exactly because it sent a whole run as one
  // long SysEx). A one-field value change now costs one short run, not a
  // full 55-char row resend. sendToHardware() updates m_pHardwareState for
  // every byte it emits, so the next diff starts from an accurate picture.
  const int CHUNK = 4;
  int runStart = -1;
  for (int i = 0; i <= rowLen; i++) {
    bool changed = (i < rowLen) && (hwState[i] != text[i]);
    if (changed && runStart == -1) {
      runStart = i;
    } else if (!changed && runStart != -1) {
      for (int pos = runStart; pos < i; pos += CHUNK) {
        int len = std::min(CHUNK, i - pos);
        sendToHardware(row, pos, text + pos, len);
      }
      runStart = -1;
    }
  }
}

void DisplayHandler::sendToHardware(int row, int pos, char const *text,
                                    int len) {
  if (!text || row < 0 || row >= 4 || pos < 0 || len <= 0)
    return;
  // Displays keep their logical row order. Apply the per-unit preference only
  // while addressing the physical LCD, so it affects every display user
  // (modes, selectors, splash screen, and ProX panels) uniformly.
  int hardwareRow = row;
  if (m_pUnit->switchRows() && hardwareRow < 2)
    hardwareRow = 1 - hardwareRow;

  if (row == 0 || row == 1) {
    char tmp[56] = {};
    for (int k = 0; k < len && k < 55; k++)
      tmp[k] = (text[k] >= 0x20 && text[k] < 0x7f) ? text[k] : '.';
    MCU_LOG("ROW%d snd pos=%d len=%d [%s]", row, pos, len, tmp);
  }

  m_pHardwareState->changeText(hardwareRow, pos, text, len);

  if (!m_pActualDisplay)
    return;

  if (hardwareRow > 1 && !m_isProX)
    return;

  pos += m_pActualDisplay->getRowLength(hardwareRow) * (hardwareRow % 2) +
    (hardwareRow == 1); // + row because there is one unused byte at the end of each row

  MIDI_Message mm;
  addHeader(&mm, hardwareRow);
  //  F0 00 00 66 14 12 xx <data> F7   : update LCD. xx=offset (0-112), string.
  //  display is 55 chars wide, second line begins at 56, though.

  //  mm.evt.frame_offset=0;
  //  mm.evt.size=0;


  mm.append((hardwareRow > 1) ? 0x13 : 0x12);
  mm.append(static_cast<unsigned char>(pos));

  int cnt = 0;
  while (cnt < len) {
    mm.append(static_cast<unsigned char>(*text++));
    cnt++;
  }
  mm.append(0xF7);
  m_pUnit->sendMsg(mm.event(), -1);
}

void DisplayHandler::switchTo(Display *pDisplay) {
  if (!pDisplay || m_pActualDisplay == pDisplay)
    return;

  m_pActualDisplay = pDisplay;
  pDisplay->activate();

  memset(m_pHardwareState->getText()[1], 1, pDisplay->getRowLength(0));
}

void DisplayHandler::enableMCUMeter(int channel, bool enable) // channel is 1 based
{
  // widened from fixed <=9 to vector bounds
  if (channel <= 0 || channel >= (int)m_metersEnabled.size())
    return;

  // This is Mackie's LCD-meter SysEx mode. Controllers without that feature
  // must not receive it; their software meter is rendered separately.
  if (enable && !m_pUnit->metersOnDisplay())
    enable = false;

  if (enable == m_metersEnabled[channel])
    return;
  m_metersEnabled[channel] = enable;
  //  F0 00 00 66 14 20 0x 03 F7       : put track in VU meter mode, x=track
  MIDI_Message mm;

  addHeader(&mm, 0);

  mm.append(0x20);
  mm.append(static_cast<unsigned char>(channel - 1));
  //  mm.evt.midi_message[mm.evt.size++] = enable ? 0x07 : 0x01;
  mm.append(enable ? 0x03 : 0x01);
  mm.append(0xF7);
  MCU_LOG("METER ch=%d enable=%d -> 0x20 sent", channel, (int)enable);
  m_pUnit->sendMsg(mm.event(), -1);

  //  F0 00 00 66 14 21 01 F7       : Vertical Line Meter
  MIDI_Message mm2;

  addHeader(&mm2, 0);

  mm2.append(0x21);
  mm2.append(0x01);
  mm2.append(0xF7);
  m_pUnit->sendMsg(mm2.event(), -1);

}

void DisplayHandler::enableMCUMeter(bool enable) {
  // widened from fixed <9 to vector bounds (8 strips per unit)
  for (int i = 1; i < (int)m_metersEnabled.size(); i++) {
    enableMCUMeter(i, enable);
  }
  safe_call(m_pActualDisplay, resendRow(1));
}

void DisplayHandler::addHeader(MIDI_Message *pmm, int row) {
  //  F0 00 00 66 xx
  if (!pmm)
    return;
  pmm->append(0xF0);
  pmm->append(0x00);
  pmm->append(0x00);
  switch (m_mcuType) {
  case MCU_EX:
    pmm->append(0x66);
    pmm->append(0x15);
    break;
  case MCU:
    if (row <= 1) {
      pmm->append(0x66);
      pmm->append(0x14);
    } else {
      pmm->append(0x67);
      pmm->append(0x15);
    }
    break;
  case PROX:
    break;
  }
}

void DisplayHandler::waitForMoreChanges(bool block) {
  if (m_wait == block)
    return;

  if (block) {
    m_wait = true;
  } else {
    m_wait = false;
    safe_call(m_pActualDisplay, resendAllRows());
  }
}
