#include <ctype.h>
#include <string.h>
#include "datapage.h"
#include "fields.h"
#include "theme.h"
#include "route.h"
#include "fonts/ui_fonts.h"

#define C_HL (theme_colors()->sel)

#define PAD_X     10      /* caption and value inset */
#define PAD_X_NARROW 7    /* in cells under NARROW_W */
#define NARROW_W  100
#define INLINE_MIN_W 160  /* value on the caption line only in wide cells */
#define PAD_TOP   6
#define CAP_GAP   2       /* caption bottom to digit tops */
#define PAD_BOT   6
#define BAR_H     4       /* route progress bar */
#define UNIT_GAP  4

/* value fonts, largest first */
static const lv_font_t *const k_num_fonts[] = { &ui_num_80, &ui_num_68, &ui_num_48, &ui_num_34, &ui_num_24, &ui_num_18 };
#define N_NUM_FONTS (int)(sizeof k_num_fonts / sizeof k_num_fonts[0])

#define CAP_FONT  (&ui_label_10)
#define UNIT_FONT (&ui_text_12)

/* Distance from a label's top to its text baseline. */
static int baseline_from_top(const lv_font_t *f) { return f->line_height - f->base_line; }

/* Digit metrics: height of '0' and the gap above it inside a label. */
static void digit_box(const lv_font_t *f, int *height, int *top_gap)
{
    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(f, &g, '0', 0)) { *height = f->line_height; *top_gap = 0; return; }
    int top = g.box_h + g.ofs_y;                 /* glyph top above the baseline */
    *height = top;
    *top_gap = baseline_from_top(f) - top;
}

static int text_w(const char *s, const lv_font_t *f)
{
    lv_point_t sz;
    lv_text_get_size(&sz, s, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

static void cell_click_cb(lv_event_t *e)
{
    datapage_t *dp = lv_event_get_user_data(e);
    lv_obj_t *box = lv_event_get_target(e);
    for (int i = 0; i < dp->ncells; i++) {
        if (dp->cell[i].box == box && dp->click_cb) {
            dp->click_cb(dp, i, dp->click_ctx);
            return;
        }
    }
}

void datapage_delete(datapage_t *dp)
{
    if (dp->cont) lv_obj_delete(dp->cont);
    memset(dp, 0, sizeof *dp);
    dp->hl = -1;
}

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_clickable(o, false);
    return o;
}

void datapage_build(datapage_t *dp, lv_obj_t *parent, const page_cfg_t *cfg, int x, int y, int w, int h)
{
    datapage_click_cb_t cb = dp->click_cb;
    void *ctx = dp->click_ctx;
    datapage_delete(dp);
    dp->click_cb = cb;
    dp->click_ctx = ctx;
    dp->cfg = cfg;
    dp->w = w;
    dp->h = h;

    const layout_t *l = layout_get(cfg->layout);
    dp->ncells = layout_cells(l);

    dp->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(dp->cont);
    lv_obj_set_pos(dp->cont, x, y);
    lv_obj_set_size(dp->cont, w, h);

    /* map layouts: the cells form a strip at the bottom, the map canvas
     * (mapview.c) covers the rest; a placeholder marks that area in the
     * menu previews */
    int strip_h = layout_strip_h(l);
    int cells_y = l->map ? h - strip_h : 0;
    int cells_h = l->map ? strip_h : h;
    if (l->map) {
        lv_obj_t *ph = plain(dp->cont);
        lv_obj_set_pos(ph, 0, 0);
        lv_obj_set_size(ph, w, cells_y);
        lv_obj_add_style(ph, &theme_st_panel, 0);
        lv_obj_set_style_bg_opa(ph, LV_OPA_COVER, 0);
        lv_obj_t *t = lv_label_create(ph);
        lv_obj_set_style_text_font(t, &ui_text_16, 0);
        lv_obj_add_style(t, &theme_st_muted, 0);
        lv_label_set_text(t, "Map");
        lv_obj_center(t);
    }

    for (int i = 0; i < dp->ncells; i++) {
        int cx, cy, cw, ch;
        layout_cell_rect(l, i, w, cells_h, &cx, &cy, &cw, &ch);
        dp->cell[i].w = cw;
        dp->cell[i].h = ch;
        dp->cell[i].inline_val = ch < DATAPAGE_INLINE_H && cw >= INLINE_MIN_W;
        int px = cw < NARROW_W ? PAD_X_NARROW : PAD_X;

        /* dividers: right unless last in the row, bottom unless last row,
         * and a top line where a map strip meets the map */
        lv_border_side_t sides = LV_BORDER_SIDE_NONE;
        if (cx + cw < w) sides |= LV_BORDER_SIDE_RIGHT;
        if (cy + ch < cells_h) sides |= LV_BORDER_SIDE_BOTTOM;
        if (l->map && cy == 0) sides |= LV_BORDER_SIDE_TOP;
        dp->cell[i].sides = sides;

        lv_obj_t *box = lv_obj_create(dp->cont);
        lv_obj_remove_style_all(box);
        lv_obj_set_pos(box, cx, cy + cells_y);
        lv_obj_set_size(box, cw, ch);
        lv_obj_add_style(box, &theme_st_line, 0);
        lv_obj_add_style(box, &theme_st_bg, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box, sides ? 1 : 0, 0);
        lv_obj_set_style_border_side(box, sides, 0);
        lv_obj_add_event_cb(box, cell_click_cb, LV_EVENT_CLICKED, dp);
        dp->cell[i].box = box;

        field_id_t f = cfg->field[i];
        char cap[24];
        const char *name = f == FIELD_NONE ? "--" : field_name(f);
        size_t k = 0;
        for (; name[k] && k < sizeof cap - 1; k++) cap[k] = (char)toupper((unsigned char)name[k]);
        cap[k] = 0;
        lv_obj_t *n = lv_label_create(box);
        lv_obj_set_style_text_font(n, CAP_FONT, 0);
        lv_obj_set_style_text_letter_space(n, cw < NARROW_W ? 0 : 1, 0);
        lv_obj_add_style(n, &theme_st_muted, 0);
        if (f == FIELD_NONE) lv_obj_set_style_text_opa(n, LV_OPA_50, 0);
        lv_label_set_text(n, cap);
        lv_label_set_long_mode(n, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(n, cw - 2 * px + 2);
        lv_obj_set_pos(n, px, PAD_TOP);
        dp->cell[i].name = n;

        lv_obj_t *v = lv_label_create(box);
        lv_obj_add_style(v, &theme_st_text, 0);
        lv_label_set_text(v, "");
        dp->cell[i].val = v;

        lv_obj_t *u = lv_label_create(box);
        lv_obj_set_style_text_font(u, UNIT_FONT, 0);
        lv_obj_add_style(u, &theme_st_muted, 0);
        lv_label_set_text(u, field_unit(f));
        dp->cell[i].unit = u;

        if (f == FIELD_ROUTE_LEFT) {
            lv_obj_t *bar = plain(box);
            lv_obj_set_size(bar, cw - 2 * px, BAR_H);
            lv_obj_set_pos(bar, px, ch - PAD_BOT - BAR_H);
            lv_obj_set_style_radius(bar, BAR_H / 2, 0);
            lv_obj_set_style_clip_corner(bar, true, 0);
            lv_obj_set_style_bg_color(bar, theme_colors()->line, 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_t *fill = plain(bar);
            lv_obj_set_size(fill, 0, BAR_H);
            lv_obj_set_style_bg_color(fill, theme_colors()->route, 0);
            lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
            dp->cell[i].bar = bar;
            dp->cell[i].bar_fill = fill;
            dp->cell[i].bar_pct = -1;
        }

        dp->cell[i].last[0] = 0;
        dp->cell[i].font = NULL;
    }
    datapage_refresh(dp);
}

/* Places value and unit for the text in buf, choosing the largest number
 * font that fits the cell. */
static void layout_value(datapage_t *dp, int i, const char *buf, const char *unit)
{
    int cw = dp->cell[i].w, ch = dp->cell[i].h;
    const int PX = cw < NARROW_W ? PAD_X_NARROW : PAD_X;
    int unit_w = *unit ? text_w(unit, UNIT_FONT) + UNIT_GAP : 0;
    int avail_w = cw - 2 * PX - unit_w;
    int bar = dp->cell[i].bar ? BAR_H + 4 : 0;
    int cap_h = CAP_FONT->line_height;
    bool inl = dp->cell[i].inline_val;
    /* room for the digits */
    int avail_h = inl ? ch - PAD_TOP - PAD_BOT - bar
                      : ch - PAD_TOP - cap_h - CAP_GAP - PAD_BOT - bar;
    if (inl) avail_w -= text_w(lv_label_get_text(dp->cell[i].name), CAP_FONT) + 8;

    const lv_font_t *font = k_num_fonts[N_NUM_FONTS - 1];
    int dh = 0, gap = 0;
    for (int k = 0; k < N_NUM_FONTS; k++) {
        digit_box(k_num_fonts[k], &dh, &gap);
        if (dh <= avail_h && text_w(buf, k_num_fonts[k]) <= avail_w) { font = k_num_fonts[k]; break; }
    }
    digit_box(font, &dh, &gap);
    if (font != dp->cell[i].font) {
        dp->cell[i].font = font;
        lv_obj_set_style_text_font(dp->cell[i].val, font, 0);
    }
    lv_label_set_text(dp->cell[i].val, buf);

    int vw = text_w(buf, font);
    int digit_top = inl ? PAD_TOP : PAD_TOP + cap_h + CAP_GAP;
    int base = digit_top + dh;                   /* shared baseline of value and unit */
    int vx = inl ? cw - PX - unit_w - vw : PX;
    lv_obj_set_pos(dp->cell[i].val, vx, base - baseline_from_top(font));
    lv_obj_set_pos(dp->cell[i].unit, vx + vw + UNIT_GAP, base - baseline_from_top(UNIT_FONT));
    if (inl) {   /* caption on the same baseline */
        lv_obj_set_pos(dp->cell[i].name, PX, base - baseline_from_top(CAP_FONT));
    }
}

void datapage_refresh(datapage_t *dp)
{
    char buf[24];
    for (int i = 0; i < dp->ncells; i++) {
        field_id_t f = dp->cfg->field[i];
        field_value(f, buf, sizeof buf);
        if (dp->cell[i].bar) {
            float left = 0, off = 0, len = route_length_m();
            int pct = route_loaded() && len > 0 && route_remaining(&left, &off) ? (int)(100.0f * (1.0f - left / len) + 0.5f) : 0;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            if (pct != dp->cell[i].bar_pct) {
                dp->cell[i].bar_pct = pct;
                lv_obj_set_width(dp->cell[i].bar_fill, lv_obj_get_style_width(dp->cell[i].bar, 0) * pct / 100);
            }
        }
        if (dp->cell[i].font && strcmp(buf, dp->cell[i].last) == 0) continue;
        strcpy(dp->cell[i].last, buf);
        layout_value(dp, i, buf, field_unit(f));
    }
}

void datapage_highlight(datapage_t *dp, int cell)
{
    for (int i = 0; i < dp->ncells; i++) {
        bool on = i == cell;
        lv_obj_t *box = dp->cell[i].box;
        if (on) {
            lv_obj_set_style_border_width(box, 3, 0);
            lv_obj_set_style_border_side(box, LV_BORDER_SIDE_FULL, 0);
            lv_obj_set_style_border_color(box, C_HL, 0);
        } else {
            lv_obj_set_style_border_width(box, dp->cell[i].sides ? 1 : 0, 0);
            lv_obj_set_style_border_side(box, dp->cell[i].sides, 0);
            lv_obj_remove_local_style_prop(box, LV_STYLE_BORDER_COLOR, 0);
        }
    }
    dp->hl = cell;
}

void datapage_set_click_cb(datapage_t *dp, datapage_click_cb_t cb, void *ctx)
{
    dp->click_cb = cb;
    dp->click_ctx = ctx;
}
