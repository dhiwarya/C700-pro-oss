#include "theme.h"

lv_style_t theme_st_bg, theme_st_text, theme_st_muted, theme_st_panel, theme_st_line;
lv_style_t theme_st_go, theme_st_outline, theme_st_fill_fg;

/* "Daylight" and "Night" (docs/UI_DESIGN.md): near-white / near-black
 * grounds, one neutral surface, and state colours that keep 4.5:1 text
 * contrast on their own theme's background. */
static const theme_colors_t k_dark = {
    .bg     = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .fg     = LV_COLOR_MAKE(0xff, 0xff, 0xff),
    .muted  = LV_COLOR_MAKE(0xa3, 0xad, 0xb8),
    .line   = LV_COLOR_MAKE(0x2b, 0x31, 0x39),
    .panel  = LV_COLOR_MAKE(0x15, 0x18, 0x1d),
    .sel    = LV_COLOR_MAKE(0xff, 0xd4, 0x00),
    .sel_fg = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .ok     = LV_COLOR_MAKE(0x34, 0xd1, 0x7a),
    .ok_fg  = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .warn   = LV_COLOR_MAKE(0xff, 0xb0, 0x20),
    .route  = LV_COLOR_MAKE(0xe8, 0x44, 0x9e),
    .marker = LV_COLOR_MAKE(0x3d, 0x8b, 0xff),
};
static const theme_colors_t k_light = {
    .bg     = LV_COLOR_MAKE(0xff, 0xff, 0xff),
    .fg     = LV_COLOR_MAKE(0x0b, 0x0d, 0x10),
    .muted  = LV_COLOR_MAKE(0x4b, 0x55, 0x63),
    .line   = LV_COLOR_MAKE(0xcb, 0xd1, 0xd8),
    .panel  = LV_COLOR_MAKE(0xed, 0xf0, 0xf3),
    .sel    = LV_COLOR_MAKE(0xff, 0xd4, 0x00),
    .sel_fg = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .ok     = LV_COLOR_MAKE(0x0f, 0x7a, 0x3d),
    .ok_fg  = LV_COLOR_MAKE(0xff, 0xff, 0xff),
    .warn   = LV_COLOR_MAKE(0xa8, 0x5f, 0x00),
    .route  = LV_COLOR_MAKE(0xc4, 0x14, 0x6e),
    .marker = LV_COLOR_MAKE(0x1d, 0x63, 0xd6),
};

static theme_id_t s_id;
static const theme_colors_t *s_c = &k_dark;

static lv_style_t *const k_styles[] = {
    &theme_st_bg, &theme_st_text, &theme_st_muted, &theme_st_panel, &theme_st_line,
    &theme_st_go, &theme_st_outline, &theme_st_fill_fg,
};
#define N_STYLES (sizeof k_styles / sizeof k_styles[0])

static void apply(void)
{
    lv_style_set_bg_color(&theme_st_bg, s_c->bg);
    lv_style_set_text_color(&theme_st_text, s_c->fg);
    lv_style_set_text_color(&theme_st_muted, s_c->muted);
    lv_style_set_bg_color(&theme_st_panel, s_c->panel);
    lv_style_set_border_color(&theme_st_panel, s_c->line);
    lv_style_set_border_color(&theme_st_line, s_c->line);
    lv_style_set_bg_color(&theme_st_go, s_c->ok);
    lv_style_set_text_color(&theme_st_go, s_c->ok_fg);
    lv_style_set_border_color(&theme_st_outline, s_c->fg);
    lv_style_set_line_color(&theme_st_outline, s_c->fg);
    lv_style_set_bg_color(&theme_st_fill_fg, s_c->fg);
}

void theme_init(theme_id_t id)
{
    for (size_t i = 0; i < N_STYLES; i++) lv_style_init(k_styles[i]);
    s_id = id;
    s_c = id == THEME_LIGHT ? &k_light : &k_dark;
    apply();
}

void theme_set(theme_id_t id)
{
    s_id = id;
    s_c = id == THEME_LIGHT ? &k_light : &k_dark;
    apply();
    for (size_t i = 0; i < N_STYLES; i++) lv_obj_report_style_change(k_styles[i]);
}

theme_id_t theme_current(void) { return s_id; }
const theme_colors_t *theme_colors(void) { return s_c; }
const char *theme_name(theme_id_t id) { return id == THEME_LIGHT ? "Light" : "Dark"; }

lv_color_t theme_palette(lv_palette_t p)
{
    return s_id == THEME_LIGHT ? lv_palette_darken(p, 2) : lv_palette_main(p);
}
