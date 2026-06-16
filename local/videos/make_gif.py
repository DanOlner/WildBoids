#!/usr/bin/env python3
"""Make a high-quality GIF from a slice of a video clip.

Usage:
    python3 make_gif.py INPUT [-o OUTPUT] [--start S] [--end S]
                              [--duration S] [--fps N] [--width PX]

Requires ffmpeg on PATH (brew install ffmpeg). No other dependencies.

Uses ffmpeg's two-pass palette method (palettegen -> paletteuse) so the GIF
keeps clean colour without the banding/dither you get from a naive one-pass
conversion. The source clip is sliced to [--start, --end] (or --start +
--duration), scaled to --width (preserving aspect, never upscaling past the
source) and resampled to --fps.

Examples:
    # First 14 seconds of circling.mp4 -> ../../predator_flocking.gif
    python3 make_gif.py circling.mp4 --end 14 -o ../../predator_flocking.gif
"""

import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def die(msg):
    sys.stderr.write(f"error: {msg}\n")
    sys.exit(1)


def run(cmd):
    print("+ " + " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        die(f"command failed ({result.returncode}):\n{result.stderr.strip()}")
    return result


def source_width(path):
    """Width of the input video in pixels, so we never upscale."""
    out = run([
        "ffprobe", "-v", "error",
        "-select_streams", "v:0",
        "-show_entries", "stream=width",
        "-of", "csv=p=0",
        path,
    ]).stdout.strip()
    try:
        return int(out)
    except ValueError:
        return None


def main():
    ap = argparse.ArgumentParser(description="Make a high-quality GIF from a video slice.")
    ap.add_argument("input", help="source video (relative to CWD)")
    ap.add_argument("-o", "--output", help="output .gif path (default: INPUT with .gif)")
    ap.add_argument("--start", type=float, default=0.0, help="start time in seconds (default 0)")
    ap.add_argument("--end", type=float, help="end time in seconds")
    ap.add_argument("--duration", type=float, help="clip length in seconds (alternative to --end)")
    ap.add_argument("--fps", type=int, default=15, help="output frame rate (default 15)")
    ap.add_argument("--width", type=int, default=360,
                    help="output width in px, aspect preserved, never upscaled (default 360)")
    args = ap.parse_args()

    if not os.path.exists(args.input):
        die(f"input not found: {args.input}")
    if args.end is not None and args.duration is not None:
        die("pass --end or --duration, not both")

    if args.duration is not None:
        duration = args.duration
    elif args.end is not None:
        duration = args.end - args.start
    else:
        duration = None
    if duration is not None and duration <= 0:
        die("clip duration must be positive (check --start/--end)")

    output = args.output or os.path.splitext(args.input)[0] + ".gif"

    width = args.width
    src_w = source_width(args.input)
    if src_w and width > src_w:
        print(f"(requested width {width} > source {src_w}; clamping to source)")
        width = src_w

    # filter chain shared by both passes
    vf = f"fps={args.fps},scale={width}:-1:flags=lanczos"

    trim = ["-ss", str(args.start)]
    if duration is not None:
        trim += ["-t", str(duration)]

    with tempfile.TemporaryDirectory() as tmp:
        palette = os.path.join(tmp, "palette.png")
        # pass 1: build an optimal palette for this slice
        run(["ffmpeg", "-y", *trim, "-i", args.input,
             "-vf", f"{vf},palettegen=stats_mode=diff", palette])
        # pass 2: render the GIF using that palette
        run(["ffmpeg", "-y", *trim, "-i", args.input, "-i", palette,
             "-lavfi", f"{vf}[x];[x][1:v]paletteuse=dither=sierra2_4a",
             "-loop", "0", output])

    size_mb = os.path.getsize(output) / 1e6
    print(f"wrote {output} ({size_mb:.1f} MB)")


if __name__ == "__main__":
    main()
