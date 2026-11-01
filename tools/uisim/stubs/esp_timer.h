#pragma once
/* Desktop stand-in (tools/uisim): virtual microseconds driven by the renderer. */
#include <stdint.h>
int64_t esp_timer_get_time(void);
