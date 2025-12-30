idf_component_register(
    SRCS "button_gpio.c"
    INCLUDE_DIRS "include"
    REQUIRES button esp_timer driver
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
