use crate::core_api::CoreCtx;
use crate::ctap2::{cbor, status::CtapStatus};

pub fn handle(_ctx: &mut CoreCtx, _cbor_req: &[u8], out: &mut [u8]) -> Result<usize, CtapStatus> {
    // CTAP2 GetInfo response is a CBOR map with fields like:
    // 1: versions, 2: extensions, 3: aaguid, 4: options, ...
    //
    // Skeleton: return {"versions": ["FIDO_2_0"]} in minimal CBOR.
    // We'll encode a small CBOR map:
    // { 1: ["FIDO_2_0"] }
    //
    // Field numbers are per CTAP2 spec; keep minimal for now.

    let mut w = cbor::Writer::new(out);

    w.map(1)?; // map with 1 pair
    w.u8(1)?; // key = 1 (versions)
    w.array(1)?; // 1 element
    w.tstr("FIDO_2_0")?;

    Ok(w.len())
}
