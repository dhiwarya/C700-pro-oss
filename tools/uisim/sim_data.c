/* tools/uisim: stand-ins for the hardware-facing modules, holding one fixed
 * ride so every screen shows consistent values:
 *   - 2026-11-01 16:14 WIB, around the Gelora Bung Karno stadium (Jakarta)
 *   - route: the 2.4 km square test loop, 91 s in, 630 m ridden, heading
 *     south at 25 km/h, 38/42 satellites, battery 100 %
 *   - no ANT+ sensors paired, no shifting
 * The real modules compiled alongside: ui, datapage, theme, layouts, fields,
 * config, stats, sun, mapview, mapfile (see Makefile). */
#define _DEFAULT_SOURCE
#include <math.h>
#include <string.h>
#include <time.h>
#include "sim.h"
#include "gps.h"
#include "ant.h"
#include "ride.h"
#include "trip.h"
#include "health.h"
#include "shifting.h"
#include "route.h"
#include "trail.h"
#include "menu.h"
#include "ui_port.h"
#include "utc.h"
#include "mapfile.h"
#include "sdcard.h"

/* ---- the fixed ride ---------------------------------------------------- */

#define LOOP_LAT0  (-6.2160)    /* NW corner of the loop around the main stadium */
#define LOOP_LON0  (106.7990)
#define LEG_M      600.0
#define STEP_M     50.0
#define RIDDEN_M   630.0

static ride_mode_t s_mode = RIDE_IDLE;
static bool s_searching;
static uint32_t s_t0_ms;           /* virtual ms at "16:14:00" */
static int32_t s_route_x[64], s_route_y[64], s_trail_x[64], s_trail_y[64];
static size_t s_route_n, s_trail_n;
static double s_pos_lat, s_pos_lon;

static void step(double *lat, double *lon, double course, double m)
{
    *lat += m * cos(course * M_PI / 180.0) / 111320.0;
    *lon += m * sin(course * M_PI / 180.0) / (111320.0 * cos(*lat * M_PI / 180.0));
}

void uisim_data_init(void)
{
    static const double courses[] = { 90, 180, 270, 0 };
    double lat = LOOP_LAT0, lon = LOOP_LON0, done = 0;
    s_route_n = s_trail_n = 0;
    s_route_x[s_route_n] = s_trail_x[s_trail_n] = (int32_t)mapfile_lon_to_px(lon, 20);
    s_route_y[s_route_n++] = s_trail_y[s_trail_n++] = (int32_t)mapfile_lat_to_py(lat, 20);
    for (int leg = 0; leg < 4; leg++) {
        for (int i = 0; i < (int)(LEG_M / STEP_M); i++) {
            double plat = lat, plon = lon;
            step(&lat, &lon, courses[leg], STEP_M);
            s_route_x[s_route_n] = (int32_t)mapfile_lon_to_px(lon, 20);
            s_route_y[s_route_n++] = (int32_t)mapfile_lat_to_py(lat, 20);
            if (done < RIDDEN_M) {
                double m = RIDDEN_M - done < STEP_M ? RIDDEN_M - done : STEP_M;
                if (m < STEP_M) { lat = plat; lon = plon; step(&lat, &lon, courses[leg], m); }
                s_trail_x[s_trail_n] = (int32_t)mapfile_lon_to_px(lon, 20);
                s_trail_y[s_trail_n++] = (int32_t)mapfile_lat_to_py(lat, 20);
                if (m < STEP_M) { s_pos_lat = lat; s_pos_lon = lon; lat = plat; lon = plon; step(&lat, &lon, courses[leg], STEP_M); }
                done += m;
            }
        }
    }
    s_t0_ms = uisim_ms();
}

void uisim_set_ride_mode(ride_mode_t m) { s_mode = m; }
void uisim_set_gps_searching(bool searching) { s_searching = searching; }

/* ---- time --------------------------------------------------------------- */

bool utc_now(uint32_t *unix_s)
{
    struct tm t = { .tm_year = 2026 - 1900, .tm_mon = 10, .tm_mday = 1, .tm_hour = 9, .tm_min = 14 };
    *unix_s = (uint32_t)timegm(&t) + (uisim_ms() - s_t0_ms) / 1000;
    return true;
}

uint32_t utc_from_civil(uint16_t year, uint8_t mon, uint8_t day, uint8_t hh, uint8_t mm, uint8_t ss)
{
    struct tm t = { .tm_year = year - 1900, .tm_mon = mon - 1, .tm_mday = day, .tm_hour = hh, .tm_min = mm, .tm_sec = ss };
    return (uint32_t)timegm(&t);
}

void utc_set_rtc(uint8_t ss, uint8_t mm, uint8_t hh, uint8_t dd, uint8_t mon0, uint8_t yy1900)
{
    (void)ss; (void)mm; (void)hh; (void)dd; (void)mon0; (void)yy1900;
}

/* ---- GPS ---------------------------------------------------------------- */

void gps_get(gps_fix_t *out)
{
    memset(out, 0, sizeof *out);
    out->baud = 921600;
    out->sentences = 4000;
    out->last_rx_ms = uisim_ms();
    out->hh = 9; out->mm = 14; out->day = 1; out->mon = 11; out->year = 26;
    if (s_searching) {
        out->sats_used = 6;
        out->sats_in_view = 9;
        return;
    }
    out->valid = true;
    out->fix_quality = 1;
    out->sats_used = 38;
    out->sats_in_view = 42;
    out->hdop = 0.4f;
    out->lat = s_pos_lat;
    out->lon = s_pos_lon;
    out->alt_m = 51;
    out->speed_kmh = s_mode == RIDE_RIDING ? 25.0f : 0.0f;
    out->course_deg = 180;
}

bool gps_inject(const char *s) { (void)s; return false; }
void gps_simulate(bool on) { (void)on; }

/* ---- ANT+ (nothing paired) ---------------------------------------------- */

const ant_channel_t *ant_channels(size_t *count) { *count = 0; return NULL; }
bool ant_live(uint8_t a, uint8_t b) { (void)a; (void)b; return false; }
void ant_get(ant_sensors_t *out) { memset(out, 0, sizeof *out); }
const char *ant_dev_name(uint8_t t) { (void)t; return "sensor"; }
float ant_wheel_m(void) { return 2.105f; }

/* ---- ride, trip, health, shifting --------------------------------------- */

ride_mode_t ride_mode(void) { return s_mode; }
bool ride_recording(void) { return s_mode == RIDE_RIDING; }
bool ride_auto_paused(void) { return false; }

float trip_distance_m(void) { return s_mode == RIDE_IDLE ? 0 : RIDDEN_M; }
uint32_t trip_laps(void) { return 0; }
float trip_lap_distance_m(void) { return trip_distance_m(); }
uint32_t trip_lap_time_ms(void) { return 91000; }
float trip_lap_avg_kmh(void) { return 25.0f; }
uint32_t trip_prev_lap_time_ms(void) { return 0; }
float trip_prev_lap_distance_m(void) { return 0; }
bool trip_prev_lap_manual(void) { return false; }

int health_age(void) { return 0; }
int health_max_hr(void) { return 180; }
int health_lthr(void) { return 160; }
float health_kcal(void) { return s_mode == RIDE_IDLE ? 0 : 21.0f; }
float health_kcal_per_h(void) { return 830.0f; }
const char *health_kcal_source(void) { return "speed"; }
int health_hr_zone(int bpm) { (void)bpm; return 0; }
int health_pwr_zone(int w) { (void)w; return 0; }
bool health_pwr_zones_available(void) { return false; }
uint32_t health_hr_zone_ms(int z) { (void)z; return 0; }
uint32_t health_pwr_zone_ms(int z) { (void)z; return 0; }
int health_hr_zone_current(void) { return -1; }
int health_pwr_zone_current(void) { return -1; }
uint32_t health_zone_since_ms(void) { return 0; }

void shifting_get(shifting_t *out) { memset(out, 0, sizeof *out); }
bool shifting_live(void) { return false; }
const char *shifting_batt_text(void) { return "--"; }

/* ---- route and trail ---------------------------------------------------- */

bool route_loaded(void) { return true; }
const char *route_name(void) { return "test_loop.gpx"; }
float route_length_m(void) { return 2400.0f; }
uint32_t route_generation(void) { return 1; }
void route_lock(void) {}
void route_unlock(void) {}
size_t route_points(const int32_t **x20, const int32_t **y20) { *x20 = s_route_x; *y20 = s_route_y; return s_route_n; }
void route_track(double lat, double lon) { (void)lat; (void)lon; }
bool route_remaining(float *remaining_m, float *off_track_m)
{
    *remaining_m = 2400.0f - (float)trip_distance_m();
    if (off_track_m) *off_track_m = 0;
    return true;
}
bool route_is_fit(const char *name) { (void)name; return false; }

uint32_t trail_generation(void) { return 1; }
void trail_lock(void) {}
void trail_unlock(void) {}
size_t trail_points(const int32_t **x20, const int32_t **y20)
{
    *x20 = s_trail_x; *y20 = s_trail_y;
    return s_mode == RIDE_IDLE ? 0 : s_trail_n;
}

/* ---- menu, port, storage ------------------------------------------------ */

void menu_open(void) {}
void menu_close(void) {}
bool menu_active(void) { return false; }
bool menu_key(uint8_t key, uint8_t evt) { (void)key; (void)evt; return false; }
void menu_set_close_cb(void (*cb)(void)) { (void)cb; }
void menu_set_action_cb(menu_action_t a, void (*cb)(void)) { (void)a; (void)cb; }

void ui_lock(void) {}
void ui_unlock(void) {}
bool ui_port_touch_state(int16_t *x, int16_t *y) { (void)x; (void)y; return false; }
