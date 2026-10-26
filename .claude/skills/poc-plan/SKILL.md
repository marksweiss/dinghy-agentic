---
name: poc-plan
description: The template and rules for plans/<poc>.md - an FM-1 POC's spec as small ordered steps, each with the exact change, the proving command and the done condition, marked for the implementer or an expert. Use when writing or checking a plan.
---
# A POC plan (plans/<poc>.md)

```markdown
# <Title>: plan            (spec: specs/<poc>.md)

## Start
tools/new_app.sh <poc> "<Title>"  (from kitchensink). Keep: ... Cut: ... Add: ...

## Parameters
| id | name | range | default | unit | page / knob | saved |

## Files
apps/<poc>/<poc>.c (sections: params, voices, audio, storage, screen, main loop), app.json, tests/*.txt, DEVICE-TEST-PLAN.md;
firmware/lib changes (only if a contract can stay the same; with a test).

## Steps
### 1. [implementer] Scaffold builds
Change: ... Command: `APP=<poc> ./build.sh` and `APP=<poc> sh tests/run_host.sh apps/<poc>/tests/smoke.txt`.
Done: package line printed, smoke screens exist.
### 2. [synth-dsp] The voice
Change: <struct, functions with signatures, constants, the per-sample and per-control-step work>. Command: ... Done: <WAV
measurement: peak, pitch of A4 = 440 Hz, no clipping>.
### ...

## Acceptance criteria -> steps -> tests
| criterion | step | host check | device row |
```

Rules: every step builds and runs; one concern a step; exact names and numbers (an implementer on Haiku must not have to
decide); steps that need judgment go to an expert; the last steps are tests (test-writer), review (rules-reviewer) and the
device test plan section. Check every command against the repository before writing it.
