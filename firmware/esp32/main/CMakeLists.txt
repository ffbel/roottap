idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS 
        "."
        "../core/include"
    REQUIRES button led button_ble button_gpio nvs_flash
)

target_link_libraries(${COMPONENT_LIB}
    ${CMAKE_SOURCE_DIR}/core/rust/target/xtensa-esp32s3-espidf/release/libcore.a
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
