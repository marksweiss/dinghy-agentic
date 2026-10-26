---
name: fm1-add-control
description: Add a synth parameter end to end in an FM-1 app - the parameter table, the ISR read, a knob on a page, the screen value, saving in the patch, a host check, a device test row. Use whenever a new knob, page value or patch value is introduced.
---
# Add a parameter

1. **Table:** a new id in the app's parameter enum and its row {name, lo, hi, default} (kitchensink's `PD[]`). Append at the
   end: patch records store values by position, so old patches load with the default for the new one.
2. **ISR:** read `par[id]` once per block (or per control step) into a local; derive coefficients at control rate.
3. **Knob:** on a page's set of four (`PG_PAR[page] + k`); `fm1_enc_take(ENC_KNOB1 + k)` -> clamp (`ks_param`) -> touched.
4. **Screen:** the knob row redraws when the value or touched changes; text via ui_num / ui_fmt_time / a name table.
5. **Patch:** included automatically if saving copies the table; a stored value is clamped on load (never trusted).
6. **Host:** a script line `param <id> <v>` (or `enc`), a check on the WAV or a screen; `APP=<poc> sh tests/run_host.sh`.
7. **Device:** a test plan row: "turn KNOB n on PAGE -> you hear ..."
