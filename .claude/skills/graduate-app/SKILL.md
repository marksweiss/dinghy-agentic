---
name: graduate-app
description: Graduate a finished synth from apps/<name> in dinghy-agentic into its own repository - when to, tools/graduate.sh, what it keeps and changes, the checks after, the GitHub repo, and moving fixes between the app and the toolkit. Use when the user is happy with a POC and wants to release or share it as its own firmware.
---
# Graduate an app

**When:** the user wants the synth to be its own thing: a name, releases, a website, a Firmware Hub listing. Until then it
stays in `apps/<name>` (cheap: shared platform and modules, many POCs side by side). A quick share can be a release from
the toolkit instead; a listed firmware should graduate (the hub follows one repository's releases).

1. Everything for the app committed in the toolkit; its device test plan passed.
2. `tools/graduate.sh <name> [dest]` (default `../<name>`): a clone of the toolkit with its full history (the Dinghy / Felucca
   lineage stays visible), then one commit that keeps `apps/<name>`, the firmware/lib modules its source #includes and their
   tests, its specs / plans; removes the other apps, the FM1 Open target and the toolkit's plan; makes `<name>` the default
   build (`./build.sh`, `tests/run_host.sh`, `tools/install_fw.sh`); writes a README (the app's README + build and credits;
   the toolkit's README moves to `docs/DINGHY-AGENTIC.md`), a LICENSING row to fill in, a note at the top of CLAUDE.md; adds
   the remote `toolkit`. Nothing is pushed.
3. In the new folder: `tools/install.sh` (its own .venv), `./build.sh`, `sh tests/run_lib.sh`, the app's host scripts; read
   the README and LICENSING row and fill in the author and description.
4. A real identity in `apps/<name>/app.json` (unique in https://thefirmwarehub.com/catalog.json), then the release checklist
   in tools/README.md (release-engineer). Creating the GitHub repo, pushing, Pages, the hub: each with the user's yes.

**Fixes both ways:** a module or platform fix made in the graduated repo goes back to the toolkit (a commit there with the
same change, with its test; or `git format-patch` / `git am`). Toolkit fixes come in with `git fetch toolkit` and a merge or
cherry-pick of the commits that touch the modules the app uses. Keep the module contracts so both sides stay compatible.
