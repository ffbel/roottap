#![allow(non_camel_case_types)]

use core::panic::PanicInfo;
use core::ffi::c_uchar;

use crate::core_api;

#[panic_handler]
fn panic(_: &PanicInfo) -> ! { loop {} }

#[allow(dead_code)]
#[repr(C)]
pub struct core_ctx_t {
    _private: [u8; 0],
}

#[unsafe(no_mangle)]
pub extern "C" fn core_ctx_size() -> usize {
    core_api::ctx_size()
}

/// Initialize context in caller-provided memory.
#[unsafe(no_mangle)]
pub extern "C" fn core_init(ctx_mem: *mut u8, ctx_mem_len: usize) -> i32 {
    core_api::init(ctx_mem, ctx_mem_len)
}

/// Feed a full CTAP2 request (already reassembled by HID layer).
#[unsafe(no_mangle)]
pub extern "C" fn core_handle_request(
    ctx_mem: *mut u8,
    ctx_mem_len: usize,
    req: *const c_uchar,
    req_len: usize,
    resp: *mut c_uchar,
    resp_cap: usize,
    out_resp_len: *mut usize,
) -> i32 {
    core_api::handle_request(ctx_mem, ctx_mem_len, req, req_len, resp, resp_cap, out_resp_len)
}
