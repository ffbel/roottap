use crate::core_api::CoreCtx;
use super::{constants::*, status::CtapStatus};

use super::commands;

pub fn dispatch(ctx: &mut CoreCtx, req: &[u8], resp: &mut [u8]) -> Result<usize, CtapStatus> {
    if req.is_empty() {
        return Err(CtapStatus::InvalidLength);
    }

    let cmd = req[0];
    let cbor = &req[1..];

    // CTAP2 over CBOR response format: first byte = status, then CBOR map (optional)
    // TODO(): write status later; handlers write CBOR payload into resp[1..]
    resp[0] = CtapStatus::Ok as u8;

    let out_len = match cmd {
        CTAP2_GET_INFO        => commands::get_info::handle(ctx, cbor, &mut resp[1..])?,
        CTAP2_MAKE_CREDENTIAL => commands::make_credential::handle(ctx, cbor, &mut resp[1..])?,
        CTAP2_GET_ASSERTION   => commands::get_assertion::handle(ctx, cbor, &mut resp[1..])?,
        CTAP2_CLIENT_PIN      => commands::client_pin::handle(ctx, cbor, &mut resp[1..])?,
        CTAP2_RESET           => commands::reset::handle(ctx, cbor, &mut resp[1..])?,
        CTAP2_SELECTION       => commands::selection::handle(ctx, cbor, &mut resp[1..])?,
        _ => return Err(CtapStatus::InvalidCommand),
    };

    Ok(1 + out_len)
}
