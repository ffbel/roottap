use super::status::CtapStatus;

const USER_PRESENCE_TIMEOUT_MS: u32 = 20_000;
const USER_PRESENCE_OK: i32 = 0;
const USER_PRESENCE_DENIED: i32 = 1;
const USER_PRESENCE_TIMEOUT: i32 = 2;
const USER_PRESENCE_ERROR: i32 = 3;

unsafe extern "C" {
    fn user_presence_wait_for_approval(timeout_ms: u32) -> i32;
}

pub fn require_user_presence() -> Result<(), CtapStatus> {
    let rc = unsafe { user_presence_wait_for_approval(USER_PRESENCE_TIMEOUT_MS) };
    match rc {
        USER_PRESENCE_OK => Ok(()),
        USER_PRESENCE_DENIED => Err(CtapStatus::OperationDenied),
        USER_PRESENCE_TIMEOUT | USER_PRESENCE_ERROR => Err(CtapStatus::Timeout),
        _ => Err(CtapStatus::Other),
    }
}
