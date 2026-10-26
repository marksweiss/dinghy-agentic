# From a prompt to a synth on the FM-1

The loop dinghy-agentic is built for: someone describes a synth in a few sentences; the coordinator (the main Claude Code
session) turns it into a tested proof of concept they install and play on their own FM-1, then iterates on what they hear.

```
 idea ──> spec ──> plan ──> build ──> host checks ──> review ──> device test ──> feedback ──┐
  (user)  spec-     plan-    implementer  test-writer   rules-     (user, by     (user)       │
          writer    writer   + experts    build-        reviewer    test plan                 │
                                          verifier                  rows)                     │
            ^────────────────────────────── coordinator (main session) ───────────────────────┘
```

## Roles (.claude/agents)

| Role | Model | Owns | Hands back |
| --- | --- | --- | --- |
| **coordinator** (the main session: CLAUDE.md) | Opus | the conversation, the decisions, the order of work, what the user sees | each step's result, in plain words |
| `spec-writer` | Opus | `specs/<poc>.md`: the synth as the user will play it | a spec with acceptance criteria |
| `plan-writer` | Sonnet | `plans/<poc>.md`: the spec as small, ordered, checkable steps | a plan an implementer can follow without guessing |
| `implementer` | Haiku | one plan step at a time, exactly as written | the diff, the build and host results, anything the plan did not cover |
| `synth-dsp` | Opus | sound: voices, oscillators, filters, envelopes, effects, CPU | DSP code with measurements |
| `panel-ui` | Sonnet | controls, pages, screens, LEDs, help, patches | UI code with screenshots looked at |
| `fm1-platform` | Sonnet | the Dinghy platform, hal, build, flash map, USB, the update path, crashes | answers and fixes below the app |
| `fm1open-expert` | Sonnet | the FM1 Open target (targets/fm1open) | FM1 Open code and build steps |
| `test-writer` | Sonnet | host scripts and checks, the device test plan section | tests that fail before and pass after |
| `build-verifier` | Haiku | running builds and test suites, reading their output | pass / fail with the exact failing lines |
| `rules-reviewer` | Opus | reviewing a diff against docs/RULES.md and docs/LESSONS.md | findings ranked by severity |
| `researcher` | Sonnet | facts from the web and other firmwares, cited | `docs/research/<topic>.md` |
| `release-engineer` | Sonnet | identity, release builds, GitHub releases, the web tool, the hub | a release, or the exact next step for the user |

The coordinator calls agents only for work that is big enough to hand off; a one-line fix it makes itself.

## The documents

**`specs/<poc>.md`** (spec-writer; skill `poc-spec`): what it is in two sentences; the target (Dinghy or FM1 Open) and why;
the sound (voices, sources, filter, modulation, effects); every control (knob, button, page, gesture) and every screen;
what is saved; MIDI; budgets (CPU, flash, RAM); what is out of scope; acceptance criteria the user can check on the
device; open questions for the user.

**`plans/<poc>.md`** (plan-writer; skill `poc-plan`): the files to create or change; the modules from firmware/lib it uses;
the parameter table; then numbered steps, each small (one function, one page, one test), each with the exact change, the
command that proves it (`APP=<poc> tests/run_host.sh tests/scripts/<poc>-N.txt`, a check), and what "done" looks like.
Steps that need judgment are marked for an expert (synth-dsp, panel-ui) instead of the implementer.

**`apps/<poc>/`**: the firmware (`<poc>.c`, `app.json`, `README.md`), its host scripts (`tests/`), and its
`DEVICE-TEST-PLAN.md`, one section per round of device testing.

## The loop, in practice

1. **Idea** -> the coordinator asks only what blocks a spec (target, the one or two choices that change the design).
2. **Spec** -> the user reads it (or its summary) and says yes or what to change.
3. **Plan** -> the coordinator checks it against the spec, then runs the steps: implementer for the mechanical ones, the
   experts for the rest, build-verifier after each.
4. **Host checks** -> screens looked at (tools/shots.py sheet), sound measured, saves survive a power cycle (DINGHY_FLASH).
5. **Review** -> rules-reviewer on the whole diff; fix what it finds.
6. **Device test** -> `APP=<poc> ./build.sh`, the user installs (`tools/install_fw.sh build/<poc>.fwsc` or the web tool) and
   works through the new test plan section.
7. **Feedback** -> each reported row becomes a fix and a re-test row. Commit per feature.
8. When the user wants to share it: `release-engineer` (tools/README.md: identity, release, web tool, the hub).
