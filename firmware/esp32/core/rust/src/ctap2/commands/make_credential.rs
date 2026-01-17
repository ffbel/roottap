use crate::core_api::{CoreCtx, Credential, CREDENTIAL_ID_SIZE, MAX_USER_ID_SIZE};
use crate::ctap2::{cbor, constants, status::CtapStatus};

use p256::{EncodedPoint, PublicKey, SecretKey};
use p256::elliptic_curve::sec1::ToEncodedPoint;
use rand_core::{CryptoRng, Error as RngError, RngCore};
use sha2::{Digest, Sha256};

const ES256_ALG: i32 = -7;

const FLAG_UP: u8 = 0x01;
const FLAG_UV: u8 = 0x04;
const FLAG_AT: u8 = 0x40;

unsafe extern "C" {
    fn esp_random() -> u32;
}

struct EspRng;

impl RngCore for EspRng {
    fn next_u32(&mut self) -> u32 {
        unsafe { esp_random() }
    }

    fn next_u64(&mut self) -> u64 {
        ((self.next_u32() as u64) << 32) | (self.next_u32() as u64)
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        let mut i = 0;
        while i < dest.len() {
            let chunk = self.next_u32().to_be_bytes();
            for b in chunk {
                if i >= dest.len() {
                    break;
                }
                dest[i] = b;
                i += 1;
            }
        }
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), RngError> {
        self.fill_bytes(dest);
        Ok(())
    }
}

impl CryptoRng for EspRng {}

struct ParsedRequest<'a> {
    rp_id: &'a str,
    user_id: [u8; MAX_USER_ID_SIZE],
    user_id_len: u8,
    client_data_hash: [u8; 32],
    up: bool,
    uv: bool,
    _rk: bool,
}

struct Options {
    up: bool,
    uv: bool,
    rk: bool,
}

pub fn handle(ctx: &mut CoreCtx, cbor_req: &[u8], out: &mut [u8]) -> Result<usize, CtapStatus> {
    let mut reader = cbor::Reader::new(cbor_req);
    let req = parse_make_credential(&mut reader)?;
    let rp_hash = sha256(req.rp_id);
    let mut rng = EspRng;

    let cred_slot = ctx.alloc_credential_slot().ok_or(CtapStatus::KeyStoreFull)?;

    let secret = SecretKey::random(&mut rng);
    let public = secret.public_key();

    let mut credential_id = [0u8; CREDENTIAL_ID_SIZE];
    rng.fill_bytes(&mut credential_id);

    let mut new_cred = Credential::empty();
    new_cred.in_use = true;
    new_cred.sign_count = 0;
    new_cred.rp_id_hash.copy_from_slice(&rp_hash);
    new_cred.cred_id.copy_from_slice(&credential_id);
    new_cred.user_id_len = req.user_id_len;
    new_cred.user_id[..req.user_id_len as usize].copy_from_slice(&req.user_id[..req.user_id_len as usize]);

    let priv_bytes = secret.to_bytes();
    new_cred.private_key.copy_from_slice(priv_bytes.as_ref());

    // Public key in COSE_Key format
    let mut cose_key = [0u8; 96];
    let cose_len = encode_cose_public_key(&public, &mut cose_key)?;

    // authData
    let mut auth_data = [0u8; 256];
    let flags = build_flags(req.up, req.uv);
    let auth_len = build_auth_data(
        &rp_hash,
        flags,
        new_cred.sign_count,
        &credential_id,
        &cose_key[..cose_len],
        &mut auth_data,
    )?;

    // Persist only after successful construction
    *cred_slot = new_cred;
    ctx.mark_dirty();

    // attestationObject (fmt = "none", empty attStmt)
    let mut w = cbor::Writer::new(out);
    // CTAP2 encodes attestationObject as a CBOR map with numeric keys: 1=fmt, 2=authData, 3=attStmt
    w.map(3)?;        // 3 pairs
    w.u8(1)?;         // fmt
    w.tstr("none")?;
    w.u8(2)?;         // authData
    w.bstr(&auth_data[..auth_len])?;
    w.u8(3)?;         // attStmt
    w.map(0)?;        // empty map

    Ok(w.len())
}

fn build_flags(up: bool, uv: bool) -> u8 {
    // User presence is always asserted for this prototype.
    let _ = up;
    let mut flags = FLAG_AT | FLAG_UP;
    if uv {
        flags |= FLAG_UV;
    }
    flags
}

fn build_auth_data(
    rp_id_hash: &[u8; 32],
    flags: u8,
    sign_count: u32,
    credential_id: &[u8],
    cose_pub_key: &[u8],
    out: &mut [u8],
) -> Result<usize, CtapStatus> {
    let need = 32 + 1 + 4 + 16 + 2 + credential_id.len() + cose_pub_key.len();
    if out.len() < need {
        return Err(CtapStatus::InvalidLength);
    }

    let mut n = 0;

    out[n..n + 32].copy_from_slice(rp_id_hash);
    n += 32;

    out[n] = flags;
    n += 1;

    out[n] = (sign_count >> 24) as u8;
    out[n + 1] = (sign_count >> 16) as u8;
    out[n + 2] = (sign_count >> 8) as u8;
    out[n + 3] = sign_count as u8;
    n += 4;

    out[n..n + 16].copy_from_slice(&constants::AAGUID);
    n += 16;

    if credential_id.len() > u16::MAX as usize {
        return Err(CtapStatus::InvalidLength);
    }
    let cred_len = credential_id.len() as u16;
    out[n] = (cred_len >> 8) as u8;
    out[n + 1] = cred_len as u8;
    n += 2;

    out[n..n + credential_id.len()].copy_from_slice(credential_id);
    n += credential_id.len();

    out[n..n + cose_pub_key.len()].copy_from_slice(cose_pub_key);
    n += cose_pub_key.len();

    Ok(n)
}

fn encode_cose_public_key(pk: &PublicKey, out: &mut [u8]) -> Result<usize, CtapStatus> {
    let point: EncodedPoint = pk.to_encoded_point(false);
    let x = point.x().ok_or(CtapStatus::Other)?;
    let y = point.y().ok_or(CtapStatus::Other)?;

    // Canonical key order (CBOR): 1, 3, -1, -2, -3
    let mut w = cbor::Writer::new(out);
    w.map(5)?;
    w.u8(1)?; // kty: EC2
    w.u8(2)?;
    w.u8(3)?;
    w.nint(ES256_ALG)?;
    w.nint(-1)?; // crv: P-256
    w.u8(1)?;
    w.nint(-2)?; // x
    w.bstr(x.as_ref())?;
    w.nint(-3)?; // y
    w.bstr(y.as_ref())?;
    Ok(w.len())
}

fn parse_make_credential<'a>(reader: &mut cbor::Reader<'a>) -> Result<ParsedRequest<'a>, CtapStatus> {
    let pairs = reader.map()?;
    let mut rp_id: Option<&str> = None;
    let mut user: Option<(u8, [u8; MAX_USER_ID_SIZE])> = None;
    let mut client_hash: Option<[u8; 32]> = None;
    let mut alg_ok: Option<bool> = None;
    let mut opts = Options {
        up: true,
        uv: false,
        rk: false,
    };

    for _ in 0..pairs {
        let key = reader.u32()?;
        match key {
            1 => {
                let hash = reader.bstr()?;
                if hash.len() != 32 {
                    return Err(CtapStatus::InvalidLength);
                }
                let mut tmp = [0u8; 32];
                tmp.copy_from_slice(hash);
                client_hash = Some(tmp);
            }
            2 => rp_id = Some(parse_rp(reader)?),
            3 => user = Some(parse_user(reader)?),
            4 => alg_ok = Some(parse_pubkey_params(reader)?),
            7 => opts = parse_options(reader)?,
            _ => reader.skip()?,
        }
    }

    let rp_id = rp_id.ok_or(CtapStatus::MissingParameter)?;
    let (user_id_len, user_id) = user.ok_or(CtapStatus::MissingParameter)?;
    let client_data_hash = client_hash.ok_or(CtapStatus::MissingParameter)?;

    match alg_ok {
        Some(true) => {}
        Some(false) => return Err(CtapStatus::UnsupportedAlgorithm),
        None => return Err(CtapStatus::MissingParameter),
    }

    Ok(ParsedRequest {
        rp_id,
        user_id,
        user_id_len,
        client_data_hash,
        up: opts.up,
        uv: opts.uv,
        _rk: opts.rk,
    })
}

fn parse_rp<'a>(reader: &mut cbor::Reader<'a>) -> Result<&'a str, CtapStatus> {
    let len = reader.map()?;
    let mut id = None;
    for _ in 0..len {
        let key = reader.tstr()?;
        match key {
            "id" => id = Some(reader.tstr()?),
            _ => reader.skip()?,
        }
    }
    id.ok_or(CtapStatus::MissingParameter)
}

fn parse_user(reader: &mut cbor::Reader<'_>) -> Result<(u8, [u8; MAX_USER_ID_SIZE]), CtapStatus> {
    let len = reader.map()?;
    let mut id = None;
    for _ in 0..len {
        let key = reader.tstr()?;
        match key {
            "id" => {
                let val = reader.bstr()?;
                if val.is_empty() || val.len() > MAX_USER_ID_SIZE {
                    return Err(CtapStatus::InvalidLength);
                }
                let mut buf = [0u8; MAX_USER_ID_SIZE];
                buf[..val.len()].copy_from_slice(val);
                id = Some((val.len() as u8, buf));
            }
            _ => reader.skip()?,
        }
    }
    id.ok_or(CtapStatus::MissingParameter)
}

fn parse_pubkey_params(reader: &mut cbor::Reader<'_>) -> Result<bool, CtapStatus> {
    let len = reader.array()?;
    let mut found = false;
    for _ in 0..len {
        let map_len = reader.map()?;
        let mut alg = None;
        let mut ty = None;
        for _ in 0..map_len {
            let key = reader.tstr()?;
            match key {
                "alg" => alg = Some(reader.nint()?),
                "type" => ty = Some(reader.tstr()?),
                _ => reader.skip()?,
            }
        }
        if alg == Some(ES256_ALG) && ty == Some("public-key") {
            found = true;
        }
    }
    Ok(found)
}

fn parse_options(reader: &mut cbor::Reader<'_>) -> Result<Options, CtapStatus> {
    let len = reader.map()?;
    let mut opts = Options {
        up: true,
        uv: false,
        rk: false,
    };

    for _ in 0..len {
        let key = reader.tstr()?;
        match key {
            "up" => opts.up = reader.bool()?,
            "uv" => opts.uv = reader.bool()?,
            "rk" => opts.rk = reader.bool()?,
            _ => reader.skip()?,
        }
    }
    Ok(opts)
}

fn sha256(data: &str) -> [u8; 32] {
    let mut hasher = Sha256::new();
    hasher.update(data.as_bytes());
    let digest = hasher.finalize();
    let mut out = [0u8; 32];
    out.copy_from_slice(&digest);
    out
}
