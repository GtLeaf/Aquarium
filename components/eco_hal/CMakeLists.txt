idf_component_register(
    SRCS "hal_i2c.c" "hal_display.c" "hal_touch.c" "hal_lvgl.c" "hal_rtc.c" "hal_pmu.c" "hal_imu.c" "hal_audio.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd esp_timer lvgl esp_driver_ledc esp_lcd_sh8601
    PRIV_REQUIRES esp_psram
)
