# dinghy-agentic

**Describe a synth. Play it on your FM-1.**

dinghy-agentic is a toolkit for building synthesizer firmwares for the M-VAVE FM-1 with Claude Code. You describe the
instrument you want in a few sentences; a coordinator agent turns it into a spec, a plan and a working proof of concept,
handing the work to specialist agents (spec, plan, implementation, DSP, panel, platform, tests, review, release). You
install the result on your own FM-1, play it, and tell it what to change.

It is a fork of [Dinghy](https://github.com/zvenson/dinghy) (the smallest FM-1 firmware to build on, by zvenson, on
[Felucca](https://github.com/hugelton/Felucca) and [SLOOP](https://github.com/isod89/sloop-fm1)), with everything learned
building [TastyWaves](https://github.com/marksweiss/tastywaves) (a released 12-voice wavetable synth) turned into reusable
modules, tools, agents and skills, and M-VAVE's official [FM1 Open](https://github.com/m-vave/fm1_open) kit as a second
target.

## What you get

| | |
| --- | --- |
| **A coordinator and 12 agents** | `CLAUDE.md` makes the main Claude Code session the coordinator (use Opus); `.claude/agents` has the spec writer (Opus), plan writer (Sonnet), implementer (Haiku), the experts for DSP, panel, platform and FM1 Open, a test writer, a build verifier, a rules reviewer, a researcher and a release engineer |
| **17 skills** | `.claude/skills`: spec and plan templates, starting a POC, the modules, fixed-point DSP, host mode, the screen and panel conventions, storage, device test plans, builds, crashes, installs and releases, the browser tool and GitHub Pages, the Firmware Hub, FM1 Open, graduating an app into its own repository |
| **Synth modules** | `firmware/lib`: voices (pitch, oscillators, ADSR, state-variable filter, LFO, glide, voice stealing), a 4-slot effects chain, an arpeggiator and two chord sequencers, a power-cut-safe patch store, a USB link to the browser, a UI kit, a splash animation. Integer-only, each with a host test ([docs/MODULES.md](docs/MODULES.md)) |
| **Many POCs, one repo** | `apps/<name>/`, built with `APP=<name> ./build.sh`; `tools/new_app.sh` starts one from `apps/kitchensink`, a small synth that uses every module; `tools/graduate.sh` turns a finished one into its own repository (as TastyWaves was) |
| **Host mode** | Every app runs on your computer: scripted keys and knobs, the screen as PNG, the sound as WAV, saves across simulated power cycles. Most bugs die before the FM-1 sees them |
| **Install from the browser** | `tools/web`: the Install button (Felucca's installer), patch backup and restore, ready for GitHub Pages |
| **Two targets** | Dinghy (fully open, your own engine, builds on Linux or macOS with Docker) and FM1 Open (M-VAVE's engines, samples, Bluetooth; Windows build) ([targets/fm1open](targets/fm1open/README.md)) |
| **The platform, intact** | Dinghy 0.4 underneath: updates that always work, the USB rescue, USB-MIDI and TRS MIDI in, USB audio, the SMK-37, the build's safety checks, the test suite |

## Quick start

You need an M-VAVE FM-1 and a USB cable; Linux on x86-64, or macOS / Windows with Docker; Python 3; [uv](https://docs.astral.sh/uv/);
[Claude Code](https://claude.com/claude-code).

```sh
git clone https://github.com/marksweiss/dinghy-agentic.git && cd dinghy-agentic
tools/install.sh            # once: venv, JieLi toolchain, the AC79 SDK files; then a build and the host tests
claude                      # in this directory; /model opus
```

Then tell it what you want, for example:

> A four-voice paraphonic drone synth: two oscillators per voice slowly detuning against each other, a resonant
> low-pass that the mod wheel sweeps, a long reverb. The keys latch notes on and off.

The coordinator asks what it needs to know, writes `specs/<name>.md` for you to approve, plans it, builds it, checks it
in host mode, has it reviewed, and gives you the file to install and a short test plan to try on the FM-1. Report back
by row number ("2.3: the reverb tail clicks") and it fixes and asks you to retest. See [docs/WORKFLOW.md](docs/WORKFLOW.md).

By hand, without the agents:

```sh
tools/new_app.sh drone "Drone"                    # apps/drone from kitchensink
APP=drone sh tests/run_host.sh apps/drone/tests/smoke.txt
APP=drone ./build.sh                              # build/drone.fwsc
tools/install_fw.sh build/drone.fwsc              # onto the FM-1 (asks first)
```

## Documentation

- [CLAUDE.md](CLAUDE.md): the coordinator's playbook. [docs/WORKFLOW.md](docs/WORKFLOW.md): the loop and the roles.
- [docs/LESSONS.md](docs/LESSONS.md): what building TastyWaves taught (the chip, the panel, testing, shipping).
- [docs/MODULES.md](docs/MODULES.md): firmware/lib. [targets/fm1open/README.md](targets/fm1open/README.md): the second target.
- [tools/README.md](tools/README.md): the tools, identities, the release checklist, the Firmware Hub.
- [docs/DINGHY.md](docs/DINGHY.md), [docs/API.md](docs/API.md), [docs/RULES.md](docs/RULES.md): the platform underneath.

## Credits and licence

GPL-3.0-only ([LICENSE](LICENSE), [LICENSING.md](LICENSING.md) for who made which part). Built on Dinghy by zvenson,
Felucca by Leo Kuroshita (Hügelton Instruments), SLOOP by isod89, and the people Dinghy credits; modules, tools, agents and
skills by Mark Weiss ([@marksweiss](https://github.com/marksweiss)) with Claude (Anthropic), from TastyWaves. M-VAVE's FM1 Open
kit is fetched separately under M-VAVE's own terms and is not part of this repository. "M-VAVE" and "FM-1" belong to their
owners; dinghy-agentic is not made or endorsed by them.
