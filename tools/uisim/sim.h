#pragma once
/* tools/uisim: desktop renderer for the firmware UI. */
#include <stdint.h>
#include <stdbool.h>
#include "ride.h"

void uisim_advance_ms(uint32_t ms);
uint32_t uisim_ms(void);
void uisim_run_tasks(void);

/* sim_data.c: the fake ride shown on every screen (the 2.4 km test loop,
 * 91 s in, 630 m ridden, heading south at 25 km/h, 16:14 WIB) */
void uisim_data_init(void);
void uisim_set_ride_mode(ride_mode_t m);
void uisim_set_gps_searching(bool searching);
