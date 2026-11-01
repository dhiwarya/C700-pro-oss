#!/usr/bin/env python3
"""Adapt an lv_font_conv 1.5.2 font source to LVGL 9.

    tools/fix_font.py main/fonts/ui_text_14.c [fallback_font]

- The glyph cache member only exists in LVGL 8: guard it the way LVGL 9's own
  built-in fonts do (`#if LVGL_VERSION_MAJOR == 8`).
- Optionally add `.fallback = &<font>` so glyphs missing here (LV_SYMBOL_*
  icons) come from that font.
"""
import sys

path = sys.argv[1]
fallback = sys.argv[2] if len(sys.argv) > 2 else None
s = open(path).read()

old_decl = ("#if LV_VERSION_CHECK(8, 0, 0)\n/*Store all the custom data of the font*/\n"
            "static  lv_font_fmt_txt_glyph_cache_t cache;\n")
new_decl = ("#if LV_VERSION_CHECK(8, 0, 0)\n#if LVGL_VERSION_MAJOR == 8\n"
            "static  lv_font_fmt_txt_glyph_cache_t cache;\n#endif\n")
old_use = "#if LV_VERSION_CHECK(8, 0, 0)\n    .cache = &cache\n#endif"
new_use = "#if LVGL_VERSION_MAJOR == 8\n    .cache = &cache\n#endif"
for old, new in ((old_decl, new_decl), (old_use, new_use)):
    if old not in s:
        sys.exit("%s: unexpected lv_font_conv output (%r not found)" % (path, old[:40]))
    s = s.replace(old, new)

if fallback:
    anchor = "    .dsc = &font_dsc"
    if anchor not in s:
        sys.exit("%s: no font descriptor found" % path)
    s = s.replace(anchor, "    .fallback = &%s,\n%s" % (fallback, anchor), 1)

open(path, "w").write(s)
