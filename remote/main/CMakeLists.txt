set(srcs
    "remote_main.c"
    "remote_oled.c"
    "remote_buttons.c"
    "remote_rtc.c"
    "remote_ble.c"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES driver nvs_flash bt esp_timer esp_hw_support
)
