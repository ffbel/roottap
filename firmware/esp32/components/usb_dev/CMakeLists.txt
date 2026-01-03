idf_component_register(
    SRCS "usb_cdc_cmd.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_tinyusb driver
)
