use core::{mem, ptr, slice};

use crate::ctap2::{dispatcher::dispatch, status::CtapStatus};

pub const MAX_CREDENTIALS: usize = 4;
pub const CREDENTIAL_ID_SIZE: usize = 16;
pub const MAX_USER_ID_SIZE: usize = 32;
const PERSIST_MAGIC: u32 = 0x52544B59; // "RTKY"
const PERSIST_VERSION: u16 = 1;

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

#[repr(C)]
#[derive(Copy, Clone)]
struct PersistedCredential {
    in_use: u8,
    user_id_len: u8,
    _reserved: u16,
    sign_count: u32,
    cred_id: [u8; CREDENTIAL_ID_SIZE],
    rp_id_hash: [u8; 32],
    user_id: [u8; MAX_USER_ID_SIZE],
    private_key: [u8; 32],
}

impl PersistedCredential {
    const fn empty() -> Self {
        Self {
            in_use: 0,
            user_id_len: 0,
            _reserved: 0,
            sign_count: 0,
            cred_id: [0; CREDENTIAL_ID_SIZE],
            rp_id_hash: [0; 32],
            user_id: [0; MAX_USER_ID_SIZE],
            private_key: [0; 32],
        }
    }

    fn from_runtime(c: &Credential) -> Self {
        let mut p = Self::empty();
        if c.in_use {
            p.in_use = 1;
        }
        p.user_id_len = c.user_id_len;
        p.sign_count = c.sign_count;
        p.cred_id = c.cred_id;
        p.rp_id_hash = c.rp_id_hash;
        p.user_id = c.user_id;
        p.private_key = c.private_key;
        p
    }

    fn to_runtime(&self) -> Result<Credential, CtapStatus> {
        let mut c = Credential::empty();
        if self.in_use == 0 {
            return Ok(c);
        }
        if self.user_id_len as usize > MAX_USER_ID_SIZE {
            return Err(CtapStatus::Other);
        }
        c.in_use = true;
        c.user_id_len = self.user_id_len;
        c.sign_count = self.sign_count;
        c.cred_id = self.cred_id;
        c.rp_id_hash = self.rp_id_hash;
        c.user_id[..self.user_id_len as usize]
            .copy_from_slice(&self.user_id[..self.user_id_len as usize]);
        c.private_key = self.private_key;
        Ok(c)
    }
}

#[repr(C)]
#[derive(Copy, Clone)]
struct PersistedState {
    magic: u32,
    version: u16,
    _reserved: u16,
    creds: [PersistedCredential; MAX_CREDENTIALS],
}

impl PersistedState {
    const fn new() -> Self {
        Self {
            magic: PERSIST_MAGIC,
            version: PERSIST_VERSION,
            _reserved: 0,
            creds: [PersistedCredential::empty(); MAX_CREDENTIALS],
        }
    }

    fn from_ctx(ctx: &CoreCtx) -> Self {
        let mut s = Self::new();
        for (dst, src) in s.creds.iter_mut().zip(ctx.credentials.iter()) {
            *dst = PersistedCredential::from_runtime(src);
        }
        s
    }

    fn apply(&self, ctx: &mut CoreCtx) -> Result<(), CtapStatus> {
        if self.magic != PERSIST_MAGIC || self.version != PERSIST_VERSION {
            return Err(CtapStatus::Other);
        }
        *ctx = CoreCtx::new();
        for (dst, src) in ctx.credentials.iter_mut().zip(self.creds.iter()) {
            *dst = src.to_runtime()?;
        }
        ctx.initialized = true;
        ctx.clear_dirty();
        Ok(())
    }
}

pub struct CoreCtx {
    // TODO(): persistent state, pin retries, uv/permissions, session, etc.
    pub initialized: bool,
    pub credentials: [Credential; MAX_CREDENTIALS],
    dirty: bool,
}

impl CoreCtx {
    pub const fn new() -> Self {
        Self {
            initialized: false,
            credentials: [Credential::empty(); MAX_CREDENTIALS],
            dirty: false,
        }
    }

    pub fn alloc_credential_slot(&mut self) -> Option<&mut Credential> {
        self.credentials.iter_mut().find(|c| !c.in_use)
    }

    pub fn mark_dirty(&mut self) {
        self.dirty = true;
    }

    pub fn clear_dirty(&mut self) {
        self.dirty = false;
    }

    pub fn is_dirty(&self) -> bool {
        self.dirty
    }
}

pub fn ctx_size() -> usize {
    mem::size_of::<CoreCtx>()
}

pub(crate) fn ctx_from_mem<'a>(ctx_mem: *mut u8, ctx_mem_len: usize) -> Result<&'a mut CoreCtx, CtapStatus> {
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

pub fn persist_blob_size() -> usize {
    mem::size_of::<PersistedState>()
}

pub fn persist_export(ctx: &CoreCtx, out: &mut [u8]) -> Result<usize, CtapStatus> {
    let state = PersistedState::from_ctx(ctx);
    let raw = unsafe {
        slice::from_raw_parts(
            &state as *const PersistedState as *const u8,
            mem::size_of::<PersistedState>(),
        )
    };

    if out.len() < raw.len() {
        return Err(CtapStatus::InvalidLength);
    }
    out[..raw.len()].copy_from_slice(raw);
    Ok(raw.len())
}

pub fn persist_import(ctx: &mut CoreCtx, data: &[u8]) -> Result<(), CtapStatus> {
    if data.len() != mem::size_of::<PersistedState>() {
        return Err(CtapStatus::InvalidLength);
    }
    let mut tmp: mem::MaybeUninit<PersistedState> = mem::MaybeUninit::uninit();
    unsafe {
        ptr::copy_nonoverlapping(
            data.as_ptr(),
            tmp.as_mut_ptr() as *mut u8,
            mem::size_of::<PersistedState>(),
        );
        let state = tmp.assume_init();
        state.apply(ctx)
    }
}
