idf_component_register(
    SRCS "ui_main.c" "ui_screens.c" "ui_popup.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl eco_hal utils
    PRIV_REQUIRES engine
)
