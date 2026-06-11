# Making a video reel

`make_reel.py` stitches the clips in this folder into one video, with text
"cards" between them. What goes in the reel — order, captions, trims, music —
is all controlled by a JSON manifest (`reel.json`); the script itself never
needs editing.

## Requirements

- **ffmpeg** on PATH — `brew install ffmpeg`
- **Pillow** (for drawing the cards) — `pip install Pillow`. If the `python3`
  you run with doesn't have it, the script auto-relaunches with one that does
  (e.g. a conda/miniforge python), so usually you don't have to think about it.

## Running it

```bash
python3 make_reel.py            # uses reel.json -> wildboids_reel.mp4
python3 make_reel.py test_reel.json   # any manifest you like
```

Re-running overwrites the output. Everything here is git-ignored scratch space.

## The manifest

### Top-level options

| Key              | Meaning                                                        |
|------------------|----------------------------------------------------------------|
| `output`         | Output filename (written into this folder).                    |
| `width`,`height` | Canvas size. Clips are scaled to fit and padded, so mixed source resolutions are fine. `1080`×`1080` (square) suits the near-square sim recordings. |
| `fps`            | Output frame rate (e.g. `30`).                                 |
| `background`     | Card background colour, e.g. `"0x0d1117"`.                      |
| `fontcolor`      | Card text colour, e.g. `"white"`.                              |
| `font`           | Path to a `.ttf`/`.ttc` (default: macOS Arial).                |
| `card_seconds`   | How long each card is shown.                                   |
| `title_fontsize` | Size of a card's **first** line (the title).                   |
| `body_fontsize`  | Size of the remaining card lines (the body).                   |
| `max_chars`      | Body text wraps at roughly this many characters per line.      |

### Segments

`segments` is an ordered list. Each entry can have a `card`, a `clip`, or both
(a card alone makes an intro/outro title screen; a clip alone plays with no
card before it).

| Key        | Meaning                                                              |
|------------|----------------------------------------------------------------------|
| `card`     | Card text. The **first line** is the big title; lines after it (split on `\n`) are smaller body text, auto-wrapped. |
| `clip`     | A video filename in this folder.                                     |
| `start`    | Seconds into the clip to begin (optional).                           |
| `end`      | Seconds into the clip to stop (optional).                            |
| `card_seconds` | How long *this* card shows, overriding the global default (optional). Handy for giving a wordier card more reading time. |

Omit both `start` and `end` to use the whole clip. Examples:

```json
{ "card": "Wild Boids\nEvolving predators and prey" },          // card only
{ "card": "A torus world\nBoids wrap around the edges",         // card + clip,
  "clip": "stage1_torustest.mov", "start": 3, "end": 8 },       //   trimmed 3-8s
{ "clip": "circling.mp4" }                                      // clip, no card
```

### Music (optional)

Add a `music` block to lay a soundtrack over the finished reel. Leave `file`
empty (or omit the block) to keep the reel silent. Drop an `mp3`/`m4a`/`wav`
into this folder and name it in `file`.

| Key        | Meaning                                                              |
|------------|----------------------------------------------------------------------|
| `file`     | Audio filename in this folder. Empty = no music.                     |
| `start`    | Seconds into the track to begin.                                     |
| `volume`   | `1.0` = unchanged, `0.7` = quieter, etc.                             |
| `loop`     | `true` repeats the track if it's shorter than the reel.              |
| `fade_in`  | Fade-in length in seconds.                                           |
| `fade_out` | Fade-out length in seconds (applied at the very end).                |

```json
"music": {
  "file": "soundtrack.m4a",
  "start": 0,
  "volume": 0.7,
  "loop": true,
  "fade_in": 1.5,
  "fade_out": 3.0
}
```

The video stream is copied untouched (fast, lossless); only the audio is
encoded. The track is padded with silence so a short song never truncates the
video. Use royalty-free music (e.g. YouTube Audio Library, Incompetech,
Pixabay) if you don't have your own.

## How it works (briefly)

1. Each card is drawn to a PNG with Pillow, then encoded to a short clip.
2. Each video clip is scaled/padded to the common canvas, set to the target
   fps, re-encoded to H.264, and its audio dropped.
3. All segments (identical format) are concatenated by stream copy.
4. If music is configured, it's muxed over the result as a final step.
