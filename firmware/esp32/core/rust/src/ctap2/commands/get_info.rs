use crate::core_api::CoreCtx;
use crate::ctap2::{cbor, constants, status::CtapStatus};

pub fn handle(_ctx: &mut CoreCtx, _cbor_req: &[u8], out: &mut [u8]) -> Result<usize, CtapStatus> {
    let mut w = cbor::Writer::new(out);

    w.map(5)?;

    // versions
    w.u8(1)?;
    w.array(1)?;
    w.tstr("FIDO_2_0")?;

    // aaguid
    w.u8(3)?;
    w.bstr(&constants::AAGUID)?;

    // options
    w.u8(4)?;
    w.map(4)?;
    w.tstr("rk")?;
    w.bool(false)?;
    w.tstr("up")?;
    w.bool(true)?;
    w.tstr("uv")?;
    w.bool(false)?;
    w.tstr("plat")?;
    w.bool(false)?;

    // maxMsgSize
    w.u8(5)?;
    w.u32(constants::MAX_MSG_SIZE as u32)?;

    // algorithms
    w.u8(0x0A)?;
    w.array(1)?;
    w.map(2)?;
    // Shorter key first for canonical CBOR.
    w.tstr("alg")?;
    w.nint(-7)?;
    w.tstr("type")?;
    w.tstr("public-key")?;


    Ok(w.len())
}
