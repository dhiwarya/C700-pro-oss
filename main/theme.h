#pragma once
#include <stdbool.h>
#include "lvgl.h"

/* Light / dark colour theme. Objects that should follow the theme add one
 * of the shared styles below; changing the theme updates them in place
 * (lv_obj_report_style_change), so nothing needs to be rebuilt. Status
 * colours (green / orange / ...) go through theme_palette() so they stay
 * readable on both backgrounds. */

typedef enum { THEME_DARK = 0, THEME_LIGHT = 1 } theme_id_t;

typedef struct {
    lv_color_t bg;        /* page background */
    lv_color_t fg;        /* primary text */
    lv_color_t muted;     /* captions, units, secondary text */
    lv_color_t line;      /* cell borders, list separators */
    lv_color_t panel;     /* menu rows, boxes */
    lv_color_t sel;       /* selection / accent (yellow) */
    lv_color_t sel_fg;    /* text on the selection colour */
    lv_color_t ok;        /* ready / recording / Start button */
    lv_color_t ok_fg;     /* text on the ok colour */
    lv_color_t warn;      /* searching / paused */
    lv_color_t route;     /* GPX route on the map, route progress */
    lv_color_t marker;    /* position marker */
} theme_colors_t;

extern lv_style_t theme_st_bg;        /* bg_color = bg (needs bg_opa on the object) */
extern lv_style_t theme_st_text;      /* text_color = fg */
extern lv_style_t theme_st_muted;     /* text_color = muted */
extern lv_style_t theme_st_panel;     /* bg_color = panel, border_color = line */
extern lv_style_t theme_st_line;      /* border_color = line */
extern lv_style_t theme_st_go;        /* bg_color = ok, text_color = ok_fg (the Start button) */
extern lv_style_t theme_st_outline;   /* border_color = line_color = fg (icons drawn with borders / lines) */
extern lv_style_t theme_st_fill_fg;   /* bg_color = fg */

void theme_init(theme_id_t id);
void theme_set(theme_id_t id);
theme_id_t theme_current(void);
const theme_colors_t *theme_colors(void);
const char *theme_name(theme_id_t id);
/* A palette colour with enough contrast for the current background. */
lv_color_t theme_palette(lv_palette_t p);
