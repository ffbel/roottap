use crate::core_api::{CoreCtx, Credential, CREDENTIAL_ID_SIZE};
use crate::ctap2::{cbor, status::CtapStatus};
use p256::ecdsa::{signature::Signer, DerSignature, Signature, SigningKey};
use sha2::{Digest, Sha256};

const FLAG_UP: u8 = 0x01;
const AUTH_DATA_LEN: usize = 32 + 1 + 4;
const SIG_INPUT_LEN: usize = AUTH_DATA_LEN + 32; // authData + clientDataHash

struct ParsedRequest<'a> {
    rp_id: &'a str,
    client_data_hash: [u8; 32],
    allow_cred_id: [u8; CREDENTIAL_ID_SIZE],
}

pub fn handle(ctx: &mut CoreCtx, cbor_req: &[u8], out: &mut [u8]) -> Result<usize, CtapStatus> {
    let mut reader = cbor::Reader::new(cbor_req);
    let req = parse_get_assertion(&mut reader)?;

    let rp_hash = sha256(req.rp_id);
    let cred = find_credential(ctx, &req.allow_cred_id, &rp_hash)?;

    // TODO: wire to real user-presence + keepalive once transport exposes it.
    require_user_presence()?;

    let mut auth_data = [0u8; AUTH_DATA_LEN];
    let new_sign_count = cred.sign_count.wrapping_add(1);
    let auth_len = build_auth_data(&rp_hash, new_sign_count, &mut auth_data)?;
    cred.sign_count = new_sign_count;

    let mut signed_data = [0u8; SIG_INPUT_LEN];
    signed_data[..auth_len].copy_from_slice(&auth_data[..auth_len]);
    signed_data[auth_len..auth_len + req.client_data_hash.len()]
        .copy_from_slice(&req.client_data_hash);

    let signing_key = SigningKey::from_slice(&cred.private_key).map_err(|_| CtapStatus::Other)?;
    let signature: Signature = signing_key
        .try_sign(&signed_data[..auth_len + req.client_data_hash.len()])
        .map_err(|_| CtapStatus::Other)?;
    let sig_der: DerSignature = signature.to_der();

    let mut w = cbor::Writer::new(out);
    w.map(3)?;
    w.u8(1)?;
    encode_credential_descriptor(&req.allow_cred_id, &mut w)?;
    w.u8(2)?;
    w.bstr(&auth_data[..auth_len])?;
    w.u8(3)?;
    w.bstr(sig_der.as_bytes())?;

    Ok(w.len())
}

fn parse_get_assertion<'a>(reader: &mut cbor::Reader<'a>) -> Result<ParsedRequest<'a>, CtapStatus> {
    let pairs = reader.map()?;
    let mut rp_id: Option<&str> = None;
    let mut client_data_hash: Option<[u8; 32]> = None;
    let mut allow_cred_id: Option<[u8; CREDENTIAL_ID_SIZE]> = None;

    for _ in 0..pairs {
        let key = reader.u32()?;
        match key {
            1 => rp_id = Some(reader.tstr()?),
            2 => client_data_hash = Some(parse_client_hash(reader)?),
            3 => allow_cred_id = Some(parse_allow_list(reader)?),
            _ => reader.skip()?,
        }
    }

    Ok(ParsedRequest {
        rp_id: rp_id.ok_or(CtapStatus::MissingParameter)?,
        client_data_hash: client_data_hash.ok_or(CtapStatus::MissingParameter)?,
        allow_cred_id: allow_cred_id.ok_or(CtapStatus::MissingParameter)?,
    })
}

fn parse_client_hash(reader: &mut cbor::Reader<'_>) -> Result<[u8; 32], CtapStatus> {
    let hash = reader.bstr()?;
    if hash.len() != 32 {
        return Err(CtapStatus::InvalidLength);
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(hash);
    Ok(out)
}

fn parse_allow_list(reader: &mut cbor::Reader<'_>) -> Result<[u8; CREDENTIAL_ID_SIZE], CtapStatus> {
    let len = reader.array()?;
    if len == 0 {
        return Err(CtapStatus::MissingParameter);
    }
    let cred_id = parse_credential_descriptor(reader)?;
    for _ in 1..len {
        reader.skip()?;
    }
    Ok(cred_id)
}

fn parse_credential_descriptor(
    reader: &mut cbor::Reader<'_>,
) -> Result<[u8; CREDENTIAL_ID_SIZE], CtapStatus> {
    let pairs = reader.map()?;
    let mut ty = None;
    let mut id: Option<[u8; CREDENTIAL_ID_SIZE]> = None;

    for _ in 0..pairs {
        let key = reader.tstr()?;
        match key {
            "type" => ty = Some(reader.tstr()?),
            "id" => {
                let raw = reader.bstr()?;
                if raw.len() != CREDENTIAL_ID_SIZE {
                    return Err(CtapStatus::InvalidLength);
                }
                let mut tmp = [0u8; CREDENTIAL_ID_SIZE];
                tmp.copy_from_slice(raw);
                id = Some(tmp);
            }
            _ => reader.skip()?,
        }
    }

    if ty != Some("public-key") {
        return Err(CtapStatus::InvalidParameter);
    }

    id.ok_or(CtapStatus::MissingParameter)
}

fn find_credential<'a>(
    ctx: &'a mut CoreCtx,
    cred_id: &[u8; CREDENTIAL_ID_SIZE],
    rp_hash: &[u8; 32],
) -> Result<&'a mut Credential, CtapStatus> {
    ctx.credentials
        .iter_mut()
        .find(|c| c.in_use && c.cred_id == *cred_id && c.rp_id_hash == *rp_hash)
        .ok_or(CtapStatus::NoCredentials)
}

fn build_auth_data(
    rp_hash: &[u8; 32],
    sign_count: u32,
    out: &mut [u8],
) -> Result<usize, CtapStatus> {
    if out.len() < AUTH_DATA_LEN {
        return Err(CtapStatus::InvalidLength);
    }

    out[..32].copy_from_slice(rp_hash);
    out[32] = FLAG_UP;
    out[33] = (sign_count >> 24) as u8;
    out[34] = (sign_count >> 16) as u8;
    out[35] = (sign_count >> 8) as u8;
    out[36] = sign_count as u8;

    Ok(AUTH_DATA_LEN)
}

fn encode_credential_descriptor(
    cred_id: &[u8; CREDENTIAL_ID_SIZE],
    w: &mut cbor::Writer<'_>,
) -> Result<(), CtapStatus> {
    w.map(2)?;
    // Canonical ordering: shorter key first.
    w.tstr("id")?;
    w.bstr(cred_id)?;
    w.tstr("type")?;
    w.tstr("public-key")
}

fn require_user_presence() -> Result<(), CtapStatus> {
    // Placeholder: always satisfied for now.
    Ok(())
}

fn sha256(data: &str) -> [u8; 32] {
    let mut hasher = Sha256::new();
    hasher.update(data.as_bytes());
    let digest = hasher.finalize();
    let mut out = [0u8; 32];
    out.copy_from_slice(&digest);
    out
}
