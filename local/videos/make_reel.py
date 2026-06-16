#!/usr/bin/env python3
"""Build a single video reel from clips + title cards described in reel.json.

Usage:
    python3 make_reel.py [reel.json]

Requires ffmpeg on PATH (brew install ffmpeg) and Pillow (pip install Pillow).
If the interpreter you launch with lacks Pillow, the script re-execs itself
with one that has it (e.g. a conda/miniforge python).

Each clip can be any resolution/codec; they are normalized to a common square
canvas, fps and H.264 so they concatenate cleanly. Title cards are drawn with
Pillow (so we don't depend on ffmpeg being built with the drawtext filter).

If the manifest has a "music" block with a "file", the silent reel gets that
track muxed over it as a final step (video stream-copied, audio encoded).
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap

HERE = os.path.dirname(os.path.abspath(__file__))


def die(msg):
    sys.stderr.write(f"error: {msg}\n")
    sys.exit(1)


def ensure_pillow():
    """Cards are drawn with Pillow. If this interpreter lacks it, re-exec with
    one that has it (conda/miniforge pythons usually do)."""
    try:
        import PIL  # noqa: F401
        return
    except ImportError:
        pass
    candidates = [
        os.path.expanduser("~/miniforge3/bin/python3"),
        os.path.expanduser("~/miniconda3/bin/python3"),
        os.path.expanduser("~/anaconda3/bin/python3"),
        shutil.which("python3"),
    ]
    seen = {os.path.realpath(sys.executable)}
    for py in candidates:
        if not py or not os.path.exists(py) or os.path.realpath(py) in seen:
            continue
        seen.add(os.path.realpath(py))
        if subprocess.run([py, "-c", "import PIL"], capture_output=True).returncode == 0:
            print(f"(re-launching with Pillow-capable python: {py})")
            os.execv(py, [py] + sys.argv)
    die("Pillow not found. Install it:  python3 -m pip install Pillow")


def run(cmd):
    """Run ffmpeg, showing its command and surfacing errors."""
    print("  $ " + " ".join(repr(c) if " " in c else c for c in cmd))
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(res.stderr[-4000:] + "\n")
        die("ffmpeg failed (see output above)")


def normalize_crop(crop):
    """Accept 0.8 *or* 80 to mean 'keep central 80%'. Returns a 0<f<=1 fraction."""
    if crop is None:
        return 1.0
    f = crop / 100.0 if crop > 1 else float(crop)
    if not (0 < f <= 1):
        die(f"crop must be between 0 and 1 (or a percent up to 100); got {crop}")
    return f


def normalize_filter(w, h, fps, crop=1.0):
    """Optionally center-crop to the central 'crop' fraction (zooming in), then
    scale to fit inside WxH keeping aspect, pad the rest, fix sar/fps."""
    chain = []
    if crop < 1.0:
        chain.append(f"crop=iw*{crop}:ih*{crop}")  # crop is centered by default
    chain += [
        f"scale={w}:{h}:force_original_aspect_ratio=decrease",
        f"pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:color=black",
        "setsar=1", f"fps={fps}", "format=yuv420p",
    ]
    return ",".join(chain)


def render_clip(src, dst, cfg, start=None, end=None, duration=None, crop=None):
    """Trim with optional 'start'/'end' (seconds). Omit both for the whole clip.
    'duration' is still honoured for back-compat; 'end' wins if both are given.
    'crop' zooms in, keeping the central fraction (0.8 or 80 = central 80%)."""
    w, h, fps = cfg["width"], cfg["height"], cfg["fps"]
    crop = normalize_crop(crop)
    if end is not None:
        if end <= (start or 0):
            die(f"end ({end}) must be greater than start ({start or 0}) for {src}")
        duration = end - (start or 0)
    cmd = ["ffmpeg", "-y"]
    if start is not None:
        cmd += ["-ss", str(start)]
    cmd += ["-i", src]
    if duration is not None:
        cmd += ["-t", str(duration)]
    cmd += [
        "-an",  # drop audio so every segment is uniform
        "-vf", normalize_filter(w, h, fps, crop),
        "-c:v", "libx264", "-preset", "medium", "-crf", "20",
        "-pix_fmt", "yuv420p", "-r", str(fps),
        dst,
    ]
    run(cmd)


def render_card(text, dst, cfg, tmpdir, idx, seconds=None):
    """Draw a centered title card to a PNG (Pillow), then encode it to video.
    'seconds' overrides the global card_seconds for this card."""
    from PIL import Image, ImageColor, ImageDraw, ImageFont

    if seconds is None:
        seconds = cfg["card_seconds"]

    w, h, fps = cfg["width"], cfg["height"], cfg["fps"]
    title_fs = cfg["title_fontsize"]
    body_fs = cfg["body_fontsize"]
    font = cfg["font"]
    if not os.path.exists(font):
        die(f"font not found: {font} (set 'font' in the manifest to a .ttf/.ttc)")

    bg = ImageColor.getrgb(cfg["background"].replace("0x", "#"))
    fg = ImageColor.getrgb(cfg["fontcolor"])
    title_font = ImageFont.truetype(font, title_fs)
    body_font = ImageFont.truetype(font, body_fs)

    # First line = title; remaining lines = body (auto-wrapped).
    raw = text.split("\n")
    rows = []  # (text, font, fontsize)
    if raw:
        rows.append((raw[0], title_font, title_fs))
    for ln in raw[1:]:
        for wln in (textwrap.wrap(ln, cfg["max_chars"]) or [""]):
            rows.append((wln, body_font, body_fs))

    gap_after_title = title_fs * 0.4 if len(raw) > 1 else 0
    total = sum(fs * 1.45 for _, _, fs in rows) + gap_after_title
    y = (h - total) / 2.0

    img = Image.new("RGB", (w, h), bg)
    draw = ImageDraw.Draw(img)
    for i, (txt, fnt, fs) in enumerate(rows):
        tw = draw.textlength(txt, font=fnt)
        draw.text(((w - tw) / 2.0, y), txt, font=fnt, fill=fg)
        y += fs * 1.45
        if i == 0 and len(raw) > 1:
            y += gap_after_title  # gap between title and body

    png = os.path.join(tmpdir, f"card{idx}.png")
    img.save(png)
    run([
        "ffmpeg", "-y", "-loop", "1", "-i", png,
        "-t", str(seconds), "-r", str(fps),
        "-vf", "format=yuv420p",
        "-c:v", "libx264", "-preset", "medium", "-crf", "20",
        "-pix_fmt", "yuv420p",
        dst,
    ])


def probe_duration(path):
    res = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=nw=1:nk=1", path],
        capture_output=True, text=True)
    try:
        return float(res.stdout.strip())
    except ValueError:
        return None


def add_music(silent_video, out, music, cfg):
    """Mux a music track over the (silent) reel. Video is stream-copied, so
    only the audio is encoded. 'start' seeks into the track; the audio is
    padded/looped to cover the whole video and faded as configured."""
    mfile = music["file"]
    mpath = mfile if os.path.isabs(mfile) else os.path.join(HERE, mfile)
    if not os.path.exists(mpath):
        die(f"music file not found: {mpath}")

    dur = probe_duration(silent_video) or 0.0
    volume = music.get("volume", 1.0)
    fade_in = music.get("fade_in", 0)
    fade_out = music.get("fade_out", 0)
    start = music.get("start", 0)
    loop = music.get("loop", False)

    afilters = []
    if volume != 1.0:
        afilters.append(f"volume={volume}")
    if fade_in:
        afilters.append(f"afade=t=in:st=0:d={fade_in}")
    if fade_out and dur:
        afilters.append(f"afade=t=out:st={max(0.0, dur - fade_out):.3f}:d={fade_out}")
    afilters.append("apad")  # pad with silence so audio always covers the video

    cmd = ["ffmpeg", "-y", "-i", silent_video]
    if loop:
        cmd += ["-stream_loop", "-1"]   # input option: loop the music input
    if start:
        cmd += ["-ss", str(start)]      # input option: seek into the music
    cmd += [
        "-i", mpath,
        "-filter:a", ",".join(afilters),
        "-map", "0:v:0", "-map", "1:a:0",
        "-c:v", "copy", "-c:a", "aac", "-b:a", "192k",
        "-shortest", out,
    ]
    extra = (", looped" if loop else "") + (f", from {start}s" if start else "")
    print(f"\nAdding music: {mfile} (vol {volume}, "
          f"fade in {fade_in}s / out {fade_out}s{extra}) -> {out}")
    run(cmd)


def main():
    if not shutil.which("ffmpeg"):
        die("ffmpeg not on PATH. Install it with:  brew install ffmpeg")
    ensure_pillow()

    manifest = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "reel.json")
    with open(manifest) as fh:
        cfg = json.load(fh)

    segs = cfg.get("segments", [])
    if not segs:
        die("no 'segments' in manifest")

    tmpdir = tempfile.mkdtemp(prefix="reel_", dir=HERE)
    parts = []
    try:
        for idx, seg in enumerate(segs):
            label = seg.get("clip") or (seg.get("card", "").split("\n")[0])
            print(f"[{idx + 1}/{len(segs)}] {label}")

            if "card" in seg:
                card_mp4 = os.path.join(tmpdir, f"seg{idx:03d}_card.mp4")
                render_card(seg["card"], card_mp4, cfg, tmpdir, idx,
                            seconds=seg.get("card_seconds"))
                parts.append(card_mp4)

            if "clip" in seg:
                src = os.path.join(HERE, seg["clip"])
                if not os.path.exists(src):
                    die(f"clip not found: {src}")
                clip_mp4 = os.path.join(tmpdir, f"seg{idx:03d}_clip.mp4")
                render_clip(src, clip_mp4, cfg, start=seg.get("start"),
                            end=seg.get("end"), duration=seg.get("duration"),
                            crop=seg.get("crop"))
                parts.append(clip_mp4)

        # Concatenate (all parts share identical encoding -> stream copy).
        listfile = os.path.join(tmpdir, "concat.txt")
        with open(listfile, "w") as fh:
            for p in parts:
                fh.write(f"file '{p}'\n")

        out = os.path.join(HERE, cfg.get("output", "wildboids_reel.mp4"))
        music = cfg.get("music")
        has_music = bool(music and music.get("file"))

        # If adding music, concat to a silent temp file first, then mux.
        concat_target = os.path.join(tmpdir, "reel_silent.mp4") if has_music else out
        print(f"\nConcatenating {len(parts)} segments -> {concat_target}")
        run(["ffmpeg", "-y", "-f", "concat", "-safe", "0", "-i", listfile,
             "-c", "copy", concat_target])

        if has_music:
            add_music(concat_target, out, music, cfg)
        print(f"\nDone: {out}")
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    main()
