// TODO()
use crate::core_api::CoreCtx;
use crate::ctap2::status::CtapStatus;

pub fn handle(_ctx: &mut CoreCtx, _cbor_req: &[u8], _out: &mut [u8]) -> Result<usize, CtapStatus> {
    Err(CtapStatus::NoCredentials)
}
