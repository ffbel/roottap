// CTAP2 command bytes (CTAPHID_CBOR payload first byte)
pub const CTAP2_MAKE_CREDENTIAL: u8 = 0x01;
pub const CTAP2_GET_ASSERTION: u8 = 0x02;
pub const CTAP2_GET_INFO: u8 = 0x04;
pub const CTAP2_CLIENT_PIN: u8 = 0x06;
pub const CTAP2_RESET: u8 = 0x07;
pub const CTAP2_SELECTION: u8 = 0x0B;

// Common limits
pub const MAX_MSG_SIZE: usize = 1024; // tune later (HID supports larger)
