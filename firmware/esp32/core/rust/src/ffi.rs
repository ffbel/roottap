use core::ffi::c_char;

#[unsafe(no_mangle)]
pub extern "C" fn rust_hello() -> *const c_char {
    b"Hello world from Rust\0".as_ptr() as *const c_char
}
