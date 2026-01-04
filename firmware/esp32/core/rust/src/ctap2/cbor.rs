use crate::ctap2::status::CtapStatus;

pub struct Writer<'a> {
    out: &'a mut [u8],
    n: usize,
}

impl<'a> Writer<'a> {
    pub fn new(out: &'a mut [u8]) -> Self {
        Self { out, n: 0 }
    }
    pub fn len(&self) -> usize {
        self.n
    }

    fn push(&mut self, b: u8) -> Result<(), CtapStatus> {
        if self.n >= self.out.len() {
            return Err(CtapStatus::InvalidLength);
        }
        self.out[self.n] = b;
        self.n += 1;
        Ok(())
    }

    fn bytes(&mut self, data: &[u8]) -> Result<(), CtapStatus> {
        if self.n + data.len() > self.out.len() {
            return Err(CtapStatus::InvalidLength);
        }
        self.out[self.n..self.n + data.len()].copy_from_slice(data);
        self.n += data.len();
        Ok(())
    }

    // Major type 5 (map)
    pub fn map(&mut self, pairs: u8) -> Result<(), CtapStatus> {
        self.push(0b101_00000 | pairs)
    }

    // Major type 4 (array)
    pub fn array(&mut self, len: u8) -> Result<(), CtapStatus> {
        self.push(0b100_00000 | len)
    }

    // Major type 0 (unsigned int) small
    pub fn u8(&mut self, v: u8) -> Result<(), CtapStatus> {
        self.u32(v as u32)
    }

    // Major type 0 (unsigned int) generic
    pub fn u32(&mut self, v: u32) -> Result<(), CtapStatus> {
        match v {
            0..=23 => self.push(0b000_00000 | v as u8),
            24..=0xff => {
                self.push(0b000_11000)?; // uint8 follows
                self.push(v as u8)
            }
            0x100..=0xffff => {
                self.push(0b000_11001)?; // uint16 follows
                self.push((v >> 8) as u8)?;
                self.push(v as u8)
            }
            _ => {
                self.push(0b000_11010)?; // uint32 follows
                self.push((v >> 24) as u8)?;
                self.push((v >> 16) as u8)?;
                self.push((v >> 8) as u8)?;
                self.push(v as u8)
            }
        }
    }

    // Major type 1 (negative int) small-ish
    pub fn nint(&mut self, v: i32) -> Result<(), CtapStatus> {
        if v >= 0 {
            return Err(CtapStatus::InvalidParameter);
        }
        let magnitude = (-1 - v) as u32;
        match magnitude {
            0..=23 => self.push(0b001_00000 | magnitude as u8),
            24..=0xff => {
                self.push(0b001_11000)?; // uint8 follows
                self.push(magnitude as u8)
            }
            0x100..=0xffff => {
                self.push(0b001_11001)?; // uint16 follows
                self.push((magnitude >> 8) as u8)?;
                self.push(magnitude as u8)
            }
            _ => Err(CtapStatus::InvalidParameter),
        }
    }

    // Major type 3 (text string) small
    pub fn tstr(&mut self, s: &str) -> Result<(), CtapStatus> {
        let b = s.as_bytes();
        if b.len() < 24 {
            self.push(0b011_00000 | (b.len() as u8))?;
        } else {
            return Err(CtapStatus::InvalidLength); // keep it simple in skeleton
        }
        self.bytes(b)
    }

    // Major type 2 (byte string) small
    pub fn bstr(&mut self, data: &[u8]) -> Result<(), CtapStatus> {
        if data.len() < 24 {
            self.push(0b010_00000 | (data.len() as u8))?;
        } else {
            return Err(CtapStatus::InvalidLength);
        }
        self.bytes(data)
    }

    // Major type 7 (simple value) booleans
    pub fn bool(&mut self, v: bool) -> Result<(), CtapStatus> {
        if v {
            self.push(0b111_10101) // true
        } else {
            self.push(0b111_10100) // false
        }
    }
}
