idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES eco_hal ui engine save utils
    PRIV_REQUIRES spi_flash nvs_flash
)
