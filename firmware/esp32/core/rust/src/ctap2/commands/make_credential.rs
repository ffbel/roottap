use crate::core_api::CoreCtx;
use crate::ctap2::status::CtapStatus;

// TODO()
pub fn handle(_ctx: &mut CoreCtx, _cbor_req: &[u8], _out: &mut [u8]) -> Result<usize, CtapStatus> {
    // Until implemented, deny.
    Err(CtapStatus::OperationDenied)
}
