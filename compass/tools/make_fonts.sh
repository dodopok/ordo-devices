#!/usr/bin/env bash
# Regenerates compass/src/ui/fonts/*.c: the bitmap fonts the screens use.
#
# The Ordo app sets its interface in Raleway and its liturgical text in
# Newsreader; the compass uses the same two. LVGL's built-in Montserrat only
# covers ASCII, which is why accents used to show as empty boxes.
#
#   tools/make_fonts.sh            (needs node 18+ and python3 with fonttools)
#
# Raleway's default figures are old-style; the `lnum` feature is frozen in so
# dates, clocks and counters use lining figures.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="$here/../src/ui/fonts"
work="${ORDO_FONT_WORK:-$here/.fontwork}"
mkdir -p "$work" "$out"
cd "$work"

[ -f package.json ] || npm init -y >/dev/null
npm install --silent lv_font_conv@1.5.3 @fontsource/raleway@5 @fontsource/newsreader@5
python3 -m pip install -q fonttools brotli opentype-feature-freezer

python3 - <<'PY'
from fontTools.ttLib import TTFont
base = 'node_modules/@fontsource'
for fam, names in {
    'raleway': ['raleway-latin-300-normal', 'raleway-latin-500-normal', 'raleway-latin-600-normal'],
    'newsreader': ['newsreader-latin-500-normal', 'newsreader-latin-400-italic'],
}.items():
    for n in names:
        t = TTFont(f'{base}/{fam}/files/{n}.woff')
        t.flavor = None
        t.save(f'{n}.ttf')
PY
for w in 300 500 600; do
  pyftfeatfreeze -f lnum "raleway-latin-$w-normal.ttf" "raleway-$w-lnum.ttf" >/dev/null
done

# Latin-1 plus the punctuation the screens use: dashes, curly quotes, bullet,
# ellipsis, the chevrons of "‹ voltar" and "Ordo › Ajustes", the minus sign.
TEXT="-r 0x20-0x7E -r 0xA0-0xFF -r 0x2013-0x2014 -r 0x2018-0x201D -r 0x2022 -r 0x2026 -r 0x2039-0x203A -r 0x2212"
conv="node_modules/.bin/lv_font_conv --bpp 4 --format lvgl --no-compress --lv-include lvgl.h"

gen() { # name file size ranges...
  local name="$1" file="$2" size="$3"; shift 3
  $conv --font "$file" --size "$size" "$@" -o "$out/$name.c" --lv-font-name "$name"
}

gen ordo_sans_14   raleway-600-lnum.ttf 14 $TEXT
gen ordo_sans_16   raleway-500-lnum.ttf 16 $TEXT
gen ordo_sans_18   raleway-500-lnum.ttf 18 $TEXT
gen ordo_sans_20   raleway-500-lnum.ttf 20 $TEXT
gen ordo_sans_24   raleway-500-lnum.ttf 24 $TEXT
gen ordo_serif_20i newsreader-latin-400-italic.ttf 20 $TEXT
gen ordo_serif_24  newsreader-latin-500-normal.ttf 24 $TEXT
gen ordo_serif_30  newsreader-latin-500-normal.ttf 30 $TEXT
gen ordo_serif_44  newsreader-latin-500-normal.ttf 44 $TEXT
# The clock and the counters: digits, colon and a space.
gen ordo_digits_104 raleway-300-lnum.ttf 104 -r 0x20 -r 0x30-0x3A
# The link code: capitals, digits and the hyphen.
gen ordo_code_44   raleway-600-lnum.ttf 44 -r 0x20 -r 0x2D -r 0x30-0x39 -r 0x41-0x5A

cp node_modules/@fontsource/raleway/LICENSE "$out/OFL-Raleway.txt"
cp node_modules/@fontsource/newsreader/LICENSE "$out/OFL-Newsreader.txt"
ls -la "$out"
