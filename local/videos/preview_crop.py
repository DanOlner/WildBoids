#!/usr/bin/env python3
"""Preview center-crop levels on a single frame of a clip, to help pick a
'crop' value for reel.json -- without re-rendering the whole reel.

Grabs one frame, renders it at several crop levels in a labeled row, saves
preview_crop.png and opens it. Each tile mimics the reel (centre-crop -> fit to
a square -> pad), so what you see is what the reel will show.

Usage:
    python3 preview_crop.py CLIP [LEVELS...] [--at SECONDS] [--out PATH] [--no-open]

Examples:
    python3 preview_crop.py stage2_sensetest.mov
    python3 preview_crop.py stage2_sensetest.mov 95 90 85 80 --at 5
    python3 preview_crop.py first_predators.mov 0.9 0.8 0.7 --out previews/pred.png --no-open
"""

import os
import subprocess
import sys
import tempfile

from make_reel import HERE, die, ensure_pillow, normalize_crop

THUMB = 380          # tile size (square, matching the reel's square canvas)
LABEL_H = 36
PAD = 16
BG = (13, 17, 23)    # same dark background as the cards
FONT = "/System/Library/Fonts/Supplemental/Arial.ttf"
DEFAULT_LEVELS = [1.0, 0.9, 0.8, 0.7]


def main():
    if len(sys.argv) < 2:
        die("usage: preview_crop.py CLIP [LEVELS...] [--at SECONDS]")
    ensure_pillow()
    from PIL import Image, ImageDraw, ImageFont

    args = sys.argv[1:]
    at = 2.0
    out = None
    do_open = True
    if "--no-open" in args:
        do_open = False
        args.remove("--no-open")
    if "--at" in args:
        i = args.index("--at")
        at = float(args[i + 1])
        del args[i:i + 2]
    if "--out" in args:
        i = args.index("--out")
        out = args[i + 1]
        del args[i:i + 2]

    clip = args[0]
    levels = [float(x) for x in args[1:]] or DEFAULT_LEVELS
    src = clip if os.path.isabs(clip) else os.path.join(HERE, clip)
    if not os.path.exists(src):
        die(f"clip not found: {src}")

    tmp = tempfile.mkdtemp(prefix="cropprev_", dir=HERE)
    frame = os.path.join(tmp, "frame.png")
    r = subprocess.run(["ffmpeg", "-y", "-ss", str(at), "-i", src,
                        "-frames:v", "1", frame], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(frame):
        sys.stderr.write(r.stderr[-2000:] + "\n")
        die("could not extract a frame (is --at past the end of the clip?)")

    base = Image.open(frame).convert("RGB")
    W, H = base.size
    font = ImageFont.truetype(FONT, 26)

    tiles = []
    for lvl in levels:
        f = normalize_crop(lvl)
        cw, ch = int(W * f), int(H * f)
        x, y = (W - cw) // 2, (H - ch) // 2
        crop = base.crop((x, y, x + cw, y + ch))
        crop.thumbnail((THUMB, THUMB))              # fit to square keeping aspect
        tile = Image.new("RGB", (THUMB, THUMB + LABEL_H), BG)
        tile.paste(crop, ((THUMB - crop.width) // 2, (THUMB - crop.height) // 2))
        ImageDraw.Draw(tile).text((10, THUMB + 5), f"{round(f * 100)}%",
                                  font=font, fill="white")
        tiles.append(tile)

    sheet = Image.new("RGB", (len(tiles) * (THUMB + PAD) + PAD,
                              THUMB + LABEL_H + 2 * PAD), (0, 0, 0))
    cx = PAD
    for t in tiles:
        sheet.paste(t, (cx, PAD))
        cx += THUMB + PAD

    if out is None:
        out = os.path.join(HERE, "preview_crop.png")
    elif not os.path.isabs(out):
        out = os.path.join(HERE, out)
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    sheet.save(out)
    import shutil
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"{os.path.basename(clip)} @ {at}s -- crop levels "
          f"{[round(normalize_crop(l) * 100) for l in levels]}% -> {out}")
    if do_open:
        subprocess.run(["open", out])


if __name__ == "__main__":
    main()
