# Licensing

dinghy-agentic is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). It is a fork of [Dinghy](https://github.com/zvenson/dinghy) (zvenson), which carries Felucca's platform as SLOOP
does; everything below Dinghy's line in this table is its own. You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit) |
| The TRS MIDI IN `firmware/src/midi_uart.c` | Felucca's, as sloopDX carries it, with the ring read by content from Felucca [Salt] 0.9-salt14 by ChanceTheMaker (found by keremimo) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (from OberVave, the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| From OberVave: the storage (blobs, slots), `LCD_IDLE`, the SMK-37 wiring of `platform.c`, `store.h`, the pad event queue in `smk_input.h`, `tests/smk_flash_test.c`, `tests/run_tests.sh`, the CI workflow | Copyright (C) 2026 zvenson |
| The app `firmware/src/dinghy.c`, the wiring `platform.c`, the host mode `tests/`, the docs | zvenson |
| dinghy-agentic: the modules `firmware/lib/` (from TastyWaves' engine, fx, arp, patch log, link and screens, made generic), `apps/kitchensink`, the generic browser tool `tools/web/` (but `fm1ota.js`, `fm1pkg.js`), `tools/new_app.sh`, `shots.py`, `gen_dsp_tables.py`, `web_release.sh`, `web_tool.sh`, `web_serve.py`, `install_fw.sh`, `install.sh`, `send_clock.py`, `targets/fm1open/` (the fetch script and guide, not the kit), `.claude/` (agents, skills), `CLAUDE.md`, `docs/WORKFLOW.md`, `LESSONS.md`, `MODULES.md`, `tools/README.md`, the multi-app build, the host additions (raw flash, `param`, named shots), the platform hooks (`APP_SERVICE`, the MASTER read outside LCD DMA), the `tests/lib_*` and `web_link_check.mjs` tests | Copyright (C) 2026 Mark Weiss (@marksweiss), written with Claude (Anthropic); the modules from [TastyWaves](https://github.com/marksweiss/tastywaves), same author and licence |
| The browser installer `tools/web/fm1ota.js`, `tools/web/fm1pkg.js` (Felucca's `web/`, commit 1c0798e, unchanged) | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| Spleen fonts 8x16, 12x24, 16x32 (Frederic Cambus), rendered into `firmware/gen/spleen_font.h` (the FONTS_SPLEEN option) | BSD 2-Clause | `assets/fonts/Spleen-LICENSE.txt` |
| M-VAVE's FM1 Open kit: fetched by `targets/fm1open/fetch.sh` into `targets/fm1open/kit/`, not in this tree. Its APP is GPL-3.0 with a linking exception, its engine a binary licensed for FM-1 devices only, its boot files Apache-2.0: see the kit's LICENSE.md and THIRD_PARTY.md. Never link Dinghy or dinghy-agentic code with its binary engine (they carry no such exception) | M-VAVE's terms | <https://github.com/m-vave/fm1_open> |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The logo

Dinghy's logo (the designburg signet) is not under the GPL and marks Dinghy itself; it is not part of this fork.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37" and "MKE-P37"
belong to their owners. Dinghy and dinghy-agentic are not made or endorsed by them.
