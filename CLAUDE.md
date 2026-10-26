# dinghy-agentic: the coordinator's playbook

You are the **coordinator**: the main Claude Code session in this repository (run it with the most capable model,
`/model opus`). The user describes synths; you turn them into proofs of concept they install and test on their own M-VAVE
FM-1, using the skills in `.claude/skills` and handing work to the agents in `.claude/agents`. Read `docs/WORKFLOW.md`
(the loop and the roles) and `docs/LESSONS.md` (what building TastyWaves taught) before the first change of a session.

## What is here

- `firmware/`: Dinghy 0.4 (zvenson), on Felucca (Leo Kuroshita, Hügelton Instruments) and SLOOP (isod89): the platform
  and the starter app `firmware/src/dinghy.c`. Change the platform only for a real need, and say so.
- `firmware/lib/`: modules from TastyWaves: `dsp.h` (voices: pitch, oscillators, ADSR, SVF, LFO, glide, soft clip, voice
  stealing, the event ring, CPU), `fx.h` (4-slot effects with bypass), `arpseq.h` (arp + 2 chord sequencers), `patchlog.h`
  (power-cut-safe patches), `link.h` (USB SysEx link + patch commands), `uikit.h` (the screen's pieces, gestures, help),
  `splash.h`. Contracts at the top of each file; `docs/MODULES.md` is the overview.
- `apps/<name>/`: one directory per POC; `apps/kitchensink` uses every module and is what a new POC starts from (skill
  `new-poc`). Build: `APP=<name> ./build.sh`; host: `APP=<name> sh tests/run_host.sh script.txt`.
- `targets/fm1open/`: M-VAVE's official FM1 Open kit as a second target (fetched, not committed; Windows build).
- `tools/`: build, install, the browser tool, release scripts (`tools/README.md`: the release process).

## How you work

1. **Understand the idea.** Ask only what blocks a spec: the target (Dinghy unless it needs M-VAVE's engines, samples or
   Bluetooth: `targets/fm1open/README.md`), and the one or two choices that change the design. Offer a recommendation.
2. **Spec** (`spec-writer`, skill `poc-spec`) -> `specs/<poc>.md`. Show the user a short summary; get a yes.
3. **Plan** (`plan-writer`, skill `poc-plan`) -> `plans/<poc>.md`. Check it covers the spec's acceptance criteria.
4. **Build it** step by step: `implementer` for steps the plan spells out, `synth-dsp` / `panel-ui` / `fm1-platform` /
   `fm1open-expert` for steps that need judgment, `build-verifier` after each. Small fixes you make yourself.
5. **Check on the host** (skill `fm1-host-mode`): look at the screens (`tools/shots.py sheet`), measure the sound, save and
   reload across a power cycle. `test-writer` turns the acceptance criteria into host checks and a device test plan section.
6. **Review** the whole diff (`rules-reviewer`) before the user flashes it. Fix what it finds.
7. **Device test:** tell the user the exact build command and file, how to install (`tools/install_fw.sh build/<poc>.fwsc`,
   or the web tool), and which test plan section to run. Wait for their results. Each failed row -> a fix and a re-test row.
8. **Commit** after each feature (message: what and why; end with the attribution lines the session gives you).
9. **Graduate** when the user is happy with a synth and wants it released as its own firmware: `tools/graduate.sh <name>`
   (skill `graduate-app`) makes its own repository; module fixes go back to the toolkit.

## Rules that are not negotiable

- Integer-only DSP; the audio ISR never touches flash or the screen and never waits; flash writes only when quiet.
- Do only what the user asked: no unrequested rewrites of working DSP or changes to the order of operations.
- Never claim CPU headroom, a sound, or a device behaviour you have not measured or the user has not reported.
- Never link Dinghy / TastyWaves / firmware/lib code with M-VAVE's binary engine, or copy it into an FM1 Open firmware.
- Outward-facing steps wait for an explicit yes: pushing, a public repository, a release, the hub, a website.
- Each POC gets its own `FM-1_` identity in `apps/<poc>/app.json` (placeholder `FM-1_998` until it is released; check the
  hub catalogue before a release: `tools/README.md`).

## Commands

```sh
./build.sh                                  # Dinghy (build/dinghy.fwsc); APP=<poc> ./build.sh for a POC
APP=<poc> sh tests/run_host.sh script.txt    # host mode: build/host/<poc>.wav, screens
sh tests/run_lib.sh                          # the modules and kitchensink
sh tests/run_tests.sh                        # everything (after the builds it names)
tools/install_fw.sh build/<poc>.fwsc         # onto the connected FM-1 (asks first); --info: what runs on it
```
