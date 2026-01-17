idf_component_register(
    SRCS "user_presence.c"
    INCLUDE_DIRS "include"
    REQUIRES button button_ble
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
