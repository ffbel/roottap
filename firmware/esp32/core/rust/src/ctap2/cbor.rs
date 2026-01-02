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
        if v < 24 {
            self.push(0b000_00000 | v)
        } else {
            self.push(0b000_11000)?; // additional = 24 (one byte follows)
            self.push(v)
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
}
