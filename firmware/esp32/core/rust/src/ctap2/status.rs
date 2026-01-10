#[repr(u8)]
#[derive(Copy, Clone, Debug)]
pub enum CtapStatus {
    Ok = 0x00,

    // Common CTAP2 error codes (subset)
    InvalidCommand = 0x01,
    InvalidParameter = 0x02,
    InvalidLength = 0x03,
    InvalidSeq = 0x04,
    Timeout = 0x05,
    ChannelBusy = 0x06,
    LockRequired = 0x0A,
    InvalidChannel = 0x0B,

    CborUnexpectedType = 0x11,
    InvalidCbor = 0x12,
    MissingParameter = 0x14,

    UnsupportedAlgorithm = 0x26,
    OperationDenied = 0x27,
    KeyStoreFull = 0x28,
    NoCredentials = 0x2E,

    Other = 0x7F,
}

impl CtapStatus {
    pub fn as_i32(self) -> i32 { self as u8 as i32 }
}
