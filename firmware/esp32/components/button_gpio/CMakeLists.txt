idf_component_register(
    SRCS "button_gpio.c"
    INCLUDE_DIRS "."
    REQUIRES button esp_timer driver
)
