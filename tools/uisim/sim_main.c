/* tools/uisim: renders the firmware's screens to images on a desktop.
 *
 *   build/uisim OUTDIR [light|dark]
 *
 * Builds the real UI (main/ui.c and friends) on a 240x320 RGB565 display in
 * memory, with the fixed ride of sim_data.c, and writes one PPM per screen:
 * home, home_searching, status, ride_1.., map, ride_paused. The map is drawn
 * by the real renderer from SD_MOUNT_POINT/MAP/*.map (see Makefile).
 * tools/uisim/run.sh converts them to PNG. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "lvgl.h"
#include "sim.h"
#include "board.h"
#include "config.h"
#include "stats.h"
#include "sun.h"
#include "gps.h"
#include "theme.h"
#include "mapview.h"
#include "ui.h"
#include "fields.h"

static uint16_t s_fb[LCD_H_RES * LCD_V_RES];
static uint8_t s_buf[LCD_H_RES * LCD_V_RES * 2];
static const char *s_out;

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int32_t w = lv_area_get_width(a);
    for (int32_t y = a->y1; y <= a->y2; y++) {
        memcpy(&s_fb[y * LCD_H_RES + a->x1], px + (size_t)(y - a->y1) * w * 2, (size_t)w * 2);
    }
    lv_display_flush_ready(d);
}

static uint32_t tick_cb(void) { return uisim_ms(); }
static bool s_riding;   /* feed 25 km/h once a second, as the GPS would */

/* Lets timers (2 Hz data refresh, map refresh) and the map render task run. */
static void settle(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 50) {
        uisim_advance_ms(50);
        if (s_riding && t % 1000 == 0) {
            stats_update(STAT_SPEED, 25.0f);
            stats_update(STAT_ALTITUDE, 51.0f);
        }
        lv_timer_handler();
        uisim_run_tasks();
    }
    lv_refr_now(NULL);
}

static void save(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", s_out, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", LCD_H_RES, LCD_V_RES);
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        uint16_t v = s_fb[i];
        uint8_t rgb[3] = { (uint8_t)((v >> 11) * 255 / 31), (uint8_t)(((v >> 5) & 0x3f) * 255 / 63), (uint8_t)((v & 0x1f) * 255 / 31) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", path);
}

static void push_gps(void)
{
    gps_fix_t g;
    gps_get(&g);
    ui_set_gps(&g);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: uisim OUTDIR [light|dark]\n"); return 2; }
    s_out = argv[1];
    mkdir(s_out, 0755);
    bool dark = argc > 2 && !strcmp(argv[2], "dark");

    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, s_buf, NULL, sizeof s_buf, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);

    config_load();
    app_cfg_t *c = config_get();
    c->tz_min = 7 * 60;                                /* WIB */
    c->theme = dark ? THEME_DARK : THEME_LIGHT;
    c->theme_auto = 0;
    strcpy(c->route, "test_loop.gpx");
    stats_init();
    fields_init();
    sun_init();
    uisim_data_init();

    gps_fix_t g;
    gps_get(&g);
    sun_set_position(g.lat, g.lon);
    mapview_init();
    ui_create();
    ui_set_battery(100, 4300);
    ui_set_sd(true, "MT3204", 3650);
    push_gps();
    settle(1200);
    save("home");

    uisim_set_gps_searching(true);
    push_gps();
    settle(600);
    save("home_searching");
    uisim_set_gps_searching(false);
    push_gps();

    ui_next_page();
    settle(600);
    save("status");

    /* a ride: 91 s at 25 km/h */
    stats_reset();
    uisim_set_ride_mode(RIDE_RIDING);
    ui_set_mode(RIDE_RIDING);
    s_riding = true;
    for (int s = 0; s < 91; s++) {
        stats_update(STAT_SPEED, 25.0f);
        stats_update(STAT_ALTITUDE, 51.0f);
        uisim_advance_ms(1000);
    }
    push_gps();
    mapview_set_position(g.lat, g.lon, true);
    /* the default ring during a ride: data pages 1-3, then the map */
    char name[32];
    for (int i = 1; i <= 4; i++) {
        settle(1200);
        if (i == 4) strcpy(name, "map");
        else snprintf(name, sizeof name, "ride_%d", i);
        save(name);
        if (i < 4) ui_next_page();
    }

    uisim_set_ride_mode(RIDE_PAUSED);
    ui_set_mode(RIDE_PAUSED);
    s_riding = false;
    settle(1200);
    save("ride_paused");
    return 0;
}
