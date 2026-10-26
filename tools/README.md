# tools/

Scripts to set up, build, test, install and release firmwares built here. Run them from anywhere: each one changes to
the repository first. The Python ones use `.venv/` (made by `install.sh`).

## Set up, build, install

| Tool | What it does |
| --- | --- |
| `install.sh` | Once per checkout: a uv venv (Pillow, mido, python-rtmidi, numpy), JieLi's toolchain, three files of the JieLi AC79 SDK; then a build and the host tests |
| `get_toolchain.sh` | JieLi's clang for the chip, to `~/.jieli/toolchain` (`install.sh` calls it) |
| `build.py` | The build (`../build.sh` runs it, in a `linux/amd64` Docker container on macOS): app, update loader, `.fwsc`, and the checks. `APP=name` / `--app name` builds `apps/name/`; `--release X.Y.Z` a release; `--board smk37` the SMK-37 |
| `fm1pkg_make.py`, `lz4blk.py` | Package the app and the loader into a `.fwsc` (Felucca's format); used by `build.py` |
| `install_fw.sh` | Installs a `.fwsc` on the connected FM-1 (default `build/$APP.fwsc`); `--info` shows what runs on it |
| `fm1_install.py` | The installer behind it (Felucca's USB update protocol); exit codes in its header |

## Host mode, pictures, clock

| Tool | What it does |
| --- | --- |
| `shots.py` | PPM screens to PNG, a contact sheet of several screens, an animated GIF of a host-mode film |
| `send_clock.py` | MIDI clock to the FM-1, to try sync (arp, sequencers) without a DAW |
| `gen_dsp_tables.py` | `firmware/lib/dsp_tables.h`: the curves dsp.h and fx.h need (exp, tan, tanh), computed here so the firmware stays integer only |
| `gen_font.py` | Screen fonts from BDF / TTF (`firmware/gen/felucca_font.h`, `spleen_font.h`) |

## The browser tool

| Tool | What it does |
| --- | --- |
| `web/` | Install the bundled release from the browser (Felucca's installer, `fm1ota.js` / `fm1pkg.js`, unchanged), see the FM-1's identity, back up / restore / rename / delete patches over `firmware/lib/link.h`. Everything app-specific comes from `web/firmware/release.json` |
| `web_release.sh APP X.Y.Z` | Puts `build/APP-X.Y.Z.fwsc` and its `release.json` (from `apps/APP/app.json`) into `web/firmware/` |
| `web_tool.sh` | Serves `web/` at http://localhost:8000 and opens it (Chrome or Edge) |
| `web_serve.py` | The server behind it (caching off, so a reload never mixes a new page with old scripts) |

`web/firmware/` is not committed here (it is one app's release). In your app's own repository, commit it and publish
`tools/web` with GitHub Pages: the page then installs your firmware for anyone with Chrome (TastyWaves does this at
https://tastysynth.com/tool/).

## Making a release

### The identity

Every package carries an FM-1 identity (`FM-1_` and digits). The FM-1 reports it over USB, installers check it after
writing, and the [FM-1 Firmware Hub](https://thefirmwarehub.com/) lists a firmware under it. Rules TastyWaves learned:

- **One number, your own, for every release.** Put it in `apps/<name>/app.json` (`"identity"`). Do not encode the version
  in it unless you tell the hub (a changed identity is a new listing).
- **Unique.** Check the live catalogue first: `curl -s https://thefirmwarehub.com/catalog.json` lists every firmware's
  `product`. TastyWaves first shipped `FM-1_950`, which three other firmwares already used, and had to move to `FM-1_937`.
- **Never** `FM-1_015` / `FM-1_016` (the official firmware). Dinghy itself is `FM-1_99N`.
- Recovery does not depend on it: the update loader always reports `ota-FM-1_900`, so an interrupted install resumes
  whatever the number.

### Checklist

1. Everything committed; `docs/DEVICE-TEST-PLAN.md` of the app has a section for what changed.
2. The tests: `sh tests/run_tests.sh` (after the builds it names), or at least `sh tests/run_lib.sh` and the app's own scripts.
3. `APP=name ./build.sh --release X.Y.Z` -> `build/name-X.Y.Z.fwsc`. Note the free flash it prints.
4. **On a real FM-1:** `tools/install_fw.sh build/name-X.Y.Z.fwsc`; the screen shows the version; run the new test plan
   section; saved patches survive; switch off and on once. Untested builds are not released.
5. `tools/web_release.sh name X.Y.Z`, commit, push (Pages publishes the page).
6. `git tag -a vX.Y.Z -m "..." && git push origin vX.Y.Z`, then
   `gh release create vX.Y.Z build/name-X.Y.Z.fwsc --title "..." --notes-file NOTES.md`.
7. The README's version and download link (`releases/download/vX.Y.Z/name-X.Y.Z.fwsc`).
8. Check that the build, the release download and the page's file are byte-identical (`shasum -a 256` on each).

### The FM-1 Firmware Hub

Listing is by email (https://thefirmwarehub.com/submit/): name, your name as shown, repository, identity, licence, two or
three sentences, a picture link (a 240 x 240 screen capture), optional browser / video links, and "Tested on a real FM-1,
leaving it again works: yes". That last line needs two tests first: install from the official firmware (V15 / V16), and
install another firmware from yours (it must start the next update). Note in your README that switching firmware erases
your patches, and that yours does not read other firmwares' projects or samples. Once listed, the hub picks up each new
GitHub release every few hours. Optional: let GitHub build and sign releases (the hub's shared workflow
`zvenson/fm1-build/.github/workflows/build.yml@v1`; Dinghy 0.4's `.github/workflows/release.yml` shows it).
