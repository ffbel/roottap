#![allow(non_camel_case_types)]

use core::{ffi::c_uchar, panic::PanicInfo, slice};

use crate::{
    core_api,
    ctap2::status::CtapStatus,
};

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

#[unsafe(no_mangle)]
pub extern "C" fn core_persist_size() -> usize {
    core_api::persist_blob_size()
}

#[unsafe(no_mangle)]
pub extern "C" fn core_save_state(
    ctx_mem: *mut u8,
    ctx_mem_len: usize,
    out: *mut c_uchar,
    out_cap: usize,
    out_len: *mut usize,
) -> i32 {
    let ctx = match core_api::ctx_from_mem(ctx_mem, ctx_mem_len) {
        Ok(c) if c.initialized => c,
        _ => return CtapStatus::Other.as_i32(),
    };

    if out.is_null() || out_len.is_null() {
        return CtapStatus::Other.as_i32();
    }

    let out_slice = unsafe { slice::from_raw_parts_mut(out, out_cap) };
    match core_api::persist_export(ctx, out_slice) {
        Ok(n) => {
            unsafe { *out_len = n; }
            0
        }
        Err(e) => e.as_i32(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn core_load_state(
    ctx_mem: *mut u8,
    ctx_mem_len: usize,
    data: *const c_uchar,
    data_len: usize,
) -> i32 {
    let ctx = match core_api::ctx_from_mem(ctx_mem, ctx_mem_len) {
        Ok(c) => c,
        Err(e) => return e.as_i32(),
    };

    if data.is_null() {
        return CtapStatus::Other.as_i32();
    }

    let data_slice = unsafe { slice::from_raw_parts(data, data_len) };
    match core_api::persist_import(ctx, data_slice) {
        Ok(()) => 0,
        Err(e) => e.as_i32(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn core_is_dirty(ctx_mem: *mut u8, ctx_mem_len: usize) -> bool {
    core_api::ctx_from_mem(ctx_mem, ctx_mem_len)
        .map(|c| c.is_dirty())
        .unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn core_mark_clean(ctx_mem: *mut u8, ctx_mem_len: usize) {
    if let Ok(ctx) = core_api::ctx_from_mem(ctx_mem, ctx_mem_len) {
        ctx.clear_dirty();
    }
}
