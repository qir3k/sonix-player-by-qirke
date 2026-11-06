//! The chapters inside an MP3's ID3 tag, ported from
//! `src/system/library/id3chap.c`.
//!
//! An audiobook is not always an `.m4b`. A great many are one long `.mp3` with
//! ID3v2 `CHAP` frames in it — that is what every tool that splits an audiobook
//! by chapter writes, and what Audible conversions come out as — and to a player
//! that only reads MP4 chapters those look like a four-hour track with no marks.
//!
//! This reads the `CHAP` frames and nothing else. It is deliberately not a
//! general ID3 parser: `metadata.c` already reads the tags that name a track,
//! and the one thing missing was the chapter list.
//!
//! It is fed files off a card, so every length in the file is treated as a claim
//! and not as a fact: a frame that says it is bigger than what is left is the
//! end of the parse, not a read past the buffer.

use std::io::Read as _;
use std::path::Path;

/// The shape of what is read:
///
/// ```text
/// "ID3" ver(2) flags(1) size(4, syncsafe)     the tag header, 10 bytes
/// frame id(4) size(4) flags(2) payload        repeated to the end of the tag
///
/// a CHAP payload is:
///   element id, NUL-terminated
///   start ms (4), end ms (4), start offset (4), end offset (4)
///   sub-frames, in the same shape as a frame -- TIT2 is the chapter's name
/// ```
///
/// In ID3v2.4 a frame's size is syncsafe (seven bits a byte); in 2.3 it is a
/// plain big-endian number. Getting that wrong reads a frame's length as roughly
/// half of what it is, which walks into the middle of the next frame and finds
/// nothing — so the version is read and honoured rather than assumed.

/// A tag bigger than this is not a tag, it is a file that happens to start with
/// three familiar bytes. Real ones with cover art run to a megabyte.
const MAX_TAG: u32 = 4 * 1024 * 1024;
const MAX_CHAPTERS: usize = 1024;

/// One chapter. Laid out to match the C `id3chap_t`.
#[derive(Copy, Clone)]
#[repr(C)]
pub struct Chapter {
    /// Seconds from the beginning of the file.
    pub start: f64,
    pub title: [u8; 128],
}

impl Default for Chapter {
    fn default() -> Self {
        Chapter {
            start: 0.0,
            title: [0; 128],
        }
    }
}

impl Chapter {
    /// The title as text, lossily — a tag written in a legacy encoding may not
    /// be UTF-8, and a mangled accent is better than no title.
    pub fn title(&self) -> std::borrow::Cow<'_, str> {
        let end = self.title.iter().position(|&b| b == 0).unwrap_or(128);
        String::from_utf8_lossy(&self.title[..end])
    }
}

fn be32(p: &[u8]) -> u32 {
    (u32::from(p[0]) << 24) | (u32::from(p[1]) << 16) | (u32::from(p[2]) << 8) | u32::from(p[3])
}

/// Seven bits a byte, high bit always clear, so a tag can never contain a byte
/// that looks like the start of an MPEG frame.
fn syncsafe32(p: &[u8]) -> u32 {
    (u32::from(p[0] & 0x7F) << 21)
        | (u32::from(p[1] & 0x7F) << 14)
        | (u32::from(p[2] & 0x7F) << 7)
        | u32::from(p[3] & 0x7F)
}

/// One text field out of a sub-frame: an encoding byte and then the text.
///
/// Encodings 1 and 2 are UTF-16 and are read as their low bytes, which is right
/// up to U+00FF and a question mark above it; anything else is copied byte for
/// byte.
fn read_text(data: &[u8], out: &mut [u8]) {
    if out.is_empty() {
        return;
    }
    out[0] = 0;
    if data.is_empty() {
        return;
    }

    let encoding = data[0];
    let text = &data[1..];
    let mut w = 0usize;

    if encoding == 1 || encoding == 2 {
        // UTF-16. Skip a byte-order mark and step two bytes at a time; which of
        // the pair is the character depends on the order.
        let little = encoding == 1 && text.len() >= 2 && text[0] == 0xFF && text[1] == 0xFE;
        // Encoding 1 always steps over its first two bytes, whether or not they
        // are a byte order mark — which is what the C does, and why a
        // little-endian field without one loses its first character's low byte.
        let text = if encoding == 1 && text.len() >= 2 {
            &text[2..]
        } else {
            text
        };

        let mut i = 0usize;
        while i + 1 < text.len() && w + 1 < out.len() {
            let c = if little { text[i] } else { text[i + 1] };
            let hi = if little { text[i + 1] } else { text[i] };
            if c == 0 && hi == 0 {
                break;
            }
            // Above U+00FF there is no single byte to write; a question mark
            // says "a character was here" instead of writing half of one.
            out[w] = if hi != 0 { b'?' } else { c };
            w += 1;
            i += 2;
        }
    } else {
        let mut i = 0usize;
        while i < text.len() && w + 1 < out.len() {
            if text[i] == 0 {
                break;
            }
            out[w] = text[i];
            w += 1;
            i += 1;
        }
    }

    out[w] = 0;

    // Trailing whitespace is common in tags written by hand.
    while w > 0 && matches!(out[w - 1], b' ' | b'\r' | b'\n') {
        w -= 1;
        out[w] = 0;
    }
}

/// The `TIT2` inside a `CHAP`'s payload, after the fixed part.
fn read_chapter_title(payload: &[u8], syncsafe_frames: bool, out: &mut [u8]) {
    if out.is_empty() {
        return;
    }
    out[0] = 0;

    // The element id, which is a NUL-terminated string nobody needs.
    let mut at = 0usize;
    while at < payload.len() && payload[at] != 0 {
        at += 1;
    }
    at += 1; // the NUL itself
             // Four times four bytes of times and offsets.
    if at + 16 > payload.len() {
        return;
    }
    at += 16;

    while at + 10 <= payload.len() {
        let frame = &payload[at..];
        let size = if syncsafe_frames {
            syncsafe32(&frame[4..])
        } else {
            be32(&frame[4..])
        };
        at += 10;
        if size == 0 || size as usize > payload.len() - at {
            return; // a claim bigger than what is left: stop rather than read on
        }
        if &frame[..4] == b"TIT2" {
            read_text(&payload[at..at + size as usize], out);
            return;
        }
        at += size as usize;
    }
}

/// Reads up to `out.len()` chapters into `out`, in the order the file lists
/// them, and returns how many the file has.
///
/// `None` asks only whether there are any — which is what the audiobook scan
/// does, and why that case stops at the first one found.
pub fn read_file(path: &Path, mut out: Option<&mut [Chapter]>) -> usize {
    let count_only = out.is_none();

    let Ok(mut file) = std::fs::File::open(path) else {
        return 0;
    };

    let mut header = [0u8; 10];
    if file.read_exact(&mut header).is_err() || &header[..3] != b"ID3" {
        return 0;
    }
    let version = header[3];
    let mut tag_size = syncsafe32(&header[6..]);
    if tag_size == 0 || tag_size > MAX_TAG {
        return 0;
    }

    let mut tag = vec![0u8; tag_size as usize];
    let mut got = 0usize;
    while got < tag.len() {
        match file.read(&mut tag[got..]) {
            Ok(0) => break,
            Ok(n) => got += n,
            Err(ref e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(_) => break,
        }
    }
    if got == 0 {
        return 0;
    }
    tag.truncate(got); // a truncated file is read as far as it goes
    tag_size = got as u32;

    // An extended header, when the flags say so: its own length comes first.
    let mut at = 0usize;
    if header[5] & 0x40 != 0 {
        if tag_size < 4 {
            return 0;
        }
        let ext = if version >= 4 {
            syncsafe32(&tag)
        } else {
            be32(&tag) + 4
        };
        if ext >= tag_size {
            return 0;
        }
        at = ext as usize;
    }

    let syncsafe_frames = version >= 4;
    let mut found = 0usize;

    while at + 10 <= tag.len() && found < MAX_CHAPTERS {
        let frame = &tag[at..];
        if frame[0] == 0 {
            break; // padding: the rest of the tag is zeroes
        }
        let size = if syncsafe_frames {
            syncsafe32(&frame[4..])
        } else {
            be32(&frame[4..])
        };
        at += 10;
        if size == 0 || size as usize > tag.len() - at {
            break;
        }

        if &frame[..4] == b"CHAP" {
            let payload = &tag[at..at + size as usize];
            // The start time sits after the element id.
            let mut p = 0usize;
            while p < payload.len() && payload[p] != 0 {
                p += 1;
            }
            p += 1;
            if p + 16 <= payload.len() {
                if let Some(slot) = out.as_deref_mut() {
                    if found < slot.len() {
                        let start = be32(&payload[p..]);
                        slot[found].start = f64::from(start) / 1000.0;
                        read_chapter_title(payload, syncsafe_frames, &mut slot[found].title);
                    }
                }
                found += 1;
                if count_only {
                    break; // only asked whether there are any
                }
            }
        }
        at += size as usize;
    }

    found
}

/// True when the file has at least one. Reads no further than it has to.
pub fn present(path: &Path) -> bool {
    read_file(path, None) > 0
}

#[cfg(test)]
mod tests {
    use super::*;

    fn title(ch: &Chapter) -> String {
        ch.title().into_owned()
    }

    #[test]
    fn text_encodings() {
        let mut out = [0u8; 128];
        read_text(b"\x00Hello", &mut out);
        assert_eq!(String::from_utf8_lossy(&out[..5]), "Hello");

        // Trailing spaces and line endings come off.
        read_text(b"\x00Hello  \r\n", &mut out);
        assert_eq!(String::from_utf8_lossy(&out[..5]), "Hello");

        // A NUL ends a single-byte field.
        read_text(b"\x00one\x00two", &mut out);
        assert_eq!(String::from_utf8_lossy(&out[..3]), "one");
    }

    #[test]
    fn utf16_little_endian_with_bom() {
        let mut out = [0u8; 128];
        let mut data = vec![1u8, 0xFF, 0xFE];
        for c in "Hi".encode_utf16() {
            data.extend_from_slice(&c.to_le_bytes());
        }
        read_text(&data, &mut out);
        assert_eq!(String::from_utf8_lossy(&out[..2]), "Hi");
    }

    #[test]
    fn utf16_big_endian() {
        let mut out = [0u8; 128];
        let mut data = vec![2u8];
        for c in "Yo".encode_utf16() {
            data.extend_from_slice(&c.to_be_bytes());
        }
        read_text(&data, &mut out);
        assert_eq!(String::from_utf8_lossy(&out[..2]), "Yo");
    }

    /// Encoding 1 steps over its first two bytes whether or not they are a byte
    /// order mark, and encoding 2 never steps over anything — so a big-endian
    /// field that *does* start with one reads the mark as two characters that
    /// have no single-byte form. Worth pinning down: it is what the C does, and
    /// the differential harness confirms it.
    #[test]
    fn the_byte_order_mark_rule_is_encoding_one_only() {
        let mut out = [0u8; 128];
        read_text(&[2, 0xFE, 0xFF, 0x00, b'Y', 0x00, b'o'], &mut out);
        assert_eq!(String::from_utf8_lossy(&out[..3]), "?Yo");

        // Encoding 1 without a BOM loses its first two bytes outright, and the
        // rest is then read from the wrong half of every pair: 'B' arrives as
        // the high byte of a pair whose low byte is the terminator.
        let mut out2 = [0u8; 128];
        read_text(&[1, b'A', 0, b'B', 0], &mut out2);
        assert_eq!(String::from_utf8_lossy(&out2[..1]), "?");
    }

    #[test]
    fn utf16_beyond_latin_one_becomes_a_question_mark() {
        let mut out = [0u8; 128];
        // 0x4E16 is 世: two non-zero bytes, so one '?'.
        read_text(&[2, 0x4E, 0x16, 0x00, b'A'], &mut out);
        assert_eq!(String::from_utf8_lossy(&out[..2]), "?A");
    }

    #[test]
    fn an_empty_field_is_an_empty_string() {
        let mut out = [0x55u8; 8];
        read_text(b"", &mut out);
        assert_eq!(out[0], 0);
        read_text(b"\x00", &mut out);
        assert_eq!(out[0], 0);
    }

    #[test]
    fn a_missing_file_is_no_chapters() {
        assert_eq!(read_file(Path::new("does-not-exist.mp3"), None), 0);
        let mut out = [Chapter::default(); 4];
        assert_eq!(
            read_file(Path::new("does-not-exist.mp3"), Some(&mut out)),
            0
        );
    }

    #[test]
    fn layout_matches_the_c_struct() {
        assert_eq!(core::mem::size_of::<Chapter>(), 136);
        assert_eq!(core::mem::offset_of!(Chapter, title), 8);
    }
}
