/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Meter bridge for Channel Strip Mode. Identical to MultiTrackMeterBridge
 * (hardware LED meters, master LEDs) EXCEPT that it never draws the software
 * meter bars on the LCD: row 1 of the display carries the strip-name
 * selection list or the parameter name/value text, which the meter bars
 * would otherwise overwrite (see MeterBridge::updateMeter -> showMeterOnDisplay).
 */
#ifndef MCU_CHANNELSTRIPMETERBRIDGE
#define MCU_CHANNELSTRIPMETERBRIDGE

#include "MultiTrackMeterBridge.h"

class ChannelStripMeterBridge : public MultiTrackMeterBridge {
public:
  ChannelStripMeterBridge() : MultiTrackMeterBridge() {}
  bool alsoOnDisplay() override { return false; }
};

#endif
