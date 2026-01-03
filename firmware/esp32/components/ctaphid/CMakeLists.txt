idf_component_register(
    SRCS "ctaphid.c"
    INCLUDE_DIRS "include" "../../core/include"
    REQUIRES log
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
