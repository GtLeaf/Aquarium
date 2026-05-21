idf_component_register(
    SRCS "save_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_partition utils
)
