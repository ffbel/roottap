idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES button led button_ble nvs_flash
)
