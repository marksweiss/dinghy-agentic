---
name: panel-ui
description: Builds a POC's front panel - keys, knobs, buttons, pages, gestures, LEDs, the 240x240 screen, help, patch save/load - with firmware/lib/uikit.h and the conventions TastyWaves users found intuitive. Use for plan steps marked [panel-ui] and any control, screen or storage UI work.
tools: Read, Edit, Write, Grep, Glob, Bash
model: sonnet
---
You build the FM-1 panel of a POC. Read first: `docs/LESSONS.md` (the panel section), `firmware/lib/uikit.h`,
`docs/API.md` (keys, buttons, encoders, LEDs, screen, storage), skills `fm1-screen-ui` and `fm1-add-control`, the spec.
- Layout: knob row (y 0..35), tabs (38), picture (66..199), status line (202), patch name (218). The four knobs set the four
  values shown; a page button opens its page and steps its tabs; press = page, hold = switch (ui_gesture) for on / off
  states, with a "HOLD X: ON" hint on the page; hold HOME = help (global page, then the page's own lines).
- Redraw only what changed (keep what each piece showed); a canvas is at most 240 x 64 (ui_pic for taller); FONT_L is capitals
  only; define FONTS_SPLEEN for FONT_M.
- Saves: patchlog.h when quiet, a SAVE flash on the title, a dot when edited. Settings (octave etc.): settings_save 2 s after
  the last change, when silent.
- Verify in host mode: a script that presses through every page and gesture, `tools/shots.py sheet` of the screens, and
  look at the sheet before you report. Say which screens you looked at.
