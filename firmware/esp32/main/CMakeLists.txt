idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS 
        "."
        "../core/include"
    REQUIRES button led button_ble button_gpio nvs_flash ctaphid usb_hid
)

set(RUST_DIR "${CMAKE_SOURCE_DIR}/core/rust")
set(RUST_TARGET "xtensa-esp32s3-espidf")
set(RUST_LIB "${RUST_DIR}/target/${RUST_TARGET}/release/libcore.a")

add_custom_command(
    OUTPUT ${RUST_LIB}
    COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{HOME}/.cargo/bin:$ENV{PATH}"
            cargo +esp build --release
            -Z build-std=core,compiler_builtins
            -Z build-std-features=compiler-builtins-mem
            --target ${RUST_TARGET}
    WORKING_DIRECTORY ${RUST_DIR}
    COMMENT "Building Rust core (${RUST_TARGET})"
    VERBATIM
)

add_custom_target(rust_core ALL DEPENDS ${RUST_LIB})
add_dependencies(${COMPONENT_LIB} rust_core)

target_link_libraries(${COMPONENT_LIB} INTERFACE ${RUST_LIB})

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
