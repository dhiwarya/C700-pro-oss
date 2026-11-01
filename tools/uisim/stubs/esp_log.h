#pragma once
/* Desktop stand-in for ESP-IDF's esp_log.h (tools/uisim): I/W/E to stderr, D/V dropped. */
#include <stdio.h>
#define ESP_LOGI(t, f, ...) fprintf(stderr, "I %s: " f "\n", t, ##__VA_ARGS__)
#define ESP_LOGW(t, f, ...) fprintf(stderr, "W %s: " f "\n", t, ##__VA_ARGS__)
#define ESP_LOGE(t, f, ...) fprintf(stderr, "E %s: " f "\n", t, ##__VA_ARGS__)
#define ESP_LOGD(t, f, ...) do { } while (0)
#define ESP_LOGV(t, f, ...) do { } while (0)
#define ESP_LOG_BUFFER_HEX_LEVEL(t, b, n, l) do { } while (0)
#define ESP_RETURN_ON_ERROR(x, t, m) do { esp_err_t _e = (x); if (_e != ESP_OK) return _e; } while (0)
