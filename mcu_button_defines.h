/*
  MCU documentation:

  MCU=>PC:
  The MCU seems to send, when it boots (or is reset) F0 00 00 66 14 01 58 59 5A 57 18 61 05 57 18 61 05 F7

  Ex vv vv    :   volume fader move, x=0..7, 8=master, vv vv is int14
  B0 1x vv    :   pan fader move, x=0..7, vv has 40 set if negative, low bits 0-31 are move amount
  B0 3C vv    :   jog wheel move, 01 or 41

  to the extent the buttons below have LEDs, you can set them by sending these messages, with 7f for on, 1 for blink, 0 for off.
  90 0x vv    :   rec arm push x=0..7 (vv:..)
  90 0x vv    :   solo push x=8..F (vv:..)
  90 1x vv    :   mute push x=0..7 (vv:..)
  90 1x vv    :   selected push x=8..F (vv:..)
  90 2x vv    :   pan knob push, x=0..7 (vv:..)
  90 28 vv    :   assignment track
  90 29 vv    :   assignment send
  90 2A vv    :   assignment pan/surround
  90 2B vv    :   assignment plug-in
  90 2C vv    :   assignment EQ
  90 2D vv    :   assignment instrument
  90 2E vv    :   bank down button (vv: 00=release, 7f=push)
  90 2F vv    :   bank up button (vv:..)
  90 30 vv    :   channel down button (vv: ..)
  90 31 vv    :   channel up button (vv:..)
  90 32 vv    :   flip button
  90 33 vv    :   global view button
  90 34 vv    :   name/value display button
  90 35 vv    :   smpte/beats mode switch (vv:..)
  90 36 vv    :   F1
  90 37 vv    :   F2
  90 38 vv    :   F3
  90 39 vv    :   F4
  90 3A vv    :   F5
  90 3B vv    :   F6
  90 3C vv    :   F7
  90 3D vv    :   F8
  90 3E vv    :   Global View : midi tracks
  90 3F vv    :   Global View : inputs
  90 40 vv    :   Global View : audio tracks
  90 41 vv    :   Global View : audio instrument
  90 42 vv    :   Global View : aux
  90 43 vv    :   Global View : busses
  90 44 vv    :   Global View : outputs
  90 45 vv    :   Global View : user
  90 46 vv    :   shift modifier (vv:..)
  90 47 vv    :   option modifier
  90 48 vv    :   control modifier
  90 49 vv    :   alt modifier
  90 4A vv    :   automation read/off
  90 4B vv    :   automation write
  90 4C vv    :   automation trim
  90 4D vv    :   automation touch
  90 4E vv    :   automation latch
  90 4F vv    :   automation group
  90 50 vv    :   utilities save
  90 51 vv    :   utilities undo
  90 52 vv    :   utilities cancel
  90 53 vv    :   utilities enter
  90 54 vv    :   marker
  90 55 vv    :   nudge
  90 56 vv    :   cycle
  90 57 vv    :   drop
  90 58 vv    :   replace
  90 59 vv    :   click
  90 5a vv    :   solo
  90 5b vv    :   transport rewind (vv:..)
  90 5c vv    :   transport ffwd (vv:..)
  90 5d vv    :   transport pause (vv:..)
  90 5e vv    :   transport play (vv:..)
  90 5f vv    :   transport record (vv:..)
  90 60 vv    :   up arrow button  (vv:..)
  90 61 vv    :   down arrow button 1 (vv:..)
  90 62 vv    :   left arrow button 1 (vv:..)
  90 63 vv    :   right arrow button 1 (vv:..)
  90 64 vv    :   zoom button (vv:..)
  90 65 vv    :   scrub button (vv:..)

  90 6x vv    :   fader touch x=8..f
  90 70 vv    :   master fader touch

  PC=>MCU:

  F0 00 00 66 14 12 xx <data> F7   : update LCD. xx=offset (0-112), string. display is 55 chars wide, second line begins at 56, though.
  F0 00 00 66 14 08 00 F7          : reset MCU
  F0 00 00 66 14 20 0x 03 F7       : put track in VU meter mode, x=track  

  90 73 vv : rude solo light (vv: 7f=on, 00=off, 01=blink)

  B0 3x vv : pan display, x=0..7, vv=1..17 (hex) or so
  B0 4x vv : right to left of LEDs. if 0x40 set in vv, dot below char is set (x=0..11)

  D0 yx    : update VU meter, y=track, x=0..d=volume, e=clip on, f=clip off
  Ex vv vv : set volume fader, x=track index, 8=master


*/
#define B_VPOT_TRACK                            0x28 
#define B_VPOT_SEND                                     0x29 
#define B_VPOT_PAN                                      0x2A 
#define B_VPOT_PLUG                                     0x2B 
#define B_VPOT_EQ                                               0x2C 
#define B_VPOT_INSTRUMENT               0x2D 
#define B_BANK_DOWN                                     0x2E 
#define B_BANK_UP                                               0x2F 
#define B_CHANNEL_DOWN                  0x30 
#define B_CHANNEL_UP                            0x31 
#define B_FLIP                                                  0x32 
#define B_GLOBAL_VIEW                           0x33 
#define B_NAME_VALUE                            0x34 
#define B_SMPTE_BEATS                           0x35 
#define B_F1                                                            0x36 
#define B_F2                                                            0x37 
#define B_F3                                                            0x38 
#define B_F4                                                            0x39 
#define B_F5                                                            0x3A 
#define B_F6                                                            0x3B 
#define B_F7                                                            0x3C 
#define B_F8                                                            0x3D 
#define B_GVIEW_FIRST                           0x3E
#define B_GVIEW_LAST                            0x45
#define B_SHIFT                                                 0x46 
#define B_OPTION                                                0x47 
#define B_CONTROL                                               0x48 
#define B_ALT                                                           0x49 
#define B_AUTO_READ                                     0x4A 
#define B_AUTO_WRITE                            0x4B 
#define B_AUTO_TRIM                                     0x4C 
#define B_AUTO_TOUCH                            0x4D 
#define B_AUTO_LATCH                            0x4E 
#define B_AUTO_GROUP                            0x4F 
#define B_SAVE                                                  0x50 
#define B_UNDO                                                  0x51 
#define B_CANCEL                                                0x52 
#define B_ENTER                                                 0x53 
#define B_MARKER                                                0x54 
#define B_NUDGE                                                 0x55 
#define B_CYCLE                                                 0x56 
#define B_DROP                                                  0x57 
#define B_REPLACE                                               0x58 
#define B_CLICK                                                 0x59 
#define B_SOLO                                                  0x5a 
#define B_REWIND                                                0x5b 
#define B_FFWD                                                  0x5c 
#define B_STOP                                                  0x5d 
#define B_PLAY                                                  0x5e 
#define B_RECORD                                                0x5f 
#define B_UP                                                            0x60 
#define B_DOWN                                                  0x61 
#define B_LEFT                                                  0x62 
#define B_RIGHT                                                 0x63 
#define B_ZOOM                                                  0x64 
#define B_SCRUB                                                 0x65 

#define L_RUDESOLO                                              0x73


#define LED_ON                                                  0x7f
#define LED_OFF                                                 0x00
#define LED_BLINK                                               0x01
#define LED_BLINK_LESS                                          0x02
#define LED_UNKNOWN                              -0x01
