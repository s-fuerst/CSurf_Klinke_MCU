# Project Memory

## Decisions

- 2026-07-26: Config flags "Fake fader touch" and "Emulate blinking LEDs" moved from global CONFIG_FLAG_* bits to per-unit UNIT_FLAG_* bits in UnitConfig.unitFlags. Global defines CONFIG_FLAG_FADER_TOUCH_FAKE (1) and CONFIG_FLAG_EMULATING_BLINKING (4) removed; bit values reserved as legacy (never reused). KLINKE2 config format extended to a 4th comma field "<in>,<out>,<type>,<unitFlags>"; old 3-field strings still parse (backward compatible). Legacy parser migrates old global bits onto unit 0; KLINKE2 parser migrates them onto all units. Added new per-unit option "Show level meters on the display" (UNIT_FLAG_METERS_ON_DISPLAY) — persisted only, not yet wired to runtime behavior (to be implemented later). Dialog reorganized into two GROUPBOXes: "Settings for selected unit" (type, MIDI in/out, the 3 per-unit checkboxes) and "Global settings (all units)" (keyboard modifier, swap zoom arrows).

## Active Context


## Bugs & Fixes

- Plug Mode / extenders showed bogus LCD level meters and lost row-1 text. Root cause: MeterBridge::showMeterOnDisplay() drew the '|' software-meter bars into display row 1 for EVERY unit, ignoring the per-unit UNIT_FLAG_METERS_ON_DISPLAY option — so Unit 1 (option off) still showed meters, and the bars overwrote the FADER name/value text on MCU 2-row units (extenders appeared to have no text). Fix 1: guard showMeterOnDisplay() with unit->metersOnDisplay(). Fix 2: gate the showMeterOnDisplay() call in updateMeter() with alsoOnDisplay(), and make PlugModeMeterBridge override alsoOnDisplay() to false so Plug Mode NEVER draws software-meter bars over its parameter text (other modes keep returning true). Fix 3: PlugMode::activate() calls enableMCUMeters(false, /*excludeProX=*/true) — a new CSurf_MCU overload that turns off Mackie's LCD-meter SysEx mode on non-ProX units while leaving ProX hardware meters untouched. Verified via Windows MSVC build.

## Changelog

- Plug Mode LCD meters fix: MeterBridge::showMeterOnDisplay() now respects per-unit metersOnDisplay() (was drawing bars unconditionally, clobbering row-1 text). Added CSurf_MCU::enableMCUMeters(bool, bool excludeProX) overload; PlugMode::activate() calls enableMCUMeters(false, true) so Mackie units get LCD-meter SysEx off but ProX keeps its hardware meters. Fixed meter bars still appearing in Plug Mode: now the showMeterOnDisplay() call in updateMeter() is gated by alsoOnDisplay(), and PlugModeMeterBridge overrides alsoOnDisplay() to false so Plug Mode never draws software-meter bars over its parameter text. Fixes: Unit 1 showing meters with option off; extenders losing row-1 text in FX mode.

## Patterns

