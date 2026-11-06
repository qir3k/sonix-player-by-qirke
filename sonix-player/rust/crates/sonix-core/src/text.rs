//! Text files off the card, decoded into UTF-8 the interface can draw, ported
//! from `src/system/text/textfile.c`.
//!
//! Two pages need this and neither owns it: the file manager's text viewer,
//! which only reads, and the note editor, which reads and writes. The rules are
//! exactly what that page reads files by, so a note and a `.txt` opened from the
//! browser come out the same.
//!
//! UTF-8 (with or without a byte order mark) and UTF-16 with a byte order mark
//! are read as such; anything else that is not valid UTF-8 is taken for
//! Windows-1252, which is what a file saved on Windows as "ANSI" is. Line
//! endings become `\n`, tabs become four spaces, and other control characters
//! are left out — a NUL, an escape or a stray byte from a file saved as RTF
//! would otherwise be drawn as a box or cut the line short.

/// Windows-1252 from 0x80 to 0x9F; 0 where the code page leaves a hole. From
/// 0xA0 up it is Latin-1, the same numbers as Unicode.
const CP1252_HIGH: [u16; 32] = [
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039,
    0x0152, 0, 0x017D, 0, 0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC,
    0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
];

/// Appends `cp` as UTF-8.
fn put_utf8(out: &mut Vec<u8>, cp: u32) {
    if cp < 0x80 {
        out.push(cp as u8);
    } else if cp < 0x800 {
        out.push((0xC0 | (cp >> 6)) as u8);
        out.push((0x80 | (cp & 0x3F)) as u8);
    } else if cp < 0x10000 {
        out.push((0xE0 | (cp >> 12)) as u8);
        out.push((0x80 | ((cp >> 6) & 0x3F)) as u8);
        out.push((0x80 | (cp & 0x3F)) as u8);
    } else {
        out.push((0xF0 | (cp >> 18)) as u8);
        out.push((0x80 | ((cp >> 12) & 0x3F)) as u8);
        out.push((0x80 | ((cp >> 6) & 0x3F)) as u8);
        out.push((0x80 | (cp & 0x3F)) as u8);
    }
}

/// The length of the valid UTF-8 sequence at the start of `p`, or 0 when there
/// is none. A sequence cut by the end of the buffer counts as valid: the file
/// was read up to a limit, not to its end.
fn utf8_len(p: &[u8]) -> usize {
    if p.is_empty() {
        return 0;
    }
    let b0 = p[0];
    let need = if b0 < 0x80 {
        1
    } else if (b0 & 0xE0) == 0xC0 {
        2
    } else if (b0 & 0xF0) == 0xE0 {
        3
    } else if (b0 & 0xF8) == 0xF0 {
        4
    } else {
        0
    };
    if need == 0 || (need == 2 && b0 < 0xC2) {
        return 0;
    }
    for i in 1..need {
        if i >= p.len() {
            return p.len();
        }
        if (p[i] & 0xC0) != 0x80 {
            return 0;
        }
    }
    need
}

fn is_utf8(p: &[u8]) -> bool {
    let mut i = 0;
    while i < p.len() {
        let n = utf8_len(&p[i..]);
        if n == 0 {
            return false;
        }
        i += n;
    }
    true
}

/// Adds one code point to `out`, with line endings made `\n`, tabs made four
/// spaces and other control characters left out.
fn put_char(out: &mut Vec<u8>, cp: u32, prev: &mut u32) {
    let before = *prev;
    *prev = cp;

    match cp {
        0x0D => {
            // A carriage return is a line ending on its own.
            out.push(b'\n');
            return;
        }
        0x0A => {
            if before == 0x0D {
                return; // the second half of "\r\n"
            }
            out.push(b'\n');
            return;
        }
        0x09 => {
            out.extend_from_slice(b"    ");
            return;
        }
        _ => {}
    }

    if cp < 0x20 || cp == 0x7F || cp == 0xFEFF {
        return;
    }
    put_utf8(out, cp);
}

/// Decodes `raw` into UTF-8.
///
/// Every input byte becomes at most four output bytes, so `len * 4 + 1` is
/// always enough; `Vec` handles the growth and the terminator is the caller's
/// business.
pub fn decode(raw: &[u8]) -> Vec<u8> {
    let len = raw.len();
    let mut out: Vec<u8> = Vec::with_capacity(len.saturating_mul(4).saturating_add(1));
    let mut prev = 0u32;

    let le = len >= 2 && raw[0] == 0xFF && raw[1] == 0xFE;
    let be = len >= 2 && raw[0] == 0xFE && raw[1] == 0xFF;

    if le || be {
        let mut i = 2;
        while i + 1 < len {
            let mut unit = if le {
                u32::from(raw[i]) | (u32::from(raw[i + 1]) << 8)
            } else {
                (u32::from(raw[i]) << 8) | u32::from(raw[i + 1])
            };

            if (0xD800..0xDC00).contains(&unit) && i + 3 < len {
                let low = if le {
                    u32::from(raw[i + 2]) | (u32::from(raw[i + 3]) << 8)
                } else {
                    (u32::from(raw[i + 2]) << 8) | u32::from(raw[i + 3])
                };
                if (0xDC00..0xE000).contains(&low) {
                    unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                    i += 2;
                }
            }

            // An unpaired surrogate is dropped rather than drawn as a box.
            if !(0xD800..0xE000).contains(&unit) {
                put_char(&mut out, unit, &mut prev);
            }
            i += 2;
        }
    } else if is_utf8(raw) {
        let mut i = 0;
        while i < len {
            let n = utf8_len(&raw[i..]);
            if n == 0 {
                break;
            }
            if raw[i] < 0x80 {
                put_char(&mut out, u32::from(raw[i]), &mut prev);
            } else if n == 3 && raw[i] == 0xEF && raw[i + 1] == 0xBB && raw[i + 2] == 0xBF {
                prev = 0; // a byte order mark
            } else {
                out.extend_from_slice(&raw[i..i + n]);
                prev = 0x80;
            }
            i += n;
        }
    } else {
        for &byte in raw {
            let cp = if byte < 0x80 {
                u32::from(byte)
            } else if byte < 0xA0 {
                u32::from(CP1252_HIGH[usize::from(byte - 0x80)])
            } else {
                u32::from(byte)
            };
            if cp != 0 {
                put_char(&mut out, cp, &mut prev);
            }
        }
    }

    out
}

/// The same, as a `String`, for callers that want text rather than bytes.
pub fn decode_to_string(raw: &[u8]) -> String {
    String::from_utf8_lossy(&decode(raw)).into_owned()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn decoded(raw: &[u8]) -> String {
        decode_to_string(raw)
    }

    #[test]
    fn plain_ascii_passes_through() {
        assert_eq!(decoded(b"hello world"), "hello world");
    }

    #[test]
    fn line_endings_become_newlines() {
        assert_eq!(decoded(b"a\r\nb"), "a\nb", "CRLF is one newline");
        assert_eq!(decoded(b"a\rb"), "a\nb", "a lone CR is a newline too");
        assert_eq!(decoded(b"a\nb"), "a\nb");
        assert_eq!(decoded(b"a\r\r\nb"), "a\n\nb");
    }

    #[test]
    fn tabs_become_four_spaces() {
        assert_eq!(decoded(b"a\tb"), "a    b");
    }

    #[test]
    fn control_characters_are_dropped() {
        assert_eq!(decoded(b"a\x00b\x1bc\x7fd"), "abcd");
        assert_eq!(decoded(b"\x08backspace"), "backspace");
    }

    #[test]
    fn utf8_with_a_bom_loses_it() {
        assert_eq!(decoded(b"\xEF\xBB\xBFhello"), "hello");
        // A BOM in the middle is stripped as well, which is what the C does.
        assert_eq!(decoded(b"a\xEF\xBB\xBFb"), "ab");
    }

    #[test]
    fn utf8_multibyte_survives() {
        assert_eq!(decoded("héllo wörld".as_bytes()), "héllo wörld");
        assert_eq!(decoded("世界".as_bytes()), "世界");
        assert_eq!(decoded("🎵 note".as_bytes()), "🎵 note");
    }

    #[test]
    fn utf16_little_endian() {
        let mut raw = vec![0xFF, 0xFE];
        for unit in "hello".encode_utf16() {
            raw.extend_from_slice(&unit.to_le_bytes());
        }
        assert_eq!(decoded(&raw), "hello");
    }

    #[test]
    fn utf16_big_endian() {
        let mut raw = vec![0xFE, 0xFF];
        for unit in "hello".encode_utf16() {
            raw.extend_from_slice(&unit.to_be_bytes());
        }
        assert_eq!(decoded(&raw), "hello");
    }

    #[test]
    fn utf16_surrogate_pairs_and_line_endings() {
        let mut raw = vec![0xFF, 0xFE];
        for unit in "🎵\r\nok".encode_utf16() {
            raw.extend_from_slice(&unit.to_le_bytes());
        }
        assert_eq!(decoded(&raw), "🎵\nok");
    }

    #[test]
    fn utf16_unpaired_surrogate_is_dropped() {
        let raw = vec![0xFF, 0xFE, 0x3C, 0xD8, b'a', 0x00];
        assert_eq!(decoded(&raw), "a");
    }

    #[test]
    fn windows_1252_is_the_fallback() {
        // 0x93/0x94 are the "smart quotes" in 1252, and 0xE9 is é.
        assert_eq!(decoded(b"\x93quoted\x94"), "\u{201C}quoted\u{201D}");
        assert_eq!(decoded(b"caf\xE9 x"), "café x");
        // 0x81 is a hole in the code page: dropped rather than guessed at.
        assert_eq!(decoded(b"a\x81b"), "ab");
        // The euro sign.
        assert_eq!(decoded(b"\x80"), "\u{20AC}");
    }

    /// A high byte as the very last byte of the buffer is ambiguous: it could be
    /// the first byte of a character the read limit cut in half, so it is kept
    /// as-is rather than read as Windows-1252. That is the C behaviour, and it
    /// is why a 1252 file whose final byte is accented needs one more byte of
    /// context to be recognised at all.
    #[test]
    fn a_lone_high_byte_at_the_end_is_held_to_be_truncated() {
        assert_eq!(decode(b"caf\xE9"), b"caf\xE9".to_vec());
    }

    /// An invalid UTF-8 sequence is what sends a file down the 1252 path, so a
    /// lone continuation byte is enough to change the reading of every accent
    /// in the file — which is the behaviour being pinned down here.
    #[test]
    fn invalid_utf8_takes_the_whole_file_to_1252() {
        assert_eq!(decoded(b"caf\xE9 ok\x80"), "café ok\u{20AC}");
    }

    #[test]
    fn a_sequence_cut_by_the_read_limit_is_kept() {
        // Two bytes of a three-byte character at the very end: the file was read
        // up to a limit, so this is not corruption and must not send the whole
        // file down the Windows-1252 path.
        let mut raw = b"ab".to_vec();
        raw.extend_from_slice(&"世".as_bytes()[..2]);
        let out = decode(&raw);
        assert_eq!(&out[..2], b"ab");
        assert_eq!(out.len(), 4, "the two clipped bytes are carried through");
    }

    #[test]
    fn empty_input() {
        assert_eq!(decoded(b""), "");
    }

    #[test]
    fn overlong_sequences_are_not_utf8() {
        // 0xC0 0xAF is an overlong '/', which strict UTF-8 rejects; the C
        // version's utf8_len rejects a 2-byte lead below 0xC2 for this reason.
        assert_eq!(decoded(b"\xC0\xAF"), "À¯");
    }
}
