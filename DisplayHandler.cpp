/**
 * Copyright (C) 2009-2012 Steffen Fuerst 
 * Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
 */

#include "DisplayHandler.h"

#include "McuDebugLog.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "Display.h"

class MIDI_Message {
public:
  MIDI_Message() {
    evt.frame_offset = 0;
    evt.size = 0;
    memset(data, 0, 512);
  }
  MIDI_event_t evt;
  char data[512];
};

DisplayHandler::DisplayHandler(CSurf_MCU *pMCU, EnumMCUType mcuType) {
  m_pMCU = pMCU;
  m_pActualDisplay = NULL;
  m_mcuType = mcuType;
  m_wait = false;
  for (int i = 0; i < 9; i++) {
    m_metersEnabled[i] = false;
  }

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
  if (pDisplay != m_pActualDisplay)
    return;

  int diffStart = -1;
  char *cpos = m_pHardwareState->getText()[row];
  const char *org = text;

  for (int i = 0; i < pDisplay->getRowLength(row); i++) {
    if (*cpos != *text && diffStart == -1) {
      diffStart = i;
    }
    if (*cpos == *text && diffStart != -1) {
      sendToHardware(row, diffStart, org + diffStart, i - diffStart);
      diffStart = -1;
    }
    *cpos++;
    *text++;
  }
  if (diffStart != -1)
    sendToHardware(row, diffStart, org + diffStart,
                   pDisplay->getRowLength(row) - diffStart);
}

void DisplayHandler::sendToHardware(int row, int pos, char const *text,
                                    int len) {
  if (row == 0 || row == 1) {
    char tmp[56] = {};
    for (int k = 0; k < len && k < 55; k++)
      tmp[k] = (text[k] >= 0x20 && text[k] < 0x7f) ? text[k] : '.';
    MCU_LOG("ROW%d snd pos=%d len=%d [%s]", row, pos, len, tmp);
  }

  m_pHardwareState->changeText(row, pos, text, len);

  if (!m_pActualDisplay)
    return;

	if (row > 1 && !m_pMCU->IsFlagSet(CONFIG_FLAG_PROX))
		return;

	pos += m_pActualDisplay->getRowLength(row) * (row % 2) +
           (row == 1); // + row because there is one unused byte at the end of each row

  MIDI_Message mm;
  addHeader(&mm, row);
  //  F0 00 00 66 14 12 xx <data> F7   : update LCD. xx=offset (0-112), string.
  //  display is 55 chars wide, second line begins at 56, though.

  //  mm.evt.frame_offset=0;
  //  mm.evt.size=0;


	mm.evt.midi_message[mm.evt.size++] = (row > 1) ? 0x13 : 0x12; // 0x12
  mm.evt.midi_message[mm.evt.size++] = pos;

  int cnt = 0;
  while (cnt < len) {
    mm.evt.midi_message[mm.evt.size++] = *text++;
    cnt++;
  }
  mm.evt.midi_message[mm.evt.size++] = 0xF7;
  m_pMCU->SendMsg(&mm.evt, -1);
}

void DisplayHandler::invalidateHardwareState() {
  for (int row = 0; row < 4; row++)
    memset(m_pHardwareState->getText()[row], 1, m_pHardwareState->getRowLength(row));
}

void DisplayHandler::switchTo(Display *pDisplay) {
  if (m_pActualDisplay == pDisplay)
    return;

  // Disable VU meters on every display change; PanMode::updateDisplay re-enables them.
  enableMCUMeter(false);

  m_pActualDisplay = pDisplay;
  pDisplay->activate();

  // Invalidate row 0 separator positions so boot animation underscores are cleared.
  // Row 1 separators are owned by VU meter hardware (mode 0x03) — don't touch them.
  char *row0 = m_pHardwareState->getText()[0];
  for (int sep = 6; sep < 55; sep += 7)
    row0[sep] = 1;
}

void DisplayHandler::enableMCUMeter(int channel, bool enable) // channel is 1 based
{
  ASSERT(channel > 0 && channel <= 9);

  // if (! m_pMCU->IsFlagSet(CONFIG_FLAG_MACKIE_LEVEL_METER))
  //   enable = false;

  if (enable == m_metersEnabled[channel])
    return;
  m_metersEnabled[channel] = enable;
  //  F0 00 00 66 14 20 0x 03 F7       : put track in VU meter mode, x=track
  MIDI_Message mm;

  addHeader(&mm, 0);

  mm.evt.midi_message[mm.evt.size++] = 0x20;
  mm.evt.midi_message[mm.evt.size++] = 0x00 + channel - 1;
	//  mm.evt.midi_message[mm.evt.size++] = enable ? 0x07 : 0x01;
  mm.evt.midi_message[mm.evt.size++] = enable ? 0x03 : 0x00;
  mm.evt.midi_message[mm.evt.size++] = 0xF7;
  MCU_LOG("METER ch=%d enable=%d -> 0x20 sent", channel, (int)enable);
  m_pMCU->SendMsg(&mm.evt, -1);

  // 0x21 (Vertical Line Meter global mode) is handled once in the bool wrapper.
}

void DisplayHandler::enableMCUMeter(bool enable) {
  bool anyChanged = false;
  for (int i = 1; i < 9; i++) {
    if (m_metersEnabled[i] != enable) {
      anyChanged = true;
      enableMCUMeter(i, enable);
    }
  }

  // Only send 0x21 0x01 when at least one channel actually transitioned to
  // enabled.  Sending it on every PanMode activation (even with no state
  // change) disrupts the MCU display each time the Pan button is pressed.
  // Do NOT send 0x21 0x00 on disable — that command breaks the MCU display.
  if (anyChanged && enable) {
    MIDI_Message mm;
    addHeader(&mm, 0);
    mm.evt.midi_message[mm.evt.size++] = 0x21;
    mm.evt.midi_message[mm.evt.size++] = 0x01;
    mm.evt.midi_message[mm.evt.size++] = 0xF7;
    MCU_LOG("METER all 0x21 01 sent");
    m_pMCU->SendMsg(&mm.evt, -1);
    safe_call(m_pActualDisplay, resendRow(1));
  }
}

void DisplayHandler::addHeader(MIDI_Message *pmm, int row) {
  //  F0 00 00 66 xx
  pmm->evt.midi_message[pmm->evt.size++] = 0xF0;
  pmm->evt.midi_message[pmm->evt.size++] = 0x00;
  pmm->evt.midi_message[pmm->evt.size++] = 0x00;
  switch (m_mcuType) {
  case MCU_EX:
		pmm->evt.midi_message[pmm->evt.size++] = 0x66; //0x66
    pmm->evt.midi_message[pmm->evt.size++] = 0x15;
    break;
	case MCU:
		if (row <= 1) {
			pmm->evt.midi_message[pmm->evt.size++] = 0x66; //0x66
			pmm->evt.midi_message[pmm->evt.size++] = 0x14; // 0x14
		} else {
			pmm->evt.midi_message[pmm->evt.size++] = 0x67; //0x66
			pmm->evt.midi_message[pmm->evt.size++] = 0x15; // 0x14
		}
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
