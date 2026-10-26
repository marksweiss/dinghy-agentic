---
name: fm1-host-mode
description: Run an FM-1 app on the computer - scripted keys, buttons, knobs, MIDI and params; screens as PNG, sound as WAV, a film; a flash image across power cycles - and check the results. Use to test anything before asking for a device test.
---
# Host mode

`APP=<name> sh tests/run_host.sh script.txt` -> `build/host/<name>.wav`, `build/host/<name>-*.png`, stdout (MIDI out,
saves, the app's printf under `#ifdef DINGHY_HOST`). Without APP: Dinghy itself.

Script lines (`tests/host.c` has the full list; ms first; keep them in time order):
```
100 keys 0x91          # bit k = key k (0 = F3): F3 A3 C4 held; "keys 0" lets go
200 btn 2              # a tap (FX); 300 btndown 10 / 900 btnup 10: a hold of ARP (600 ms)
400 enc 2 -5           # encoder: 0 SELECT, 1 ALGORITHM, 2..5 KNOB1..4, 6 PRESETS
500 midi 0x90 60 100   # USB MIDI in ("trs ..." the jack); 600 clock 120 96: 96 clocks at 120 BPM
700 param 3 64         # the app's parameter 3 = 64 (HOST_PARAM): the engine without the menus
800 shot fx            # the screen -> build/host/<name>-fx.ppm (then .png)
2000 end
```
Buttons: 0 OCT- 1 OCT+ 2 FX 3 SEL 4 ENV 5 LFO 6 EDIT 7 GLO 8 HOME 9 SAVE 10 ARP 11 SEQ 12 PLAY 13 REC.

- **Look** at screens: `.venv/bin/python tools/shots.py sheet out.png build/host/<name>-*.png`, then read it.
- **Measure** sound in a scratchpad `python -I` script (wave module): peak, RMS, clipping, pitch by FFT.
- **Power cycles:** `DINGHY_FLASH=file` keeps the raw flash (patchlog) across runs: run 1 saves, run 2 loads.
- **Splash / films:** `DINGHY_SPLASH=1` shows the splash; a 4th argument FPS to the binary writes a frame every 1/FPS s
  (`build/host/<name> OUT script.txt 20`), `tools/shots.py gif` makes a GIF.
- Not on the host: CPU timing (fm1_ticks is coarse: the meter reads ~0), the ADC, LCD timing, real USB. Say so.
