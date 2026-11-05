//! MD5 (RFC 1321), ported from `src/system/core/md5.c`.
//!
//! Not a choice made here, and not a defensible one in 2026 — MD5 has been
//! broken for collisions for twenty years — but it protects nothing of this
//! project's: it is the lock Qobuz puts on its own door, and getting in means
//! holding the key it asks for.
//!
//! The K constants are `floor(2^32 * |sin(i+1)|)` and the S shifts are the
//! specification's. Both are tabulated rather than computed, because a table
//! can be checked against the document and a `sin()` formula cannot.

pub const DIGEST_LEN: usize = 16;

const K: [u32; 64] = [
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
];

const S: [u32; 64] = [
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, //
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, //
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, //
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
];

const INIT: [u32; 4] = [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476];

/// One 64-byte block.
///
/// The words are assembled little-endian a byte at a time. The C version was
/// written that way on purpose: on MIPS an unaligned access is not merely
/// slow, and the input is an arbitrary string, not a 4-byte-aligned buffer.
fn block(state: &mut [u32; 4], block: &[u8]) {
    debug_assert_eq!(block.len(), 64);

    let mut m = [0u32; 16];
    for (i, word) in m.iter_mut().enumerate() {
        let b = &block[i * 4..i * 4 + 4];
        *word = u32::from(b[0])
            | (u32::from(b[1]) << 8)
            | (u32::from(b[2]) << 16)
            | (u32::from(b[3]) << 24);
    }

    let [mut a, mut b, mut c, mut d] = *state;

    for i in 0..64 {
        let (f, g) = if i < 16 {
            ((b & c) | (!b & d), i)
        } else if i < 32 {
            ((d & b) | (!d & c), (5 * i + 1) % 16)
        } else if i < 48 {
            (b ^ c ^ d, (3 * i + 5) % 16)
        } else {
            (c ^ (b | !d), (7 * i) % 16)
        };

        let temp = d;
        d = c;
        c = b;
        b = b.wrapping_add(
            a.wrapping_add(f)
                .wrapping_add(K[i])
                .wrapping_add(m[g])
                .rotate_left(S[i]),
        );
        a = temp;
    }

    state[0] = state[0].wrapping_add(a);
    state[1] = state[1].wrapping_add(b);
    state[2] = state[2].wrapping_add(c);
    state[3] = state[3].wrapping_add(d);
}

/// The digest of `data`.
pub fn digest(data: &[u8]) -> [u8; DIGEST_LEN] {
    let mut state = INIT;

    let mut chunks = data.chunks_exact(64);
    for chunk in &mut chunks {
        block(&mut state, chunk);
    }

    // The tail: the remainder, a 0x80 byte, zeros up to offset 56, and the
    // length in bits as a 64-bit little-endian number. A remainder above 55
    // needs two blocks — the case that is always got wrong — so the buffer
    // holds 128 bytes and one or two blocks are used as the count requires.
    let rest = chunks.remainder();
    let mut tail = [0u8; 128];
    tail[..rest.len()].copy_from_slice(rest);
    tail[rest.len()] = 0x80;

    let total = if rest.len() < 56 { 64 } else { 128 };
    let bits = (data.len() as u64).wrapping_mul(8);
    tail[total - 8..total].copy_from_slice(&bits.to_le_bytes());

    block(&mut state, &tail[..64]);
    if total == 128 {
        block(&mut state, &tail[64..]);
    }

    let mut out = [0u8; DIGEST_LEN];
    for (i, word) in state.iter().enumerate() {
        out[i * 4..i * 4 + 4].copy_from_slice(&word.to_le_bytes());
    }
    out
}

/// The same, written as lower-case hex: the form the signature takes in the
/// Qobuz URL.
pub fn hex(data: &[u8]) -> String {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let digest = digest(data);
    let mut out = String::with_capacity(DIGEST_LEN * 2);
    for byte in digest {
        out.push(HEX[usize::from(byte >> 4)] as char);
        out.push(HEX[usize::from(byte & 0x0F)] as char);
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The seven test vectors from RFC 1321, appendix A.5.
    #[test]
    fn rfc1321_vectors() {
        assert_eq!(hex(b""), "d41d8cd98f00b204e9800998ecf8427e");
        assert_eq!(hex(b"a"), "0cc175b9c0f1b6a831c399e269772661");
        assert_eq!(hex(b"abc"), "900150983cd24fb0d6963f7d28e17f72");
        assert_eq!(hex(b"message digest"), "f96b697d7cb7938d525a2f31aaf161d0");
        assert_eq!(
            hex(b"abcdefghijklmnopqrstuvwxyz"),
            "c3fcd3d76192e4007dfb496cca67e13b"
        );
        assert_eq!(
            hex(b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
            "d174ab98d277d9f5a5611c2c9f419d9f"
        );
        assert_eq!(
            hex(
                b"12345678901234567890123456789012345678901234567890123456789012345678901234567890"
            ),
            "57edf4a22be3c955ac49da2e2107b67a"
        );
    }

    /// 55 and 56 bytes are the boundary between one tail block and two; the C
    /// version comments on it, so it is worth pinning down.
    #[test]
    fn tail_block_boundary() {
        let a = [b'x'; 55];
        let b = [b'x'; 56];
        let c = [b'x'; 64];
        assert_eq!(hex(&a).len(), 32);
        assert_eq!(hex(&b).len(), 32);
        assert_ne!(hex(&a), hex(&b));
        assert_eq!(hex(&c), hex(&[b'x'; 64]));
    }

    #[test]
    fn digest_layout_is_little_endian() {
        let d = digest(b"abc");
        assert_eq!(
            d,
            [
                0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1,
                0x7f, 0x72
            ]
        );
    }
}
