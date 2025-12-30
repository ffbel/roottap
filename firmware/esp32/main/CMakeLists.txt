idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES button led button_ble button_gpio nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Wpointer-arith
    -Wcast-align
    -Wwrite-strings
    -Wmissing-prototypes
    -Wstrict-prototypes
    -Werror=implicit-function-declaration
)
