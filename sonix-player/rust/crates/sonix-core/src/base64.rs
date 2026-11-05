//! Base64 decoding, ported from `src/system/core/base64.c`.
//!
//! Tidal wraps the important things in base64: the response that says where a
//! track is does not carry the URL, it carries a `manifest` field holding
//! either a JSON with the URL or a DASH MPD. Without decoding it nothing plays.
//!
//! The C version is lenient in a way that is not an accident: whitespace,
//! padding and garbage are skipped rather than failing the decode, and the
//! URL-safe alphabet (`-` and `_`) is accepted alongside the standard one.
//! Both behaviours are kept here, because OAuth tokens and Tidal manifests are
//! the two inputs this function exists for and they disagree about the
//! alphabet.

/// Characters that carry bits, or `None` for anything to be skipped.
fn value_of(c: u8) -> Option<u32> {
    match c {
        b'A'..=b'Z' => Some(u32::from(c - b'A')),
        b'a'..=b'z' => Some(u32::from(c - b'a') + 26),
        b'0'..=b'9' => Some(u32::from(c - b'0') + 52),
        // '+' and '/' from the standard alphabet, '-' and '_' from the
        // URL-safe one. Accepting all four costs two arms and removes a whole
        // class of silent decode failures.
        b'+' | b'-' => Some(62),
        b'/' | b'_' => Some(63),
        // Padding, whitespace or garbage: skipped.
        _ => None,
    }
}

/// Decodes `text`, refusing anything that would produce more than
/// `max_bytes` bytes.
///
/// `None` when nothing decodes, when the worst case would exceed `max_bytes`,
/// or when the input is empty. The cap is checked *before* allocating, against
/// the worst-case output length (four characters make three bytes), so an
/// arbitrarily long input cannot decide how much memory to take on a device
/// with 64 MB in total.
pub fn decode(text: &[u8], max_bytes: usize) -> Option<Vec<u8>> {
    if text.is_empty() {
        return None;
    }

    let worst = text.len() / 4 * 3 + 3;
    if worst > max_bytes {
        return None;
    }

    let mut out = Vec::with_capacity(worst);
    let mut accum: u32 = 0;
    let mut bits: u32 = 0;

    for &c in text {
        let Some(v) = value_of(c) else { continue };
        accum = (accum << 6) | v;
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            out.push(((accum >> bits) & 0xFF) as u8);
        }
    }

    if out.is_empty() {
        return None;
    }

    Some(out)
}

/// The same, for the common case where the caller has a `&str` and wants the
/// result as text. The C version handed back a NUL-terminated buffer for
/// exactly this reason: a Tidal manifest is JSON or XML and its recipient
/// wants a terminated string.
pub fn decode_to_string(text: &str, max_bytes: usize) -> Option<String> {
    let bytes = decode(text.as_bytes(), max_bytes)?;
    String::from_utf8(bytes).ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn standard_alphabet() {
        assert_eq!(decode(b"aGVsbG8=", 64).unwrap(), b"hello");
        assert_eq!(
            decode(b"SGVsbG8sIFdvcmxkIQ==", 64).unwrap(),
            b"Hello, World!"
        );
    }

    #[test]
    fn url_safe_alphabet_is_accepted() {
        // 0xFB 0xFF encodes as "+/8=" in the standard alphabet and "-_8=" in
        // the URL-safe one; both must decode to the same two bytes, because a
        // Tidal manifest and an OAuth token disagree about which they use.
        assert_eq!(decode(b"+/8=", 8).unwrap(), vec![0xFB, 0xFF]);
        assert_eq!(decode(b"-_8=", 8).unwrap(), vec![0xFB, 0xFF]);
    }

    #[test]
    fn whitespace_and_padding_are_skipped() {
        let wrapped = b"SGVs\r\nbG8s\r\nIFdv\r\ncmxkIQ==";
        assert_eq!(decode(wrapped, 64).unwrap(), b"Hello, World!");
    }

    #[test]
    fn unpadded_input_still_decodes() {
        assert_eq!(decode(b"aGVsbG8", 64).unwrap(), b"hello");
    }

    #[test]
    fn cap_is_checked_before_decoding() {
        // 12 characters is a worst case of 12 bytes; ask for 4 and it is
        // refused outright rather than decoded and then rejected.
        assert!(decode(b"SGVsbG8sIQ==", 4).is_none());
        assert_eq!(decode(b"SGVsbG8sIQ==", 32).unwrap(), b"Hello,!");
    }

    #[test]
    fn nothing_decodable_is_none() {
        assert!(decode(b"", 64).is_none());
        assert!(decode(b"====", 64).is_none());
        assert!(decode(b" \r\n\t", 64).is_none());
    }

    #[test]
    fn manifest_sized_payload_round_trips_as_text() {
        let payload = "{\"urls\":[\"https://sp-pr-fa.audio.tidal.com/mediatracks/1.flac\"]}";
        // Encode with the standard alphabet by hand so the test does not
        // depend on an encoder this module deliberately does not have.
        let encoded = {
            const ALPHA: &[u8; 64] =
                b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            let src = payload.as_bytes();
            let mut s = String::new();
            for chunk in src.chunks(3) {
                let b = [
                    chunk[0],
                    *chunk.get(1).unwrap_or(&0),
                    *chunk.get(2).unwrap_or(&0),
                ];
                let n = (u32::from(b[0]) << 16) | (u32::from(b[1]) << 8) | u32::from(b[2]);
                s.push(ALPHA[(n >> 18) as usize & 63] as char);
                s.push(ALPHA[(n >> 12) as usize & 63] as char);
                s.push(if chunk.len() > 1 {
                    ALPHA[(n >> 6) as usize & 63] as char
                } else {
                    '='
                });
                s.push(if chunk.len() > 2 {
                    ALPHA[n as usize & 63] as char
                } else {
                    '='
                });
            }
            s
        };
        assert_eq!(decode_to_string(&encoded, 4096).unwrap(), payload);
    }
}
