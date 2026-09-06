# media

| file | what it is |
|---|---|
| `persistence.mp4` | the complete 2:30 at 60 fps with sound, captured from the host player |
| `gallery.jpg` | one still per scene |
| `f*.png` | the same stills, individually |
| `song_roll.png` | the piano roll drawn from `song.c`'s own tables |
| `audit.txt` | the host referee's output over all 9,000 frames |
| `telemetry.txt` | the device's own report, one line a second, for the whole run |

## About the video

It is a **host capture**, not a recording of the Pico's VGA output — the same
arrangement every production in this repository uses, and it needs saying
plainly because this demo's whole claim is about what happens on the device.
What the video shows is the picture; what the device does with that picture is
in `telemetry.txt`, which is the file that carries the claim.

The two agree by construction rather than by inspection: the host player calls
`beam_frame(f)` once and then `beam_line(f, px, y)` for y = 0..479, in order,
which is exactly what core 1 does. There is no separate rendering path.

## Why this content is hard on a codec

Native 640×480 with nothing filtered: one-pixel-wide grid lines going to the
horizon, a dithered fog stipple that alternates every frame, flat-shaded facets
with hard edges, and a plasma with no flat area anywhere in it. All of that is
high-frequency detail that a codec is designed to throw away first.

`capture.py` therefore turns off the two defaults that would eat it —
`no-dct-decimate` and both deadzones — and raises the adaptive quantiser, at a
cost of about 5.8 Mbit/s. Encoding it at a sensible bitrate produces a video in
which the floor's grid shimmers and the stipple turns to mud, which would be a
recording of the encoder rather than of the demo.
