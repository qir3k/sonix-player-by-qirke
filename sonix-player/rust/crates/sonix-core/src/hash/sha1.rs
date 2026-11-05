//! SHA-1 (FIPS 180-1), ported from `src/system/core/sha1.c`.
//!
//! Podcast Index signs every request with it. As with the MD5 next door this
//! is not a choice made here and it protects nothing of this project's; it
//! demonstrates knowledge of a secret, which SHA-1 is still adequate for.
//! Nothing in the player hands it anything that must withstand an attacker.

pub const DIGEST_LEN: usize = 20;

const INIT: [u32; 5] = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0];

fn block(h: &mut [u32; 5], p: &[u8]) {
    debug_assert_eq!(p.len(), 64);
    let mut w = [0u32; 80];

    // The first sixteen are the block's bytes, big-endian as the standard
    // requires; the other sixty-four are derived from those.
    for i in 0..16 {
        w[i] = (u32::from(p[i * 4]) << 24)
            | (u32::from(p[i * 4 + 1]) << 16)
            | (u32::from(p[i * 4 + 2]) << 8)
            | u32::from(p[i * 4 + 3]);
    }
    for i in 16..80 {
        w[i] = (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]).rotate_left(1);
    }

    let mut a = h[0];
    let mut b = h[1];
    let mut c = h[2];
    let mut d = h[3];
    let mut e = h[4];

    for (i, word) in w.iter().enumerate() {
        let (mix, k) = if i < 20 {
            ((b & c) | (!b & d), 0x5A82_7999u32)
        } else if i < 40 {
            (b ^ c ^ d, 0x6ED9_EBA1u32)
        } else if i < 60 {
            ((b & c) | (b & d) | (c & d), 0x8F1B_BCDCu32)
        } else {
            (b ^ c ^ d, 0xCA62_C1D6u32)
        };

        let t = a
            .rotate_left(5)
            .wrapping_add(mix)
            .wrapping_add(e)
            .wrapping_add(k)
            .wrapping_add(*word);
        e = d;
        d = c;
        c = b.rotate_left(30);
        b = a;
        a = t;
    }

    h[0] = h[0].wrapping_add(a);
    h[1] = h[1].wrapping_add(b);
    h[2] = h[2].wrapping_add(c);
    h[3] = h[3].wrapping_add(d);
    h[4] = h[4].wrapping_add(e);
}

/// The digest of `data`.
pub fn digest(data: &[u8]) -> [u8; DIGEST_LEN] {
    let mut h = INIT;

    let mut chunks = data.chunks_exact(64);
    for chunk in &mut chunks {
        block(&mut h, chunk);
    }

    // Padding: a one bit, then zeros until eight bytes are left, then the
    // length in bits. If they do not fit, this block is closed and another one
    // made entirely of padding follows — 64 or 128 bytes, as the count needs.
    let rest = chunks.remainder();
    let mut tail = [0u8; 128];
    tail[..rest.len()].copy_from_slice(rest);
    tail[rest.len()] = 0x80;

    let total = if rest.len() < 56 { 64 } else { 128 };
    let bits = (data.len() as u64).wrapping_mul(8);
    tail[total - 8..total].copy_from_slice(&bits.to_be_bytes());

    block(&mut h, &tail[..64]);
    if total == 128 {
        block(&mut h, &tail[64..]);
    }

    let mut out = [0u8; DIGEST_LEN];
    for (i, word) in h.iter().enumerate() {
        out[i * 4..i * 4 + 4].copy_from_slice(&word.to_be_bytes());
    }
    out
}

/// The same, written as lower-case hex: the form the signature takes in the
/// Authorization header.
pub fn hex(data: &[u8]) -> String {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let mut out = String::with_capacity(DIGEST_LEN * 2);
    for byte in digest(data) {
        out.push(HEX[usize::from(byte >> 4)] as char);
        out.push(HEX[usize::from(byte & 0x0F)] as char);
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    /// FIPS 180-1 / 180-2 appendix vectors, plus the classic empty input.
    #[test]
    fn fips_vectors() {
        assert_eq!(hex(b""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
        assert_eq!(hex(b"abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
        assert_eq!(
            hex(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "84983e441c3bd26ebaae4aa1f95129e5e54670f1"
        );
    }

    /// The million-'a' vector, which is the one that exercises the block loop.
    #[test]
    fn million_a() {
        let data = vec![b'a'; 1_000_000];
        assert_eq!(hex(&data), "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    }

    /// 55/56 is the one-or-two-tail-block boundary.
    #[test]
    fn tail_block_boundary() {
        let a = hex(&[b'x'; 55]);
        let b = hex(&[b'x'; 56]);
        assert_ne!(a, b);
        assert_eq!(a.len(), 40);
        assert_eq!(b.len(), 40);
    }

    /// The form Podcast Index signs with, so the digest really is big-endian
    /// on the wire and not just in the tests above.
    #[test]
    fn digest_layout_is_big_endian() {
        assert_eq!(
            digest(b"abc")[..4],
            [0xa9, 0x99, 0x3e, 0x36],
            "first word of SHA-1(\"abc\") is a9993e36"
        );
    }
}
