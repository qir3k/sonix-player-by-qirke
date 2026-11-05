//! SHA-256 (FIPS 180-4), incremental, ported from
//! `src/system/core/sha256.c`.
//!
//! The firmware image is hashed a piece at a time as it is downloaded, so the
//! context is part of the public surface: `init`, `update` as many times as
//! the network hands over bytes, then `finalize_hex`.
//!
//! ## The layout is part of the ABI
//!
//! `Ctx` is `#[repr(C)]` and field-for-field the same as the C `sha256_ctx`,
//! down to `size_t` (`usize` here). That is not vanity: `sonix-ffi` hands the
//! same struct to C callers by pointer, so a C `sha256_ctx` on the stack is a
//! valid Rust `Ctx` and neither side has to copy anything. Keep the field
//! order and types as they are.

pub const DIGEST_LEN: usize = 32;

const K: [u32; 64] = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
];

const IV: [u32; 8] = [
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
];

fn block(c: &mut Ctx, p: &[u8]) {
    debug_assert_eq!(p.len(), 64);
    let mut w = [0u32; 64];

    for i in 0..16 {
        w[i] = (u32::from(p[i * 4]) << 24)
            | (u32::from(p[i * 4 + 1]) << 16)
            | (u32::from(p[i * 4 + 2]) << 8)
            | u32::from(p[i * 4 + 3]);
    }
    for i in 16..64 {
        let s0 = w[i - 15].rotate_right(7) ^ w[i - 15].rotate_right(18) ^ (w[i - 15] >> 3);
        let s1 = w[i - 2].rotate_right(17) ^ w[i - 2].rotate_right(19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16]
            .wrapping_add(s0)
            .wrapping_add(w[i - 7])
            .wrapping_add(s1);
    }

    let mut a = c.h[0];
    let mut b = c.h[1];
    let mut cc = c.h[2];
    let mut d = c.h[3];
    let mut e = c.h[4];
    let mut f = c.h[5];
    let mut g = c.h[6];
    let mut h = c.h[7];

    for (i, word) in w.iter().enumerate() {
        let s1 = e.rotate_right(6) ^ e.rotate_right(11) ^ e.rotate_right(25);
        let ch = (e & f) ^ (!e & g);
        let t1 = h
            .wrapping_add(s1)
            .wrapping_add(ch)
            .wrapping_add(K[i])
            .wrapping_add(*word);
        let s0 = a.rotate_right(2) ^ a.rotate_right(13) ^ a.rotate_right(22);
        let maj = (a & b) ^ (a & cc) ^ (b & cc);
        let t2 = s0.wrapping_add(maj);

        h = g;
        g = f;
        f = e;
        e = d.wrapping_add(t1);
        d = cc;
        cc = b;
        b = a;
        a = t1.wrapping_add(t2);
    }

    c.h[0] = c.h[0].wrapping_add(a);
    c.h[1] = c.h[1].wrapping_add(b);
    c.h[2] = c.h[2].wrapping_add(cc);
    c.h[3] = c.h[3].wrapping_add(d);
    c.h[4] = c.h[4].wrapping_add(e);
    c.h[5] = c.h[5].wrapping_add(f);
    c.h[6] = c.h[6].wrapping_add(g);
    c.h[7] = c.h[7].wrapping_add(h);
}

/// The running state. Field order and types match the C `sha256_ctx`; see the
/// module comment.
#[repr(C)]
#[derive(Clone)]
pub struct Ctx {
    pub h: [u32; 8],
    pub bits: u64,
    pub block: [u8; 64],
    pub have: usize,
}

impl Default for Ctx {
    fn default() -> Self {
        Self::new()
    }
}

impl Ctx {
    pub fn new() -> Self {
        Self {
            h: IV,
            bits: 0,
            block: [0u8; 64],
            have: 0,
        }
    }

    /// Feeds another piece of the message in. Called once per network read.
    pub fn update(&mut self, data: &[u8]) {
        let mut p = data;
        self.bits = self.bits.wrapping_add((p.len() as u64).wrapping_mul(8));

        // Finish the block already started, if one is half full.
        if self.have > 0 {
            let mut take = 64 - self.have;
            if take > p.len() {
                take = p.len();
            }
            self.block[self.have..self.have + take].copy_from_slice(&p[..take]);
            self.have += take;
            p = &p[take..];
            if self.have < 64 {
                return;
            }
            let full = self.block;
            block(self, &full);
            self.have = 0;
        }

        while p.len() >= 64 {
            block(self, &p[..64]);
            p = &p[64..];
        }

        self.block[..p.len()].copy_from_slice(p);
        self.have = p.len();
    }

    /// The digest as lower-case hex. The context is spent afterwards, which the
    /// `self` receiver makes impossible to get wrong.
    pub fn finalize_hex(self) -> String {
        let bits = self.bits;
        let mut c = self;

        // Padding: a 1 bit, zeros up to 56 bytes into a block, the length in
        // bits as a big-endian 64-bit number.
        c.block[c.have] = 0x80;
        c.have += 1;
        if c.have > 56 {
            for byte in &mut c.block[c.have..] {
                *byte = 0;
            }
            let full = c.block;
            block(&mut c, &full);
            c.have = 0;
        }
        for byte in &mut c.block[c.have..56] {
            *byte = 0;
        }
        c.block[56..64].copy_from_slice(&bits.to_be_bytes());
        let full = c.block;
        block(&mut c, &full);

        let mut out = String::with_capacity(64);
        for word in c.h {
            // `{:08x}` of a u32 is the C `%08x` of an unsigned int: eight
            // lower-case hex digits, no sign, no padding surprises.
            out.push_str(&format!("{word:08x}"));
        }
        out
    }
}

/// One-shot convenience for callers that do have the whole message.
pub fn digest_hex(data: &[u8]) -> String {
    let mut c = Ctx::new();
    c.update(data);
    c.finalize_hex()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fips_vectors() {
        assert_eq!(
            digest_hex(b""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        );
        assert_eq!(
            digest_hex(b"abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
        assert_eq!(
            digest_hex(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
        );
    }

    /// The reason this hash is incremental: a firmware image arrives in chunks
    /// and any split must give the same digest as one call.
    #[test]
    fn chunked_equals_one_shot() {
        let data: Vec<u8> = (0..100_000u32).map(|i| (i % 251) as u8).collect();
        let one = digest_hex(&data);

        for split in [1usize, 63, 64, 65, 1000, 65_536] {
            let mut c = Ctx::new();
            for chunk in data.chunks(split) {
                c.update(chunk);
            }
            assert_eq!(c.finalize_hex(), one, "split every {split} bytes");
        }
    }

    /// A 56-byte message is the boundary where the padding needs a second
    /// block; a chunked feed must not touch that decision.
    #[test]
    fn padding_boundaries() {
        for len in [0, 1, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128] {
            let data = vec![0x5Au8; len];
            let one = digest_hex(&data);
            let mut c = Ctx::new();
            for byte in &data {
                c.update(std::slice::from_ref(byte));
            }
            assert_eq!(c.finalize_hex(), one, "length {len} fed a byte at a time");
        }
    }

    #[test]
    fn million_a() {
        let data = vec![b'a'; 1_000_000];
        assert_eq!(
            digest_hex(&data),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"
        );
    }
}
