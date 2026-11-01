/* UI: a home page (clock, ready checklist, Menu / Start), a status page
 * (battery arc, environment, key indicators, event log, heap stats) and the
 * user-configurable data pages (config.c: layout + field per cell, edited
 * in the settings menu). Idle mode cycles home <-> status with KEY_NEXT_PAGE;
 * a ride (ride.c) cycles the enabled data pages.
 *
 * Look: "Daylight" / "Night" (docs/UI_DESIGN.md). Every page has the same
 * status bar: on the left a chip naming what key 0 does next (it sits by
 * that key), the ride state, then GPS, battery and the settings gear. Ride
 * pages end in a soft-key bar labelling the two bottom keys. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "board.h"
#include "ui_port.h"
#include "ui.h"
#include "stats.h"
#include "fields.h"
#include "sun.h"
#include "utc.h"
#include "route.h"
#include "config.h"
#include "datapage.h"
#include "menu.h"
#include "theme.h"
#include "mapview.h"
#include "fonts/ui_fonts.h"

#define LOG_LINES 2
#define MAX_PAGES (3 + CFG_PAGES)   /* idle, status, map, data pages */
#define HDR_H 24
#define PAGE_IDLE 0
#define PAGE_STATUS 1
#define PAGE_MAP 2
#define PAGE_FIRST_DATA 3

/* Soft-key bar: only where the two ride keys are the bottom-left and
 * bottom-right buttons (C606 Pro / CC700 Pro). */
#if KEY_LAP == 1 && KEY_RIDE == 2
#define SOFT_H 24
#else
#define SOFT_H 0
#endif

/* status bar of one page */
typedef struct {
    lv_obj_t *chip_lbl, *pill, *pill_dot, *pill_lbl, *gps_ring, *sats, *bat_fill, *bat_lbl;
} hdr_t;
/* soft-key bar of one page */
typedef struct {
    lv_obj_t *left, *right, *hint;
} soft_t;

static lv_obj_t *s_page_idle, *s_page_status, *s_page_map;
static lv_obj_t *s_pages[MAX_PAGES];
static hdr_t s_hdr[MAX_PAGES];
static soft_t s_soft[MAX_PAGES];
static int s_npages, s_page_idx;
static int s_ring[MAX_PAGES], s_ring_n;  /* pages reachable with KEY_NEXT_PAGE in the current mode */
static ride_mode_t s_mode = RIDE_IDLE;
static datapage_t s_dp[CFG_PAGES];          /* one per configured page */
static datapage_t s_dp_map;                 /* field strip under the map */
static int s_dp_of_page[MAX_PAGES];         /* page index -> config page */
static uint8_t s_bat_pct;
static bool s_bat_valid;
static gps_fix_t s_fix;                     /* last fix, for the status bars and the home checklist */
static lv_obj_t *s_arc, *s_arc_lbl, *s_env, *s_nrf, *s_gps, *s_sd, *s_log, *s_foot;
static lv_obj_t *s_key[NUM_KEYS];
static lv_obj_t *s_idle_clock, *s_idle_date;
enum { ROW_GPS, ROW_ROUTE, ROW_SENSORS, ROW_SUNSET, N_ROWS };
static lv_obj_t *s_row_dot[N_ROWS], *s_row_val[N_ROWS];
static const ant_channel_t *s_ant_ch;
static size_t s_ant_n;
static lv_obj_t *s_touch_lbl, *s_btn_start, *s_ant;
static ui_action_cb_t s_on_start, s_on_usb, s_on_usb_reboot, s_on_power_off, s_on_end_ride;
static lv_obj_t *s_popup;
static ui_popup_t s_popup_kind;
static lv_timer_t *s_popup_timer;

static char s_log_buf[LOG_LINES][32];
static int s_log_n;
static uint8_t s_bl_pct = 70;
static uint32_t s_uptime;

static const lv_color_t C_IDLE = LV_COLOR_MAKE(0x30, 0x30, 0x30);
static const lv_color_t C_HOLD = LV_COLOR_MAKE(0xd0, 0x20, 0x20);
static const lv_color_t C_CLK  = LV_COLOR_MAKE(0x20, 0xa0, 0x30);

static void build_hdr(lv_obj_t *pg, hdr_t *h);
static void ui_set_battery_bars(void);
static void build_soft(lv_obj_t *pg, soft_t *s);

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

/* A label whose colour follows the theme (theme_st_text / theme_st_muted). */
static lv_obj_t *tlabel(lv_obj_t *parent, const lv_font_t *font, lv_style_t *style)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_add_style(l, style, 0);
    return l;
}

static lv_obj_t *page(lv_obj_t *parent)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, LCD_H_RES, LCD_V_RES);
    lv_obj_add_style(p, &theme_st_bg, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    return p;
}

static void build_status(lv_obj_t *scr)
{
    s_page_status = page(scr);
    build_hdr(s_page_status, &s_hdr[PAGE_STATUS]);

    /* battery arc */
    s_arc = lv_arc_create(s_page_status);
    lv_obj_set_size(s_arc, 120, 120);
    lv_obj_align(s_arc, LV_ALIGN_TOP_MID, 0, 34);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(s_arc, false);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, C_IDLE, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, C_CLK, LV_PART_INDICATOR);
    s_arc_lbl = tlabel(s_arc, &lv_font_montserrat_20, &theme_st_text);
    lv_label_set_text(s_arc_lbl, "--\n-- mV");
    lv_obj_set_style_text_align(s_arc_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_arc_lbl);

    s_env = tlabel(s_page_status, &lv_font_montserrat_14, &theme_st_text);
    lv_label_set_text(s_env, "no sensor data yet");
    lv_obj_align(s_env, LV_ALIGN_TOP_MID, 0, 160);

    s_nrf = label(s_page_status, &lv_font_montserrat_14, theme_palette(LV_PALETTE_ORANGE));
    lv_label_set_text(s_nrf, "nRF: no reply yet");
    lv_obj_align(s_nrf, LV_ALIGN_TOP_MID, 0, 178);

    s_gps = label(s_page_status, &lv_font_montserrat_14, theme_palette(LV_PALETTE_ORANGE));
    lv_label_set_text(s_gps, "GPS: probing baud");
    lv_obj_align(s_gps, LV_ALIGN_TOP_MID, 0, 196);

    s_sd = label(s_page_status, &lv_font_montserrat_14, theme_palette(LV_PALETTE_ORANGE));
    lv_label_set_text(s_sd, "SD: not mounted");
    lv_obj_align(s_sd, LV_ALIGN_TOP_MID, 0, 214);

    /* key indicators: one box per key across the width */
    for (int i = 0; i < NUM_KEYS; i++) {
        int pitch = (LCD_H_RES - 24) / NUM_KEYS;
        s_key[i] = lv_obj_create(s_page_status);
        lv_obj_remove_style_all(s_key[i]);
        lv_obj_set_size(s_key[i], pitch - 12, 26);
        lv_obj_set_style_bg_color(s_key[i], C_IDLE, 0);
        lv_obj_set_style_bg_opa(s_key[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_key[i], 6, 0);
        lv_obj_align(s_key[i], LV_ALIGN_TOP_LEFT, 12 + i * pitch, 236);
        lv_obj_t *l = label(s_key[i], &lv_font_montserrat_14, lv_color_white());
        lv_label_set_text_fmt(l, "key %d", i);
        lv_obj_center(l);
    }

    /* event log */
    s_log = tlabel(s_page_status, &lv_font_unscii_8, &theme_st_muted);
    lv_obj_set_width(s_log, LCD_H_RES - 16);
    lv_obj_align(s_log, LV_ALIGN_TOP_LEFT, 8, 268);
    lv_label_set_text(s_log, "press a key...");

    s_touch_lbl = label(s_page_status, &lv_font_unscii_8, theme_palette(LV_PALETTE_CYAN));
    lv_obj_align(s_touch_lbl, LV_ALIGN_TOP_LEFT, 8, 290);
    lv_label_set_text(s_touch_lbl, "touch: none");

    s_ant = label(s_page_status, &lv_font_unscii_8, theme_palette(LV_PALETTE_PINK));
    lv_obj_align(s_ant, LV_ALIGN_TOP_LEFT, 120, 290);
    lv_label_set_text(s_ant, "ANT: waiting for nRF");

    /* footer */
    s_foot = tlabel(s_page_status, &lv_font_unscii_8, &theme_st_muted);
    lv_obj_align(s_foot, LV_ALIGN_BOTTOM_LEFT, 8, -4);
    lv_label_set_text(s_foot, "");
}

static lv_obj_t *button(lv_obj_t *parent, const char *txt, lv_color_t color, int w, int h)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, color, 0);
    lv_obj_t *l = label(b, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return b;
}

static void start_btn_cb(lv_event_t *e) { if (s_on_start) s_on_start(); }
static void menu_btn_cb(lv_event_t *e) { menu_open(); }

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_clickable(o, false);
    return o;
}

/* Puts a label in `font` so its digit tops are at y (number fonts have
 * room above the digits). */
static void place_digits(lv_obj_t *l, const lv_font_t *font, int32_t x, int32_t y)
{
    lv_font_glyph_dsc_t g;
    int top = lv_font_get_glyph_dsc(font, &g, '0', 0) ? g.box_h + g.ofs_y : font->line_height;
    lv_obj_set_pos(l, x, y - (font->line_height - font->base_line - top));
}

/* A status dot: filled in `c`, or a ring when !filled. */
static void set_dot(lv_obj_t *dot, lv_color_t c, bool filled)
{
    lv_obj_set_style_bg_color(dot, c, 0);
    lv_obj_set_style_bg_opa(dot, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(dot, c, 0);
    lv_obj_set_style_border_width(dot, filled ? 0 : 2, 0);
}

/* A 48 px tall button: `go` = the green Start style, else the neutral one. */
static lv_obj_t *big_button(lv_obj_t *parent, const char *txt, bool go, int32_t x, int32_t y, int32_t w, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 48);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_add_style(b, go ? &theme_st_go : &theme_st_panel, 0);
    if (!go) lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, go ? &ui_text_16 : &ui_text_14, 0);
    lv_obj_add_style(l, go ? &theme_st_go : &theme_st_text, 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return b;
}

/* Home: the clock, a "ready to ride?" checklist, Menu above key 1 and
 * Start above key 2. */
static void build_idle(lv_obj_t *scr)
{
    static const char *const names[N_ROWS] = { "GPS", "Route", "Sensors", "Sunset" };
    s_page_idle = page(scr);
    build_hdr(s_page_idle, &s_hdr[PAGE_IDLE]);

    s_idle_clock = tlabel(s_page_idle, &ui_num_68, &theme_st_text);
    lv_label_set_text(s_idle_clock, "--:--");
    place_digits(s_idle_clock, &ui_num_68, 14, HDR_H + 14);

    s_idle_date = tlabel(s_page_idle, &ui_text_14, &theme_st_muted);
    lv_label_set_text(s_idle_date, "");
    lv_obj_set_pos(s_idle_date, 14, HDR_H + 70);

    const int row_y = HDR_H + 100, row_h = 32;
    for (int i = 0; i < N_ROWS; i++) {
        lv_obj_t *row = plain(s_page_idle);
        lv_obj_set_pos(row, 0, row_y + i * row_h);
        lv_obj_set_size(row, LCD_H_RES, row_h);
        lv_obj_add_style(row, &theme_st_line, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, i ? LV_BORDER_SIDE_BOTTOM : LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);

        s_row_dot[i] = plain(row);
        lv_obj_set_size(s_row_dot[i], 8, 8);
        lv_obj_set_style_radius(s_row_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_align(s_row_dot[i], LV_ALIGN_LEFT_MID, 14, 0);
        set_dot(s_row_dot[i], theme_colors()->muted, false);

        lv_obj_t *n = tlabel(row, &ui_text_12, &theme_st_muted);
        lv_label_set_text(n, names[i]);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 30, 0);

        s_row_val[i] = tlabel(row, &ui_text_14, &theme_st_text);
        lv_label_set_long_mode(s_row_val[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_row_val[i], LCD_H_RES - 30 - 64 - 14);
        lv_obj_set_style_text_align(s_row_val[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(s_row_val[i], "--");
        lv_obj_align(s_row_val[i], LV_ALIGN_RIGHT_MID, -14, 0);
    }

    const int by = LCD_V_RES - 8 - 48;
    big_button(s_page_idle, "Menu", false, 6, by, 72, menu_btn_cb);
    s_btn_start = big_button(s_page_idle, LV_SYMBOL_PLAY "  Start ride", true, 86, by, LCD_H_RES - 86 - 6, start_btn_cb);
}

/* ---- status bar and soft keys -------------------------------------------- */

static void gear_cb(lv_event_t *e) { menu_open(); }

static void build_hdr(lv_obj_t *pg, hdr_t *h)
{
    lv_obj_t *bar = plain(pg);
    lv_obj_set_size(bar, LCD_H_RES, HDR_H);
    lv_obj_add_style(bar, &theme_st_bg, 0);
    lv_obj_add_style(bar, &theme_st_line, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);

    /* left: the key-0 chip, flush with the left edge (its left corners are
     * off screen), then the ride state */
    lv_obj_t *lg = plain(bar);
    lv_obj_set_size(lg, LV_SIZE_CONTENT, HDR_H - 1);
    lv_obj_set_pos(lg, -4, 0);
    lv_obj_set_flex_flow(lg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lg, 8, 0);
    lv_obj_t *chip = plain(lg);
    lv_obj_add_style(chip, &theme_st_panel, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(chip, 4, 0);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 18);
    lv_obj_set_style_pad_left(chip, 10, 0);
    lv_obj_set_style_pad_right(chip, 6, 0);
    h->chip_lbl = tlabel(chip, &ui_label_10, &theme_st_muted);
    lv_obj_set_style_text_letter_space(h->chip_lbl, 1, 0);
    lv_label_set_text(h->chip_lbl, "");
    lv_obj_align(h->chip_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    /* ride state: dot + REC / PAUSED */
    h->pill = plain(lg);
    lv_obj_set_size(h->pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(h->pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h->pill, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(h->pill, 5, 0);
    h->pill_dot = plain(h->pill);
    lv_obj_set_size(h->pill_dot, 7, 7);
    lv_obj_set_style_radius(h->pill_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(h->pill_dot, LV_OPA_COVER, 0);
    h->pill_lbl = lv_label_create(h->pill);
    lv_obj_set_style_text_font(h->pill_lbl, &ui_label_10, 0);
    lv_obj_set_style_text_letter_space(h->pill_lbl, 1, 0);
    lv_label_set_text(h->pill_lbl, "");
    lv_obj_set_hidden(h->pill, true);

    /* right: GPS, battery, gear */
    lv_obj_t *rg = plain(bar);
    lv_obj_set_size(rg, LV_SIZE_CONTENT, HDR_H - 1);
    lv_obj_set_flex_flow(rg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rg, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rg, 3, 0);
    lv_obj_align(rg, LV_ALIGN_TOP_RIGHT, 0, 0);

    h->gps_ring = plain(rg);
    lv_obj_set_size(h->gps_ring, 10, 10);
    lv_obj_set_style_radius(h->gps_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_style(h->gps_ring, &theme_st_outline, 0);
    lv_obj_set_style_border_width(h->gps_ring, 2, 0);
    h->sats = tlabel(rg, &ui_text_12, &theme_st_text);
    lv_label_set_text(h->sats, "--");
    lv_obj_set_style_margin_right(h->sats, 6, 0);

    lv_obj_t *bat = plain(rg);
    lv_obj_set_size(bat, 18, 11);
    lv_obj_add_style(bat, &theme_st_outline, 0);
    lv_obj_set_style_border_width(bat, 1, 0);
    lv_obj_set_style_radius(bat, 2, 0);
    lv_obj_set_style_pad_all(bat, 0, 0);
    h->bat_fill = plain(bat);
    lv_obj_set_pos(h->bat_fill, 2, 2);
    lv_obj_set_size(h->bat_fill, 0, 5);
    lv_obj_set_style_bg_opa(h->bat_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(h->bat_fill, 1, 0);
    lv_obj_t *nub = plain(rg);
    lv_obj_set_size(nub, 2, 5);
    lv_obj_add_style(nub, &theme_st_fill_fg, 0);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
    lv_obj_set_style_margin_left(nub, -3, 0);
    h->bat_lbl = tlabel(rg, &ui_text_12, &theme_st_text);
    lv_label_set_text(h->bat_lbl, "--%");

    lv_obj_t *gear = lv_obj_create(rg);
    lv_obj_remove_style_all(gear);
    lv_obj_set_size(gear, 28, HDR_H - 1);
    lv_obj_add_event_cb(gear, gear_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *g = tlabel(gear, &lv_font_montserrat_14, &theme_st_text);
    lv_label_set_text(g, LV_SYMBOL_SETTINGS);
    lv_obj_center(g);
}

/* Two tabs at the bottom corners naming what the keys below them do. Each
 * tab overhangs the screen so only its inner top corner shows rounded. */
static lv_obj_t *soft_tab(lv_obj_t *pg, bool right)
{
    lv_obj_t *t = plain(pg);
    lv_obj_add_style(t, &theme_st_panel, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(t, 6, 0);
    lv_obj_set_size(t, LV_SIZE_CONTENT, 22 + 6);
    lv_obj_set_style_pad_left(t, right ? 12 : 6 + 8, 0);
    lv_obj_set_style_pad_right(t, right ? 6 + 8 : 12, 0);
    lv_obj_set_style_pad_bottom(t, 6, 0);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(t, 6, 0);
    lv_obj_align(t, right ? LV_ALIGN_BOTTOM_RIGHT : LV_ALIGN_BOTTOM_LEFT, right ? 6 : -6, 6);
    return t;
}

static void build_soft(lv_obj_t *pg, soft_t *s)
{
#if SOFT_H
    lv_obj_t *l = soft_tab(pg, false);
    s->left = tlabel(l, &ui_label_10, &theme_st_text);
    lv_obj_set_style_text_letter_space(s->left, 1, 0);
    lv_label_set_text(s->left, "");
    lv_obj_t *r = soft_tab(pg, true);
    s->hint = tlabel(r, &ui_label_10, &theme_st_muted);
    lv_label_set_text(s->hint, "hold: end");
    s->right = tlabel(r, &ui_label_10, &theme_st_text);
    lv_obj_set_style_text_letter_space(s->right, 1, 0);
    lv_label_set_text(s->right, "");
#else
    (void)pg;
    memset(s, 0, sizeof *s);
#endif
}

/* Map page: status bar, the map canvas (mapview.c), its field strip and the
 * soft keys. */
static void build_map(lv_obj_t *scr)
{
    s_page_map = page(scr);
    lv_obj_set_hidden(s_page_map, true);
    build_hdr(s_page_map, &s_hdr[PAGE_MAP]);
    build_soft(s_page_map, &s_soft[PAGE_MAP]);
    const int h = LCD_V_RES - HDR_H - SOFT_H;
    /* the strip is built first so the map widgets end up above it */
    datapage_build(&s_dp_map, s_page_map, &config_get()->map_page, 0, HDR_H, LCD_H_RES, h);
    mapview_create(s_page_map, HDR_H, LCD_H_RES, h);
    mapview_set_area(HDR_H, h - layout_strip_h(layout_get(config_get()->map_page.layout)));
}

/* The map page's layout / theme may have changed (menu closed). */
static void rebuild_map(void)
{
    const page_cfg_t *mp = &config_get()->map_page;
    const int h = LCD_V_RES - HDR_H - SOFT_H;
    datapage_build(&s_dp_map, s_page_map, mp, 0, HDR_H, LCD_H_RES, h);
    mapview_set_area(HDR_H, h - layout_strip_h(layout_get(mp->layout)));
}

/* (Re)creates the enabled data pages from the configuration. */
static void build_data_pages(lv_obj_t *scr)
{
    for (int p = 0; p < CFG_PAGES; p++) {
        if (s_dp[p].cont) datapage_delete(&s_dp[p]);
    }
    for (int i = PAGE_FIRST_DATA; i < s_npages; i++) {
        lv_obj_delete(s_pages[i]);
        s_pages[i] = NULL;
        memset(&s_hdr[i], 0, sizeof s_hdr[i]);
        memset(&s_soft[i], 0, sizeof s_soft[i]);
    }
    s_npages = PAGE_FIRST_DATA;

    const app_cfg_t *cfg = config_get();
    for (int p = 0; p < CFG_PAGES; p++) {
        if (!cfg->page[p].enabled) continue;
        int idx = s_npages++;
        lv_obj_t *pg = page(scr);
        lv_obj_set_hidden(pg, true);
        s_pages[idx] = pg;
        s_dp_of_page[idx] = p;
        build_hdr(pg, &s_hdr[idx]);
        build_soft(pg, &s_soft[idx]);
        datapage_build(&s_dp[p], pg, &cfg->page[p], 0, HDR_H, LCD_H_RES, LCD_V_RES - HDR_H - SOFT_H);
    }
}

/* ---- live content ------------------------------------------------------- */

static const char *page_name(int idx)
{
    static char buf[16];
    if (idx == PAGE_IDLE) return "HOME";
    if (idx == PAGE_STATUS) return "STATUS";
    if (idx == PAGE_MAP) return "MAP";
    snprintf(buf, sizeof buf, "P%d", s_dp_of_page[idx] + 1);
    return buf;
}

/* Status bar chip (what key 0 does: the next page, or the position in the
 * ring during a ride), ride state and soft keys of the visible page. */
static void refresh_bars(void)
{
    if (s_page_idx < 0) return;
    hdr_t *h = &s_hdr[s_page_idx];
    if (!h->chip_lbl) return;
    const theme_colors_t *c = theme_colors();
    int pos = 0;
    for (int i = 0; i < s_ring_n; i++) if (s_ring[i] == s_page_idx) pos = i;
    if (s_mode == RIDE_IDLE) {
        lv_label_set_text_fmt(h->chip_lbl, "%s \xE2\x80\xBA", s_ring_n > 1 ? page_name(s_ring[(pos + 1) % s_ring_n]) : page_name(s_page_idx));
        lv_obj_set_hidden(h->pill, true);
    } else {
        lv_label_set_text_fmt(h->chip_lbl, "%d/%d \xE2\x80\xBA", pos + 1, s_ring_n);
        bool rec = s_mode == RIDE_RIDING;
        lv_color_t sc = rec ? c->ok : c->warn;
        lv_obj_set_style_bg_color(h->pill_dot, sc, 0);
        lv_obj_set_style_text_color(h->pill_lbl, sc, 0);
        lv_label_set_text(h->pill_lbl, rec ? "REC" : ride_auto_paused() ? "AUTO-PAUSED" : "PAUSED");
        lv_obj_set_hidden(h->pill, false);
    }
    soft_t *s = &s_soft[s_page_idx];
    if (s->left) {
        bool idle = s_mode == RIDE_IDLE;
        lv_label_set_text(s->left, idle ? "MENU" : "LAP");
        lv_label_set_text(s->right, idle ? "START" : s_mode == RIDE_RIDING ? "PAUSE" : "RESUME");
        if (idle) lv_obj_set_hidden(s->hint, true);
        else lv_obj_set_hidden(s->hint, false);
    }
}

static void row(int r, lv_color_t dot, bool filled, bool muted, const char *fmt, ...)
{
    char buf[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    set_dot(s_row_dot[r], dot, filled);
    lv_label_set_text(s_row_val[r], buf);
    lv_obj_set_style_text_color(s_row_val[r], muted ? theme_colors()->muted : theme_colors()->fg, 0);
}

/* "Sunday, 1 November" in local time */
static void fmt_date(char *buf, size_t n)
{
    static const char *const wd[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
    static const char *const mo[] = { "January", "February", "March", "April", "May", "June", "July",
                                      "August", "September", "October", "November", "December" };
    uint32_t t;
    if (!utc_now(&t)) { buf[0] = 0; return; }
    int64_t days = ((int64_t)t + config_get()->tz_min * 60) / 86400;
    int w = (int)((days + 4) % 7);                      /* 1970-01-01 was a Thursday */
    /* civil from days (H. Hinnant) */
    int64_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    snprintf(buf, n, "%s, %u %s", wd[w], d, mo[m - 1]);
}

/* Home page: clock, date and the checklist. */
static void idle_refresh(void)
{
    const theme_colors_t *c = theme_colors();
    char buf[24], buf2[24];
    field_value(FIELD_TIME_OF_DAY, buf, sizeof buf);
    buf[5] = 0;                                         /* HH:MM */
    lv_label_set_text(s_idle_clock, buf);
    fmt_date(buf, sizeof buf);
    lv_label_set_text(s_idle_date, buf);

    if (s_fix.baud == 0) row(ROW_GPS, c->muted, false, true, "No data");
    else if (s_fix.valid) row(ROW_GPS, c->ok, true, false, "Ready \xC2\xB7 %u sats", s_fix.sats_used);
    else row(ROW_GPS, c->warn, true, false, "Searching \xC2\xB7 %u sats", s_fix.sats_used);

    if (route_loaded()) {
        const char *name = route_name();
        const char *slash = strrchr(name, '/');
        if (slash) name = slash + 1;
        int len = (int)strlen(name);
        if (len > 4 && !strcasecmp(name + len - 4, ".gpx")) len -= 4;
        row(ROW_ROUTE, c->route, true, false, "%.*s \xC2\xB7 %.1f km", len, name, route_length_m() / 1000.0f);
    } else {
        row(ROW_ROUTE, c->muted, false, true, "None");
    }

    if (!s_ant_n) {
        row(ROW_SENSORS, c->muted, false, true, "None paired");
    } else {
        uint32_t now = esp_timer_get_time() / 1000;
        int live = 0, n = 0;
        buf[0] = 0;
        for (size_t i = 0; i < s_ant_n; i++) {
            const ant_channel_t *ch = &s_ant_ch[i];
            if (ch->state == ANT_ST_CONNECTED && ch->pages && now - ch->last_rx_ms < ANT_LIVE_MS) live++;
            const char *nm = ch->dev_type == ANT_DEV_HR ? "HR" : ch->dev_type == ANT_DEV_CADENCE ? "Cad"
                           : ch->dev_type == ANT_DEV_SPEED ? "Speed" : ch->dev_type == ANT_DEV_SPD_CAD ? "Spd/Cad"
                           : ch->dev_type == ANT_DEV_POWER ? "Power" : ant_dev_name(ch->dev_type);
            n += snprintf(buf + n, sizeof buf - n, "%s%s", i ? ", " : "", nm);
            if (n >= (int)sizeof buf) break;
        }
        row(ROW_SENSORS, live == (int)s_ant_n ? c->ok : c->warn, live > 0, false, "%s", buf);
    }

    if (sun_has_position()) {
        field_value(FIELD_SUNSET, buf, sizeof buf);
        field_value(FIELD_SUNSET_IN, buf2, sizeof buf2);
        int hh, mm;
        if (sscanf(buf2, "%d:%d", &hh, &mm) == 2) row(ROW_SUNSET, c->warn, false, false, "%s \xC2\xB7 in %d h %02d", buf, hh, mm);
        else row(ROW_SUNSET, c->warn, false, false, "%s", buf);
    } else {
        row(ROW_SUNSET, c->muted, false, true, "Needs a GPS fix");
    }
}

/* Auto theme: light between sunrise and sunset, dark otherwise. The
 * manual choice (config theme) is left alone so switching auto off
 * returns to it. Not while the menu is open (its rows keep local colours). */
static void apply_theme_colors(void);
static void auto_theme_tick(void)
{
    bool day;
    if (!config_get()->theme_auto || !sun_is_day(&day)) return;
    theme_id_t want = day ? THEME_LIGHT : THEME_DARK;
    if (theme_current() == want) return;
    theme_set(want);
    apply_theme_colors();
    rebuild_map();
}

/* Refreshes the visible page's live content (LVGL task, 2 Hz). */
static void data_refresh_cb(lv_timer_t *t)
{
    if (menu_active() || s_page_idx < 0) return;
    auto_theme_tick();
    refresh_bars();
    if (s_page_idx == PAGE_IDLE) {
        idle_refresh();
    } else if (s_page_idx == PAGE_MAP) {
        datapage_refresh(&s_dp_map);
    } else if (s_page_idx >= PAGE_FIRST_DATA && s_page_idx < s_npages) {
        datapage_refresh(&s_dp[s_dp_of_page[s_page_idx]]);
    }
}

/* Pages never scroll: a horizontal drag is a page swipe (gesture_cb), and
 * LVGL only reports gestures when no object took the drag as a scroll. */
static void no_scroll(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) no_scroll(lv_obj_get_child(o, i));
}

static void show_page(int idx)
{
    for (int i = 0; i < s_npages; i++) {
        lv_obj_set_hidden(s_pages[i], i != idx);
    }
    s_page_idx = idx;
    mapview_set_visible(idx == PAGE_MAP);
    if (idx >= 0) no_scroll(s_pages[idx]);   /* cheap; catches rebuilt cells */
    data_refresh_cb(NULL);   /* don't wait for the timer */
}

/* Moves dir (+1 / -1) steps around the current page ring. */
static void step_page(int dir)
{
    int pos = 0;
    for (int i = 0; i < s_ring_n; i++) if (s_ring[i] == s_page_idx) pos = i;
    show_page(s_ring[(pos + s_ring_n + dir) % s_ring_n]);
}

/* Swipe left = next page, swipe right = previous page. Gestures bubble up to
 * the screen; the menu, popups and the ride summary live on the top layer,
 * so their swipes never get here. */
static void gesture_cb(lv_event_t *e)
{
    (void)e;
    if (menu_active() || s_page_idx < 0 || s_ring_n < 2) return;
    lv_indev_t *indev = lv_indev_active();
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_LEFT) step_page(1);
    else if (dir == LV_DIR_RIGHT) step_page(-1);
    else return;
    lv_indev_wait_release(indev);   /* no click on the cell / button the swipe started on */
}

/* Pages reachable with KEY_NEXT_PAGE: idle <-> status when idle, the data pages
 * during a ride (status page as a fallback when none is enabled); the map
 * page in both modes when a map file was found. */
static void set_ring(bool keep_page)
{
    s_ring_n = 0;
    if (s_mode == RIDE_IDLE) {
        s_ring[s_ring_n++] = PAGE_IDLE;
        s_ring[s_ring_n++] = PAGE_STATUS;
    } else {
        for (int i = PAGE_FIRST_DATA; i < s_npages; i++) s_ring[s_ring_n++] = i;
        if (!s_ring_n) s_ring[s_ring_n++] = PAGE_STATUS;
    }
    if (mapview_available() && config_get()->map_page.enabled) s_ring[s_ring_n++] = PAGE_MAP;
    int pos = 0;
    if (keep_page) {
        for (int i = 0; i < s_ring_n; i++) if (s_ring[i] == s_page_idx) pos = i;
    }
    show_page(s_ring[pos]);
}

/* Status colours are local styles set at creation: refresh them for the
 * current theme (the themed labels update themselves). */
static void apply_theme_colors(void)
{
    lv_obj_set_style_text_color(s_touch_lbl, theme_palette(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_color(s_ant, theme_palette(LV_PALETTE_PINK), 0);
    if (s_bat_valid) ui_set_battery_bars();
    refresh_bars();
}

/* The configuration may have changed while the menu was open. */
static void on_menu_closed(void)
{
    apply_theme_colors();
    build_data_pages(lv_screen_active());
    rebuild_map();
    set_ring(true);
}

void ui_set_mode(ride_mode_t mode)
{
    ui_lock();
    s_mode = mode;
    set_ring(mode != RIDE_IDLE && s_page_idx >= PAGE_MAP);   /* stay on the page when pausing/resuming */
    ui_unlock();
}

void ui_maps_changed(void)
{
    ui_lock();
    set_ring(true);
    ui_unlock();
}

static void menu_usb_cb(void) { if (s_on_usb) s_on_usb(); }

void ui_set_actions(ui_action_cb_t on_start, ui_action_cb_t on_usb, ui_action_cb_t on_backlight)
{
    s_on_start = on_start;
    s_on_usb = on_usb;
    menu_set_action_cb(MENU_ACTION_USB, menu_usb_cb);
    menu_set_action_cb(MENU_ACTION_POWER_OFF, ui_show_power_popup);
    menu_set_action_cb(MENU_ACTION_BACKLIGHT, on_backlight);
}

/* ---- confirmation popups ------------------------------------------------ */

static void popup_timeout_cb(lv_timer_t *t)
{
    s_popup_timer = NULL;
    ui_hide_popup();
}

static void popup_ok_cb(lv_event_t *e)
{
    ui_action_cb_t cb = s_popup_kind == UI_POPUP_POWER ? s_on_power_off : s_on_end_ride;
    ui_hide_popup();
    if (cb) cb();
}
static void popup_cancel_cb(lv_event_t *e) { ui_hide_popup(); }

void ui_set_power_off_cb(ui_action_cb_t cb) { s_on_power_off = cb; }
void ui_set_end_ride_cb(ui_action_cb_t cb) { s_on_end_ride = cb; }

ui_popup_t ui_popup_active(void) { return s_popup ? s_popup_kind : UI_POPUP_NONE; }

void ui_hide_popup(void)
{
    ui_lock();
    if (s_popup_timer) { lv_timer_delete(s_popup_timer); s_popup_timer = NULL; }
    if (s_popup) { lv_obj_delete(s_popup); s_popup = NULL; }
    ui_unlock();
}

static void show_popup(ui_popup_t kind, const char *title, const char *hint, const char *ok, lv_palette_t color)
{
    ui_lock();
    if (s_popup) ui_hide_popup();
    s_popup_kind = kind;
    s_popup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_popup);
    lv_obj_set_size(s_popup, 208, 132);
    lv_obj_center(s_popup);
    lv_obj_add_style(s_popup, &theme_st_bg, 0);
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_popup, 12, 0);
    lv_obj_set_style_border_color(s_popup, theme_palette(color), 0);
    lv_obj_set_style_border_width(s_popup, 2, 0);
    lv_obj_set_style_pad_all(s_popup, 10, 0);
    lv_obj_t *t = tlabel(s_popup, &ui_text_16, &theme_st_text);
    lv_label_set_text(t, title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_t *h = tlabel(s_popup, &ui_text_12, &theme_st_muted);
    lv_label_set_text(h, hint);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_t *b = lv_obj_create(s_popup);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 88, 44);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, theme_palette(color), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *l = label(b, &ui_text_14, lv_color_white());
    lv_label_set_text(l, ok); lv_obj_center(l);
    lv_obj_add_event_cb(b, popup_ok_cb, LV_EVENT_CLICKED, NULL);
    b = lv_obj_create(s_popup);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 88, 44);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_add_style(b, &theme_st_panel, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    l = tlabel(b, &ui_text_14, &theme_st_text);
    lv_label_set_text(l, "Cancel"); lv_obj_center(l);
    lv_obj_add_event_cb(b, popup_cancel_cb, LV_EVENT_CLICKED, NULL);
    s_popup_timer = lv_timer_create(popup_timeout_cb, 8000, NULL);
    lv_timer_set_repeat_count(s_popup_timer, 1);
    ui_unlock();
}

/* ---- ride summary ------------------------------------------------------ */

static lv_obj_t *s_summary;

static void summary_done_cb(lv_event_t *e) { ui_hide_summary(); }

bool ui_summary_active(void) { return s_summary != NULL; }

void ui_hide_summary(void)
{
    ui_lock();
    if (s_summary) { lv_obj_delete(s_summary); s_summary = NULL; }
    ui_unlock();
}

void ui_show_summary(void)
{
    static const struct { const char *name; field_id_t id; } rows[] = {
        { "Time",        FIELD_SESSION_TIME },
        { "Distance",    FIELD_DISTANCE },
        { "Avg speed",   FIELD_STAT(STAT_SPEED, AGG_AVG) },
        { "Max speed",   FIELD_STAT(STAT_SPEED, AGG_MAX) },
        { "Avg HR",      FIELD_STAT(STAT_HR, AGG_AVG) },
        { "Max HR",      FIELD_STAT(STAT_HR, AGG_MAX) },
        { "Avg cadence", FIELD_STAT(STAT_CADENCE, AGG_AVG) },
        { "Avg power",   FIELD_STAT(STAT_POWER, AGG_AVG) },
        { "Calories",    FIELD_CALORIES },
        { "Max altitude",FIELD_STAT(STAT_ALTITUDE, AGG_MAX) },
        { "Laps",        FIELD_LAPS },
    };
    char buf[24], val[32];
    ui_lock();
    ui_hide_summary();
    s_summary = page(lv_layer_top());

    lv_obj_t *hdr = plain(s_summary);
    lv_obj_set_size(hdr, LCD_H_RES, HDR_H);
    lv_obj_add_style(hdr, &theme_st_line, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_t *t = tlabel(hdr, &ui_text_14, &theme_st_text);
    lv_label_set_text(t, LV_SYMBOL_OK "  Ride summary");
    lv_obj_center(t);

    const int row_h = 21, y0 = HDR_H + 4;
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        int y = y0 + i * row_h;
        lv_obj_t *n = tlabel(s_summary, &lv_font_montserrat_14, &theme_st_muted);
        lv_label_set_text(n, rows[i].name);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 12, y + 3);
        field_value(rows[i].id, buf, sizeof buf);
        const char *unit = field_unit(rows[i].id);
        snprintf(val, sizeof val, "%s%s%s", buf, *unit ? " " : "", unit);
        lv_obj_t *v = tlabel(s_summary, &lv_font_montserrat_20, &theme_st_text);
        lv_label_set_text(v, val);
        lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -12, y);
    }

    big_button(s_summary, "Done", true, 60, LCD_V_RES - 6 - 48, 120, summary_done_cb);
    ui_unlock();
}

/* ---- toast ------------------------------------------------------------- */

static lv_obj_t *s_toast;
static lv_timer_t *s_toast_timer;

static void toast_timeout_cb(lv_timer_t *t)
{
    s_toast_timer = NULL;
    if (s_toast) { lv_obj_delete(s_toast); s_toast = NULL; }
}

void ui_toast(const char *text)
{
    ui_lock();
    if (s_toast_timer) { lv_timer_delete(s_toast_timer); s_toast_timer = NULL; }
    if (s_toast) { lv_obj_delete(s_toast); s_toast = NULL; }
    s_toast = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_toast);
    lv_obj_set_size(s_toast, 180, 60);
    lv_obj_center(s_toast);
    lv_obj_set_style_bg_color(s_toast, theme_colors()->sel, 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_toast, 8, 0);
    lv_obj_set_clickable(s_toast, false);
    lv_obj_t *l = label(s_toast, &lv_font_montserrat_28, theme_colors()->sel_fg);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    s_toast_timer = lv_timer_create(toast_timeout_cb, 1500, NULL);
    lv_timer_set_repeat_count(s_toast_timer, 1);
    ui_unlock();
}

void ui_show_power_popup(void)
{
    show_popup(UI_POPUP_POWER, "Power off?", "key" KEY_STR(KEY_POWER) ": off   other: cancel", "Off", LV_PALETTE_RED);
}

void ui_show_end_ride_popup(void)
{
    show_popup(UI_POPUP_END_RIDE, "End ride?", "key" KEY_STR(KEY_RIDE) ": end   other: cancel", "End", LV_PALETTE_ORANGE);
}

void ui_set_touch(const char *chip_name)
{
    ui_lock();
    lv_label_set_text_fmt(s_touch_lbl, "touch: %s", chip_name);
    ui_unlock();
}

/* Last touch position on the status page. */
static void touch_timer_cb(lv_timer_t *t)
{
    int16_t x, y;
    if (ui_port_touch_state(&x, &y) && !lv_obj_is_hidden(s_page_status)) {
        lv_label_set_text_fmt(s_touch_lbl, "touch: %d,%d", x, y);
    }
}

void ui_create(void)
{
    ui_lock();
    lv_obj_t *scr = lv_screen_active();
    theme_init(config_get()->theme);
    lv_obj_add_style(scr, &theme_st_bg, 0);
    build_idle(scr);
    build_status(scr);
    build_map(scr);
    s_pages[PAGE_IDLE] = s_page_idle;
    s_pages[PAGE_STATUS] = s_page_status;
    s_pages[PAGE_MAP] = s_page_map;
    s_npages = PAGE_FIRST_DATA;
    lv_obj_set_hidden(s_page_status, true);

    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_timer_create(touch_timer_cb, 100, NULL);

    build_data_pages(scr);
    menu_set_close_cb(on_menu_closed);
    set_ring(false);
    lv_timer_create(data_refresh_cb, 500, NULL);
    ui_unlock();
}

/* Battery level in every status bar (also after a theme change). */
static void ui_set_battery_bars(void)
{
    lv_color_t c = s_bat_pct > 20 ? theme_colors()->ok : s_bat_pct > 10 ? theme_colors()->warn : theme_palette(LV_PALETTE_RED);
    for (int i = 0; i < s_npages; i++) {
        hdr_t *h = &s_hdr[i];
        if (!h->bat_lbl) continue;
        lv_label_set_text_fmt(h->bat_lbl, "%u%%", s_bat_pct);
        lv_obj_set_width(h->bat_fill, (12 * s_bat_pct + 50) / 100);
        lv_obj_set_style_bg_color(h->bat_fill, c, 0);
    }
}

void ui_set_battery(uint8_t pct, uint16_t mv)
{
    ui_lock();
    lv_arc_set_value(s_arc, pct);
    lv_label_set_text_fmt(s_arc_lbl, "%u%%\n%u mV", pct, mv);
    s_bat_pct = pct;
    s_bat_valid = true;
    ui_set_battery_bars();
    ui_unlock();
}

void ui_set_env(int16_t temp_c100, uint32_t pressure_pa100)
{
    ui_lock();
    lv_label_set_text_fmt(s_env, "%d.%02d C   %lu.%lu hPa",
                          temp_c100 / 100, abs(temp_c100 % 100),
                          (unsigned long)(pressure_pa100 / 10000),
                          (unsigned long)((pressure_pa100 / 1000) % 10));
    ui_unlock();
}

void ui_set_nrf(bool alive, uint8_t reason, const uint8_t fw[3])
{
    ui_lock();
    if (alive) {
        lv_label_set_text_fmt(s_nrf, "nRF ok  reason %u  fw %u.%u.%u", reason, fw[0], fw[1], fw[2]);
        lv_obj_set_style_text_color(s_nrf, theme_palette(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(s_nrf, "nRF: no reply yet");
    }
    ui_unlock();
}

static void refresh_footer(void)
{
    lv_label_set_text_fmt(s_foot, "int %uK  psram %uK  bl %u%%  up %lus",
                          (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                          (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                          s_bl_pct, (unsigned long)s_uptime);
}

void ui_set_backlight(uint8_t pct)
{
    s_bl_pct = pct;
    ui_lock();
    refresh_footer();
    ui_unlock();
}

static void key_reset_cb(lv_timer_t *t)
{
    lv_obj_t *box = lv_timer_get_user_data(t);
    lv_obj_set_style_bg_color(box, C_IDLE, 0);
    lv_timer_delete(t);
}

void ui_key_event(uint8_t key, uint8_t evt)
{
    ui_lock();
    if (key < NUM_KEYS) {
        lv_obj_t *box = s_key[key];
        if (evt == KEY_EVT_LONG_START) {
            lv_obj_set_style_bg_color(box, C_HOLD, 0);
        } else if (evt == KEY_EVT_CLICK) {
            lv_obj_set_style_bg_color(box, C_CLK, 0);
            lv_timer_create(key_reset_cb, 200, box);
        } else {
            lv_obj_set_style_bg_color(box, C_IDLE, 0);
        }
    }
    /* scroll the log */
    if (s_log_n == LOG_LINES) {
        for (int i = 1; i < LOG_LINES; i++) {
            strcpy(s_log_buf[i - 1], s_log_buf[i]);
        }
        s_log_n--;
    }
    snprintf(s_log_buf[s_log_n++], sizeof s_log_buf[0], "%6lus  key %u  evt %u",
             (unsigned long)s_uptime, key, evt);
    char all[LOG_LINES * 32];
    int n = 0;
    for (int i = 0; i < s_log_n; i++) {
        n += snprintf(all + n, sizeof all - n, "%s%s", i ? "\n" : "", s_log_buf[i]);
    }
    lv_label_set_text(s_log, all);
    ui_unlock();
}

void ui_set_gps(const gps_fix_t *g)
{
    char buf[64];
    mapview_set_position(g->lat, g->lon, g->valid);
    mapview_set_course(g->course_deg, g->valid && g->speed_kmh >= 3.0f);
    ui_lock();
    s_fix = *g;
    if (g->baud == 0) {
        lv_label_set_text(s_gps, "GPS: probing baud");
    } else {
        const char *fix = g->valid ? (g->fix_quality == 2 ? "DGPS" : "fix") : "no fix";
        lv_color_t c = theme_palette(g->valid ? LV_PALETTE_GREEN : LV_PALETTE_ORANGE);
        snprintf(buf, sizeof buf, "GPS %s  %u/%u sats  hdop %.1f", fix, g->sats_used, g->sats_in_view, g->hdop);
        lv_label_set_text(s_gps, buf);
        lv_obj_set_style_text_color(s_gps, c, 0);
    }
    /* status bars: satellites used, ring in the warning colour without a fix */
    for (int i = 0; i < s_npages; i++) {
        hdr_t *h = &s_hdr[i];
        if (!h->sats) continue;
        if (g->baud) lv_label_set_text_fmt(h->sats, "%u", g->sats_used);
        else lv_label_set_text(h->sats, "--");
        if (g->valid) lv_obj_remove_local_style_prop(h->gps_ring, LV_STYLE_BORDER_COLOR, 0);
        else lv_obj_set_style_border_color(h->gps_ring, theme_colors()->warn, 0);
    }
    ui_unlock();
}

/* A channel's values are shown as "--" unless it is live (see ant_live). */
void ui_set_sensors(const ant_sensors_t *v, const ant_channel_t *ch, size_t nch)
{
    char line[96], hr[8], cad[8], spd[12], pwr[8];
    uint32_t now = esp_timer_get_time() / 1000;
    bool have_hr  = ant_live(ANT_DEV_HR, ANT_DEV_HR) && v->hr_bpm;
    bool have_cad = ant_live(ANT_DEV_CADENCE, ANT_DEV_SPD_CAD);
    bool have_spd = ant_live(ANT_DEV_SPEED, ANT_DEV_SPD_CAD);
    bool have_pwr = ant_live(ANT_DEV_POWER, ANT_DEV_POWER);

    snprintf(hr, sizeof hr, have_hr ? "%u" : "--", v->hr_bpm);
    snprintf(cad, sizeof cad, have_cad ? "%.0f" : "--", v->cadence_rpm);
    snprintf(spd, sizeof spd, have_spd ? "%.1f" : "--.-", v->speed_kmh);
    int n = snprintf(line, sizeof line, "HR %s  cad %s  %s km/h", hr, cad, spd);
    if (have_pwr) {
        snprintf(pwr, sizeof pwr, "%uW", v->power_w);
        snprintf(line + n, sizeof line - n, "  %s", pwr);
    }

    ui_lock();
    s_ant_ch = ch;
    s_ant_n = nch;

    n = snprintf(line, sizeof line, "ANT");
    if (nch == 0) {
        n += snprintf(line + n, sizeof line - n, ": no paired sensors");
    }
    for (size_t i = 0; i < nch && n < (int)sizeof line - 12; i++) {
        const char *name = ch[i].dev_type == ANT_DEV_HR ? "HR" : ch[i].dev_type == ANT_DEV_CADENCE ? "cad"
                         : ch[i].dev_type == ANT_DEV_SPEED ? "spd" : ch[i].dev_type == ANT_DEV_POWER ? "pwr"
                         : ant_dev_name(ch[i].dev_type);
        bool live = ch[i].state == ANT_ST_CONNECTED && ch[i].pages && now - ch[i].last_rx_ms < ANT_LIVE_MS;
        const char *st = live ? "ok" : ch[i].state == ANT_ST_SEARCHING ? ".." : "--";
        n += snprintf(line + n, sizeof line - n, "  %s %s", name, st);
    }
    lv_label_set_text(s_ant, line);
    ui_unlock();
}

void ui_set_sd(bool mounted, const char *name, uint32_t size_mb)
{
    ui_lock();
    if (mounted) {
        lv_label_set_text_fmt(s_sd, "SD %s  %lu.%lu GB", name, (unsigned long)(size_mb / 1024),
                              (unsigned long)((size_mb % 1024) * 10 / 1024));
        lv_obj_set_style_text_color(s_sd, theme_palette(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(s_sd, "SD: not mounted");
    }
    ui_unlock();
}

static void usb_reboot_cb(lv_event_t *e) { if (s_on_usb_reboot) s_on_usb_reboot(); }

void ui_set_usb_reboot_cb(ui_action_cb_t cb) { s_on_usb_reboot = cb; }

void ui_show_usb_mode(void)
{
    ui_lock();
    show_page(-1);
    lv_obj_t *p = page(lv_screen_active());
    lv_obj_t *icon = label(p, &lv_font_montserrat_48, lv_palette_main(LV_PALETTE_BLUE));
    lv_label_set_text(icon, LV_SYMBOL_USB);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_t *t = tlabel(p, &lv_font_montserrat_20, &theme_st_text);
    lv_label_set_text(t, "USB storage");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_t *h = tlabel(p, &lv_font_montserrat_14, &theme_st_muted);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(h, "eMMC is exposed to the host.\nEject it, then reboot.");
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_t *b = button(p, LV_SYMBOL_REFRESH "  Reboot", lv_palette_main(LV_PALETTE_BLUE), 160, 44);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 236);
    lv_obj_add_event_cb(b, usb_reboot_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *k = tlabel(p, &lv_font_montserrat_14, &theme_st_muted);
    lv_label_set_text(k, "or hold key " KEY_STR(KEY_USB_EXIT));
    lv_obj_align(k, LV_ALIGN_TOP_MID, 0, 290);
    ui_unlock();
}

void ui_next_page(void)
{
    ui_lock();
    step_page(1);
    ui_unlock();
}

void ui_prev_page(void)
{
    ui_lock();
    step_page(-1);
    ui_unlock();
}

void ui_open_menu(void)
{
    ui_lock();
    menu_open();
    ui_unlock();
}

bool ui_menu_active(void)
{
    return menu_active();
}

bool ui_menu_key(uint8_t key, uint8_t evt)
{
    ui_lock();
    bool used = menu_key(key, evt);
    ui_unlock();
    return used;
}

void ui_tick(uint32_t uptime_s)
{
    s_uptime = uptime_s;
    ui_lock();
    refresh_footer();
    ui_unlock();
}
