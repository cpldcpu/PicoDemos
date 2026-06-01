#!/usr/bin/env bash
# make_logo.sh — generate the chrome wordmark stencil (assets/_packed/logo.bin)
# as an antialiased 320x48 grayscale coverage mask, using ffmpeg drawtext. No
# GenAI needed; the title scene chrome-shades this mask. Re-run to change the
# word/font. Run from the quicksilver/ dir.
set -e
export PATH="/d/msys64/ucrt64/bin:$PATH"
cd "$(dirname "$0")/.."
WORD="${1:-QUICKSILVER}"
FONT="${2:-tools/_font.ttf}"     # a TTF copied locally (Windows colon paths break the filter parser)
[ -f "$FONT" ] || cp /c/Windows/Fonts/arialbd.ttf tools/_font.ttf
ffmpeg -y -hide_banner -loglevel error -f lavfi -i color=black:s=1408x192 \
  -vf "drawtext=fontfile=$FONT:text='$WORD':fontcolor=white:fontsize=150:x=(w-text_w)/2:y=(h-text_h)/2-10" \
  -frames:v 1 tools/logo_hi.png
ffmpeg -y -hide_banner -loglevel error -i tools/logo_hi.png \
  -vf "scale=300:42:flags=lanczos,pad=320:48:10:3:black" -pix_fmt gray -f rawvideo assets/_packed/logo.bin
echo "wrote assets/_packed/logo.bin (320x48 gray) for '$WORD'"
