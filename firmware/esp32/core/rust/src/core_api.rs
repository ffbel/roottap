use core::{mem, ptr, slice};

use crate::ctap2::{dispatcher::dispatch, status::CtapStatus};

pub struct CoreCtx {
    // TODO(): persistent state, pin retries, uv/permissions, session, etc.
    pub initialized: bool,
}

impl CoreCtx {
    pub const fn new() -> Self {
        Self { initialized: false }
    }
}

pub fn ctx_size() -> usize {
    mem::size_of::<CoreCtx>()
}

fn ctx_from_mem<'a>(ctx_mem: *mut u8, ctx_mem_len: usize) -> Result<&'a mut CoreCtx, CtapStatus> {
    if ctx_mem.is_null() || ctx_mem_len < mem::size_of::<CoreCtx>() {
        return Err(CtapStatus::Other);
    }
    let ctx_ptr = ctx_mem as *mut CoreCtx;
    Ok(unsafe { &mut *ctx_ptr })
}

pub fn init(ctx_mem: *mut u8, ctx_mem_len: usize) -> i32 {
    let ctx = match ctx_from_mem(ctx_mem, ctx_mem_len) {
        Ok(c) => c,
        Err(e) => return e.as_i32(),
    };
    // placement init
    unsafe { ptr::write(ctx as *mut CoreCtx, CoreCtx::new()); }
    ctx.initialized = true;
    0
}

pub fn handle_request(
    ctx_mem: *mut u8,
    ctx_mem_len: usize,
    req: *const u8,
    req_len: usize,
    resp: *mut u8,
    resp_cap: usize,
    out_resp_len: *mut usize,
) -> i32 {
    let ctx = match ctx_from_mem(ctx_mem, ctx_mem_len) {
        Ok(c) if c.initialized => c,
        _ => return CtapStatus::Other.as_i32(),
    };

    if req.is_null() || resp.is_null() || out_resp_len.is_null() {
        return CtapStatus::Other.as_i32();
    }

    let req = unsafe { slice::from_raw_parts(req, req_len) };
    let resp_buf = unsafe { slice::from_raw_parts_mut(resp, resp_cap) };

    match dispatch(ctx, req, resp_buf) {
        Ok(n) => {
            unsafe { *out_resp_len = n; }
            0
        }
        Err(e) => e.as_i32(),
    }
}
