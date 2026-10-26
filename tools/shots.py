# SPDX-License-Identifier: GPL-3.0-only
"""Host-mode screens for people (and agents) to look at: PPM frames -> PNG, a contact sheet, an animated GIF.

    .venv/bin/python tools/shots.py sheet OUT.png build/host/myapp-*.png [--cols 5]     a contact sheet (240 px each)
    .venv/bin/python tools/shots.py gif OUT.gif build/host/myapp-0*.ppm [--ms 50]       a film (tests/host.c ... FPS)
    .venv/bin/python tools/shots.py png build/host/myapp-*.ppm                          each PPM -> PNG beside it

The host writes the screen at 240 x 240 (tests/run_host.sh scales its PNGs to 480). Look at a sheet before claiming a
screen works: an agent reading one image of five screens catches layout bugs a test would not.
"""
import sys
from pathlib import Path
from PIL import Image


def load(p):
    im = Image.open(p).convert("RGB")
    return im.resize((240, 240), Image.NEAREST) if im.size != (240, 240) else im


def main(a):
    if len(a) < 2:
        print(__doc__); return 2
    cmd, out, files = a[0], a[1], [f for f in a[2:] if not f.startswith("--")]
    opt = {}
    for i, x in enumerate(a):
        if x in ("--cols", "--ms") and i + 1 < len(a):
            opt[x] = int(a[i + 1]); files = [f for f in files if f != a[i + 1]]
    if cmd == "png":
        for p in [out] + files:
            load(p).save(Path(p).with_suffix(".png"))
        return 0
    ims = [load(p) for p in files]
    if not ims:
        print("no images"); return 1
    if cmd == "sheet":
        cols = opt.get("--cols", 5); rows = (len(ims) + cols - 1) // cols
        sh = Image.new("RGB", (cols * 244, rows * 244), (90, 90, 90))
        for i, im in enumerate(ims):
            sh.paste(im, ((i % cols) * 244 + 2, (i // cols) * 244 + 2))
        sh.save(out)
    elif cmd == "gif":
        pal = [im.convert("P", palette=Image.ADAPTIVE, colors=64) for im in ims]
        pal[0].save(out, save_all=True, append_images=pal[1:] + [pal[-1]] * 20, duration=opt.get("--ms", 50), loop=0, optimize=True)
    else:
        print(__doc__); return 2
    print(out); return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
