idf_component_register(
    SRCS "ctaphid.c"
    INCLUDE_DIRS "include" "../../core/include"
    REQUIRES log esp_timer esp_system nvs_flash
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
