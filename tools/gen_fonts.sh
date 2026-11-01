#!/bin/sh
# Regenerates main/fonts/*.c from assets/fonts/*.ttf with lv_font_conv
# (npm package lv_font_conv@1.5.2). Point LV_FONT_CONV at the binary, e.g.
#   LV_FONT_CONV=../tools/node/node_modules/.bin/lv_font_conv tools/gen_fonts.sh
#
# ui_num_*   Barlow Semi Condensed SemiBold: data values and the clock. Narrow
#            digits let large numbers fit 240 px; full printable ASCII so
#            values like "SW", "50x17" or "--" render too.
# ui_label_* Montserrat Bold: the small upper-case field captions.
# ui_text_*  Montserrat SemiBold: status bar, rows, buttons, soft keys.
# Icons (LV_SYMBOL_*) come from the built-in Montserrat via the fallback.
set -e
cd "$(dirname "$0")/.."
CONV=${LV_FONT_CONV:-lv_font_conv}
SRC=assets/fonts
OUT=main/fonts
mkdir -p "$OUT"
ASCII="-r 0x20-0x7E -r 0xB0,0xB7,0x203A"   # + degree, middle dot, single right angle quote (chevron)

gen() {   # name font size [fallback]   (the font's C name is the file name)
    "$CONV" --font "$SRC/$2" $ASCII --size "$3" --bpp 4 --format lvgl --no-compress \
        --lv-include lvgl.h -o "$OUT/$1.c"
    python3 tools/fix_font.py "$OUT/$1.c" $4   # LVGL 9 cache guard, optional fallback font
    echo "$OUT/$1.c"
}

for s in 80 68 48 34 24 18; do gen ui_num_$s BarlowSemiCondensed-SemiBold.ttf $s; done
gen ui_label_10 Montserrat-Bold.ttf 10
for s in 12 14 16; do gen ui_text_$s Montserrat-SemiBold.ttf $s lv_font_montserrat_14; done

# header with the declarations
{
    echo "#pragma once"
    echo "#include \"lvgl.h\""
    for f in "$OUT"/*.c; do echo "LV_FONT_DECLARE($(basename "$f" .c));"; done
} > "$OUT/ui_fonts.h"
echo "$OUT/ui_fonts.h"
