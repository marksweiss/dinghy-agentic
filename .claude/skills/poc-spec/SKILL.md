---
name: poc-spec
description: The template and rules for specs/<poc>.md, the spec of an FM-1 synth proof of concept - target, sound, every control and screen, storage, MIDI, budgets, acceptance criteria, open questions. Use when writing or reviewing a spec.
---
# A POC spec (specs/<poc>.md)

Write it for two readers: the user (is this the synth I meant?) and the plan writer (can I plan this without guessing?).

```markdown
# <Title>
<Two sentences: what it is and what makes it worth trying.>

## Target
Dinghy | FM1 Open, because <reason> (targets/fm1open/README.md: engines, samples, Bluetooth -> FM1 Open; else Dinghy).

## Sound
Voices (count, mono/poly/legato), sources (oscillators, wavetables, noise, FM ...), filter, envelopes, modulation
(LFOs, routing), effects (fx.h slots), the mix (headroom, soft clip). What firmware/lib/dsp.h and fx.h already give.

## Playing it
| Control | Page | Does |      <- every key, knob (on every page), button (press and hold), encoder
Pages and tabs (the knob sets of each). Gestures (press = page, hold = switch). PLAY / REC / SAVE / PRESETS.

## The screen
Per page: the knob row values, the picture (what it shows, why it helps), the status line, help lines.

## Saved
What a patch holds (patchlog.h: <= 80 int16 values + 33 words a record), settings (settings_save), what is not saved.

## MIDI
In (notes, CCs, clock), out.

## Budgets
CPU: <voices x per-voice cost> against the 5.8 ms ISR (reference: TastyWaves 12 x 2 wavetable osc + SVF ~50 %).
Flash: <tables, assets> of 581,564 B. RAM / pool: <buffers>. What to cut first if it does not fit.

## Out of scope

## Acceptance criteria (the device test plan)
1. <What the user does on the FM-1 -> what they hear / see.>

## Open questions (each with a recommendation)
```

Rules: reuse the conventions in docs/LESSONS.md; prefer firmware/lib over new code; a criterion is something a person
can check by ear and eye; keep it short enough to read in three minutes.
