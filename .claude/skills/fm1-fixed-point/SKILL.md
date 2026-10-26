---
name: fm1-fixed-point
description: Integer-only DSP idioms for the FM-1 chip (no FPU, no 64-bit divide) - Q formats, phase increments, tables, filters, envelopes, feedback loops, headroom. Use when writing or converting any audio or control math.
---
# Fixed point on the FM-1

- **Formats:** audio Q15 (+-32767); coefficients Q30 / Q28 in int32; products in int64 (`(int64_t)a * b >> 28`). The fx bus is
  Q15 x 32. Clamp BEFORE a left shift. Never create the constant `-32767 << 7` (it looks like a ROM address: the build fails).
- **Division:** 32-bit only (`/`, `%` on int64 needs a missing libgcc routine: link error), divisor checked non-zero (zero
  does not trap: garbage). Divide by constants freely; prefer shifts and reciprocal tables in loops.
- **Phase:** `uint32_t ph += inc`, `inc = f * 2^32 / 44100` -> `dsp_note_inc`, `dsp_pitch_inc` (1/256 semitone).
- **Curves:** exp / tan / tanh are tables computed offline (`tools/gen_dsp_tables.py`), `const` in flash, interpolated.
- **Rates:** per sample only what must be; envelopes, filter coefficients, LFOs once per 32 frames (DSP_CR).
- **Rounding:** `x >> n` rounds toward -infinity. In a feedback loop that leaves a -1 forever (a limit cycle, a DC hum): round
  toward zero (`fx.h` fx_tz8 / fx_mz). Test that a loop falls silent after the input stops.
- **Headroom:** sum voices into int32, scale for the voice count, then `dsp_softclip` (transparent to 0.5 FS).
- **Host:** the same C runs on the computer (tests compile with cc and float available): grep `float|double|\d\.\d` before a
  device build; the device build is the real check.
