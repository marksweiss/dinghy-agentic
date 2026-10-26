# dinghy-agentic: the build plan

Goal: a fork of [Dinghy](https://github.com/zvenson/dinghy) 0.4 that is a toolkit for prompt-driven development of
proof-of-concept synth firmwares for the M-VAVE FM-1: a coordinator (the main Claude Code session) drawing on skills
and handing work to agents, the user testing each POC on their own FM-1. Built from what TastyWaves
(github.com/marksweiss/tastywaves) taught.

Decisions (user, 2026-10-25): base = upstream Dinghy 0.4; two targets (Dinghy, fully open; M-VAVE's FM1 Open, the
official kit); rename marksweiss/dinghy to marksweiss/dinghy-agentic (keeps "forked from zvenson/dinghy"), old
VAVEWave branch archived; reusable modules from TastyWaves.

## Layout

```
firmware/            Dinghy 0.4: the platform (hal/, loader/, src/) and the starter app (src/dinghy.c), plus small hooks
firmware/lib/        reusable modules for the Dinghy target, each a header with its contract at the top
apps/<name>/         one directory per synth POC: <name>.c, app.json (display name, identity, version), README.md
apps/kitchensink/    the demo that uses every module (the modules' integration test)
targets/fm1open/     the second target: README, a pinned fetch script, notes for writing an FM1 Open instrument
tools/               0.4's tools + install_fw.sh, the browser installer (web/), web_release.sh, shots.py, send_clock.py
specs/, plans/       where the spec writer and the plan writer put their documents (one per POC / feature)
.claude/agents/      the roles; .claude/skills/: the recipes; CLAUDE.md: the coordinator's playbook
docs/                0.4's docs + WORKFLOW.md, LESSONS.md, MODULES.md, TARGETS.md
```

## Licensing rule (shapes the two targets)

Dinghy, Felucca, SLOOP and TastyWaves are GPL-3.0-only without a linking exception. FM1 Open's app is GPL-3.0 with an
exception for M-VAVE's proprietary binary engine (libfm1_engine.a). So: a Dinghy-target firmware never links the FM1
engine; Dinghy / TastyWaves / firmware/lib code is never copied into an FM1 Open firmware (only the user's own new code,
which they may license as they like). The FM1 Open kit is fetched by script, not vendored.

## Phases

1. [x] Repo: main fast-forwarded to upstream 0.4; branch `agentic`.
2. [x] Platform hooks on 0.4: APP_SERVICE (a USB SysEx link beside the updater), ADC read not during LCD DMA,
       host raw-flash emulation (st_read / fl_erase4k / fl_write on a RAM image, DINGHY_FLASH=file persists it),
       host `param ID V` command (HOST_PARAM hook), cv_px on host.
3. [x] Multi-app build: APP=name / --app for build.py, run_host.sh, run_tests.sh; app.json; identity per app.
4. [x] Modules (firmware/lib): dsp.h, fx.h, arpseq.h, patchlog.h, link.h (+ patch commands), uikit.h, splash.h;
       a host test each; apps/kitchensink uses them all (host + device build).
5. [x] Tools: install_fw.sh, install.sh, web installer page (tools/web), web_release.sh, shots.py, send_clock.py,
       tools/README.md (release process, identities, hub submission).
6. [x] FM1 Open target: targets/fm1open (fetch script pinned to a release + SHA-256, build notes: Windows / VM,
       app structure, API, licence, flashing and coexistence with Dinghy firmwares).
7. [x] .claude: CLAUDE.md (coordinator), agents, skills (from TastyWaves' lessons).
8. [x] Docs: README (what it is, the workflow, quick start), LICENSING (credits: Felucca, SLOOP, Dinghy, TastyWaves,
       M-VAVE for the fetched kit), WORKFLOW, LESSONS, MODULES, TARGETS.
9. [x] Publish (2026-10-26): archive/vavewave pushed, marksweiss/dinghy renamed to marksweiss/dinghy-agentic (still forked from zvenson/dinghy), main = this work.
