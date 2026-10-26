---
name: fm1-screen-ui
description: The FM-1 screen and panel conventions with firmware/lib/uikit.h - layout, redraw-only-changes, canvas limits, fonts, help pages, gestures, LEDs - from what TastyWaves users found intuitive. Use for any screen or control design or code.
---
# Screen and panel

Layout (uikit.h): knob row y 0..35 (ui_knob x4) | tabs y 38 (ui_tabs, "SET k/n" tag) | picture y 66..199 (ui_pic, strips) |
status y 202 (ui_status: page, V n/N, CPU) | patch name y 218 (ui_title: SAVED flash, edited dot).

- **Draw only what changed:** keep what each piece last showed (a value, a hash of a picture); a full screen is ~40 ms of SPI.
  Rate-limit the status line (4 Hz). On a page change clear once and redraw all.
- **Canvas:** at most 240 x 64 (CV_MAX); `ui_pic(y, h, fn, ctx)` calls fn once per strip with cv_oy set (fn draws the whole
  picture in its own coordinates; cheap pictures only).
- **Fonts:** FONT_S 16 px (ASCII 32..126), FONT_L 32 px capitals only, FONT_M 24 px with FONTS_SPLEEN. Mixed case large:
  FONT_S at 2x (splash.h's mask). Text helpers: ui_cat / ui_num / ui_fmt_time (no printf).
- **Gestures:** press = page (again: next tab), hold = switch for on / off states (ui_gesture, 500 ms; HOME 700 ms = help);
  a hint on the page: ui_hold_hint("HOLD ARP: ", "ON", UI_ACC). PLAY = transport when a sequencer is on, else LATCH.
- **Help:** hold HOME: a global page ("BUTTON  WHAT IT DOES"), then the page's own; 14 lines; labels in the accent colour.
- **LEDs:** a button lit = its switch is on; keys lit = what sounds (or the step playing). `fm1_led_key_glow` for landmarks.
- **Colours:** UI_ACC lilac (touched / on / page), UI_WARN amber (recording, bypass), UI_GRAY labels, UI_DIM off.
Check: a host script through every page and gesture, a contact sheet, looked at.
