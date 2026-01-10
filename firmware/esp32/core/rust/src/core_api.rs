use core::{mem, ptr, slice};

use crate::ctap2::{dispatcher::dispatch, status::CtapStatus};

pub const MAX_CREDENTIALS: usize = 4;
pub const CREDENTIAL_ID_SIZE: usize = 16;
pub const MAX_USER_ID_SIZE: usize = 32;

#[derive(Copy, Clone)]
pub struct Credential {
    pub in_use: bool,
    pub cred_id: [u8; CREDENTIAL_ID_SIZE],
    pub rp_id_hash: [u8; 32],
    pub user_id: [u8; MAX_USER_ID_SIZE],
    pub user_id_len: u8,
    pub sign_count: u32,
    pub private_key: [u8; 32],
}

impl Credential {
    pub const fn empty() -> Self {
        Self {
            in_use: false,
            cred_id: [0; CREDENTIAL_ID_SIZE],
            rp_id_hash: [0; 32],
            user_id: [0; MAX_USER_ID_SIZE],
            user_id_len: 0,
            sign_count: 0,
            private_key: [0; 32],
        }
    }
}

pub struct CoreCtx {
    // TODO(): persistent state, pin retries, uv/permissions, session, etc.
    pub initialized: bool,
    pub credentials: [Credential; MAX_CREDENTIALS],
}

impl CoreCtx {
    pub const fn new() -> Self {
        Self {
            initialized: false,
            credentials: [Credential::empty(); MAX_CREDENTIALS],
        }
    }

    pub fn alloc_credential_slot(&mut self) -> Option<&mut Credential> {
        self.credentials.iter_mut().find(|c| !c.in_use)
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
