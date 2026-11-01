idf_component_register(
    SRCS "main.c" "lcd.c" "backlight.c" "nrf_link.c" "ui_port.c" "ui.c" "gps.c" "sdcard.c" "tracklog.c" "fit.c" "utc.c" "usb_msc.c" "touch.c" "ant.c" "sensor_list.c" "stats.c" "fields.c" "layouts.c" "config.c" "datapage.c" "menu.c" "devcon.c" "theme.c" "ride.c" "trip.c" "mapfile.c" "mapview.c" "route.c" "trail.c" "sun.c" "fitread.c" "history.c" "health.c" "shifting.c" "dbglog.c"
         "fonts/ui_label_10.c" "fonts/ui_num_18.c" "fonts/ui_num_24.c" "fonts/ui_num_34.c" "fonts/ui_num_48.c" "fonts/ui_num_68.c" "fonts/ui_num_80.c" "fonts/ui_text_12.c" "fonts/ui_text_14.c" "fonts/ui_text_16.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_driver_gpio esp_driver_ledc esp_driver_uart esp_timer esp_app_format fatfs sdmmc esp_driver_sdmmc vfs hal esp_driver_i2c json nvs_flash esp_driver_usb_serial_jtag
)

# The map reader and rasteriser are the hot loops of a map render: -O2 there
# (the rest of the firmware stays -Os).
set_source_files_properties(mapfile.c mapview.c PROPERTIES COMPILE_OPTIONS "-O2")
