# PipeWire-JACK Crash at Startup

## Symptome
- Reaper stürzt beim Start ab (SIGSEGV), nur manchmal
- Systematisches Muster: 1. Start = Crash, 2. Start = Recovery-Mode, 3. Start = OK, 4. Start = Crash (zyklisch)
- Tritt NICHT auf, wenn a2jmidid läuft (auch ungenutzte a2j-Ports reichen)

## Backtrace
```
Thread 15 "data-loop.0" received signal SIGSEGV, Segmentation fault.
0x00007ffff77b242f in __memmove_avx512_unaligned_erms () from /usr/lib/libc.so.6

process_empty () at ../pipewire/pipewire-jack/src/pipewire-jack.c:1783
prepare_output () at ../pipewire/pipewire-jack/src/pipewire-jack.c:1799
complete_process () at ../pipewire/pipewire-jack/src/pipewire-jack.c:1823
signal_sync () at ../pipewire/pipewire-jack/src/pipewire-jack.c:2145
cycle_signal () at ../pipewire/pipewire-jack/src/pipewire-jack.c:2181
```

## Umgebung
- PipeWire: 1.6.7-1.1
- PipeWire-JACK: 1.6.7-1.1
- Reaper: läuft unter GDK_BACKEND=x11
- Audio: PipeWire JACK Emulation → Saffire Pro 40
- MIDI: JACK MIDI Bridge, NICHT a2j-Ports

## Analyse-Hypothese
- Crash ist in process_empty() — memcpy auf (vermutlich) NULL/invalid buffer
- Thread "data-loop.0" = PipeWires Audio-Processing-Thread, nicht unser Code
- a2jmidid verhindert Crash → zusätzliche JACK-MIDI-Ports beeinflussen PipeWires Buffer-Allokation
- Unsere Extension erstellt beim Init MIDI-Ports (via Reaper API) — möglicher Trigger, aber nicht direkte Ursache
- TODO: pipewire-jack.c um line 1783 analysieren, verstehen was process_empty() tut

## Referenz-Dateien
- Crash-Log: ~/reaper_crash.log
- debug_reaper.sh: im Repo (startet Reaper unter gdb, speichert backtrace)
