idf_component_register(
    SRCS "engine_main.c" "engine_logic.c" "event_system.c" "achievement_system.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos utils save eco_hal
)
