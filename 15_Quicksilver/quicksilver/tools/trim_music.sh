#!/usr/bin/env bash
# trim_music.sh — tighten the delivered soundtrack for the demo.
#
# "Taiko Dorian Bells" arrived at 4:09, which is too long for the effect
# inventory (scenes ended up ~35 s and the demo dragged). We remove the saggy
# sustained CENTRE (126.66 s .. 180.56 s — both structural-segment boundaries
# from analyze_music.py, so the cut is beat-aligned), keeping the intro build,
# DROP 1, the biggest drop, the second drop and the designed outro. The two
# halves are joined with one short equal-power crossfade so there is a single
# seam, not several. Result ~3:15.
#
# Usage:  ./trim_music.sh "../assets/Taiko Dorian Bells.mp3" ../music.qoa
set -e
SRC="$1"; OUT="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
export PATH="/d/msys64/ucrt64/bin:$PATH"

ffmpeg -y -i "$SRC" -filter_complex "\
[0:a]asplit=2[a0][b0];\
[a0]atrim=0:126.96,asetpts=PTS-STARTPTS[a];\
[b0]atrim=180.26,asetpts=PTS-STARTPTS[b];\
[a][b]acrossfade=d=0.6:c1=tri:c2=tri[out]" \
  -map "[out]" -ac 1 -ar 22050 -f s16le "$HERE/music.raw"

"$HERE/qoaconv_s16.exe" "$HERE/music.raw" "$OUT" 22050 1
echo "wrote $OUT"
