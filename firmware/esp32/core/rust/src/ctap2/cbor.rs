use core::str;

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

    fn major_len(&mut self, major: u8, len: usize) -> Result<(), CtapStatus> {
        if len <= 23 {
            return self.push((major << 5) | (len as u8));
        }
        if len <= 0xff {
            self.push((major << 5) | 24)?;
            return self.push(len as u8);
        }
        if len <= 0xffff {
            self.push((major << 5) | 25)?;
            self.push((len >> 8) as u8)?;
            return self.push(len as u8);
        }
        if len > u32::MAX as usize {
            return Err(CtapStatus::InvalidLength);
        }
        self.push((major << 5) | 26)?;
        self.push((len >> 24) as u8)?;
        self.push((len >> 16) as u8)?;
        self.push((len >> 8) as u8)?;
        self.push(len as u8)
    }

    // Major type 5 (map)
    pub fn map(&mut self, pairs: usize) -> Result<(), CtapStatus> {
        self.major_len(5, pairs)
    }

    // Major type 4 (array)
    pub fn array(&mut self, len: usize) -> Result<(), CtapStatus> {
        self.major_len(4, len)
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
        self.major_len(3, b.len())?;
        self.bytes(b)
    }

    // Major type 2 (byte string) small
    pub fn bstr(&mut self, data: &[u8]) -> Result<(), CtapStatus> {
        self.major_len(2, data.len())?;
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

pub struct Reader<'a> {
    data: &'a [u8],
    pos: usize,
}

impl<'a> Reader<'a> {
    pub fn new(data: &'a [u8]) -> Self {
        Self { data, pos: 0 }
    }

    fn take(&mut self) -> Result<u8, CtapStatus> {
        if self.pos >= self.data.len() {
            return Err(CtapStatus::InvalidCbor);
        }
        let b = self.data[self.pos];
        self.pos += 1;
        Ok(b)
    }

    fn read_uint(&mut self, addl: u8) -> Result<u64, CtapStatus> {
        match addl {
            v @ 0..=23 => Ok(v as u64),
            24 => Ok(self.take()? as u64),
            25 => {
                let msb = self.take()? as u16;
                let lsb = self.take()? as u16;
                Ok(((msb << 8) | lsb) as u64)
            }
            26 => {
                let b0 = self.take()? as u32;
                let b1 = self.take()? as u32;
                let b2 = self.take()? as u32;
                let b3 = self.take()? as u32;
                Ok(((b0 << 24) | (b1 << 16) | (b2 << 8) | b3) as u64)
            }
            _ => Err(CtapStatus::InvalidCbor),
        }
    }

    fn read_len(&mut self, addl: u8) -> Result<usize, CtapStatus> {
        if addl == 31 {
            return Err(CtapStatus::InvalidCbor); // no indefinite lengths
        }
        let v = self.read_uint(addl)?;
        if v > usize::MAX as u64 {
            return Err(CtapStatus::InvalidLength);
        }
        Ok(v as usize)
    }

    fn major(&mut self) -> Result<(u8, u8), CtapStatus> {
        let b = self.take()?;
        Ok(((b >> 5) & 0x07, b & 0x1f))
    }

    fn slice(&mut self, len: usize) -> Result<&'a [u8], CtapStatus> {
        if self.pos + len > self.data.len() {
            return Err(CtapStatus::InvalidLength);
        }
        let start = self.pos;
        self.pos += len;
        Ok(&self.data[start..start + len])
    }

    pub fn map(&mut self) -> Result<usize, CtapStatus> {
        let (maj, addl) = self.major()?;
        if maj != 5 {
            return Err(CtapStatus::CborUnexpectedType);
        }
        self.read_len(addl)
    }

    pub fn array(&mut self) -> Result<usize, CtapStatus> {
        let (maj, addl) = self.major()?;
        if maj != 4 {
            return Err(CtapStatus::CborUnexpectedType);
        }
        self.read_len(addl)
    }

    pub fn u32(&mut self) -> Result<u32, CtapStatus> {
        let (maj, addl) = self.major()?;
        if maj != 0 {
            return Err(CtapStatus::CborUnexpectedType);
        }
        let v = self.read_uint(addl)?;
        if v > u32::MAX as u64 {
            return Err(CtapStatus::InvalidLength);
        }
        Ok(v as u32)
    }

    pub fn nint(&mut self) -> Result<i32, CtapStatus> {
        let (maj, addl) = self.major()?;
        if maj != 1 {
            return Err(CtapStatus::CborUnexpectedType);
        }
        let magnitude = self.read_uint(addl)?;
        if magnitude > i32::MAX as u64 {
            return Err(CtapStatus::InvalidParameter);
        }
        Ok(-1 - magnitude as i32)
    }

    pub fn tstr(&mut self) -> Result<&'a str, CtapStatus> {
        let (maj, addl) = self.major()?;
        if maj != 3 {
            return Err(CtapStatus::CborUnexpectedType);
        }
        let len = self.read_len(addl)?;
        let data = self.slice(len)?;
        str::from_utf8(data).map_err(|_| CtapStatus::InvalidCbor)
    }

    pub fn bstr(&mut self) -> Result<&'a [u8], CtapStatus> {
        let (maj, addl) = self.major()?;
        if maj != 2 {
            return Err(CtapStatus::CborUnexpectedType);
        }
        let len = self.read_len(addl)?;
        self.slice(len)
    }

    pub fn bool(&mut self) -> Result<bool, CtapStatus> {
        match self.major()? {
            (7, 20) => Ok(false),
            (7, 21) => Ok(true),
            _ => Err(CtapStatus::CborUnexpectedType),
        }
    }

    pub fn skip(&mut self) -> Result<(), CtapStatus> {
        let (maj, addl) = self.major()?;
        match maj {
            0 | 1 => {
                if addl >= 24 {
                    self.read_uint(addl)?;
                }
                Ok(())
            }
            2 | 3 => {
                let len = self.read_len(addl)?;
                self.slice(len).map(|_| ())
            }
            4 => {
                let len = self.read_len(addl)?;
                for _ in 0..len {
                    self.skip()?;
                }
                Ok(())
            }
            5 => {
                let len = self.read_len(addl)?;
                for _ in 0..len {
                    self.skip()?;
                    self.skip()?;
                }
                Ok(())
            }
            6 => {
                // tag, then value
                self.read_uint(addl)?;
                self.skip()
            }
            7 => match addl {
                20 | 21 | 22 | 23 => Ok(()),
                24 => {
                    self.take()?;
                    Ok(())
                }
                _ => Err(CtapStatus::CborUnexpectedType),
            },
            _ => Err(CtapStatus::InvalidCbor),
        }
    }
}
