---
name: synth-modules
description: How to use each firmware/lib module in an FM-1 app - dsp.h, fx.h, arpseq.h, patchlog.h, link.h, uikit.h, splash.h - the include order, the hooks to define, who calls what from the ISR or the main loop. Use when writing or planning any app code that uses them.
---
# The modules (firmware/lib)

Include order in an app (apps/kitchensink/kitchensink.c is the working reference):
```c
#define FONTS_SPLEEN                     /* uikit.h's FONT_M (before platform.c) */
static void link_service(void);
#define APP_SERVICE link_service         /* the SysEx link beside the updater */
static void my_param(int id, int32_t v);
#define HOST_PARAM(id, v) my_param(id, v) /* host scripts: "T param ID V" */
#include "platform.c"
#include "dsp.h"                         /* then fx.h */
/* note_on / note_off, then: */ #define ARP_NOTE_ON(n, v) note_on(n, v)
                                #define ARP_NOTE_OFF(n) note_off(n)
#include "arpseq.h"
#include "patchlog.h"                    /* PL_SLOT_INDEX: which store slot (default the last) */
#define LINK_PATCHES 1                   /* + LINK_ID1/2, LINK_QUIET(), LINK_APP(...) */
#include "link.h"
#include "uikit.h"
#include "splash.h"
```

| Module | ISR side | Main side | Test |
| --- | --- | --- | --- |
| dsp.h | dsp_* per sample / per DSP_CR frames; `dsp_ring_get` | `dsp_ring_put` (events), volatile params | tests/lib_dsp_check.c |
| fx.h | `fx_process(bus, n)` (bus Q15 x 32) | `fx_param[i]`, `fx_sync()`, `fx_bypass` | tests/lib_fx_check.c |
| arpseq.h | `arp_run(n)` after the drain; `arp_in_on/off`, `seq_in_on/off` | `arp_ext.ticks/start` (MIDI clock), patterns (len = 0 first), `sq_run` | tests/lib_arpseq_check.c |
| patchlog.h | never | `pl_init`, `pl_get/put/del` when quiet | tests/lib_patchlog_check.c |
| link.h | never | runs from platform_poll (APP_SERVICE) | tests/lib_link_check.c, web_link_check.mjs |
| uikit.h | never | drawing, `ui_gesture`, `ui_msg` | apps/kitchensink screens |
| splash.h | never | `splash_play(name, tagline)` at boot | look at it (DINGHY_SPLASH=1) |

Budgets seen: fx.h ~20 KB pool + 96 KB delay + 24 KB reverb; patchlog one store slot (16 KB); a slot of fx costs about one
TastyWaves voice of CPU. Extending a module: keep its contract, add to its test, run `sh tests/run_lib.sh`.
