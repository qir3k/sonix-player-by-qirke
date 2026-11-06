//! CUE sheets and WAV track markers, ported from `src/system/library/cue.c`.
//!
//! A CD ripped as one long file plus a `.cue` sheet: the sheet says where each
//! track starts inside that file. A WAV can also carry its own list in RIFF
//! markers, and then it stands in for the sheet.
//!
//! The fields are byte buffers rather than `String`s on purpose. The C copies
//! into fixed `char` arrays *while parsing* — a 200-byte TITLE is cut to 159
//! bytes before its trailing spaces are trimmed — so cutting afterwards would
//! give a different answer on exactly the files this parser exists to survive.
//! Keeping the buffers means the struct the C ABI hands back is the same bytes,
//! padding included.

use std::fs::{self, File};
use std::io::{BufReader, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};

/// A sheet holds ninety-nine tracks, and so does a WAV's marker list.
pub const MAX_TRACKS: usize = 99;

const LINE_MAX: usize = 1024;
const MAX_LINES: i64 = 20_000;
/// 100000 minutes is about 69 days of audio: past this the timestamp is not a
/// timestamp, and stopping here keeps the millisecond maths far from overflow.
const MAX_MINUTES: u64 = 100_000;

const WAV_MAX_CHUNK_WALK: usize = 4096;
/// The shortest stretch between two markers that is still a track: four
/// seconds, the Red Book minimum.
const WAV_MIN_TRACK_MS: u64 = 4000;

const TITLE_MAX: usize = 160;
const PERFORMER_MAX: usize = 160;
const GENRE_MAX: usize = 96;
const PATH_MAX: usize = 512;

/// One track of a sheet, laid out as `cue_track_t`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Track {
    /// The TRACK number as written.
    pub number: i32,
    pub title: [u8; TITLE_MAX],
    pub performer: [u8; PERFORMER_MAX],
    /// From INDEX 01 (INDEX 00 is the pre-gap: ignore it).
    pub begin_ms: u64,
    /// Where the next track starts; 0 on the last track.
    pub end_ms: u64,
}

impl Track {
    const fn zeroed() -> Self {
        Track {
            number: 0,
            title: [0; TITLE_MAX],
            performer: [0; PERFORMER_MAX],
            begin_ms: 0,
            end_ms: 0,
        }
    }
}

/// A parsed sheet, laid out as `cue_sheet_t`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Sheet {
    /// Absolute path of the file FILE points at.
    pub audio_path: [u8; PATH_MAX],
    /// The sheet's TITLE.
    pub album: [u8; TITLE_MAX],
    /// The sheet's PERFORMER.
    pub performer: [u8; PERFORMER_MAX],
    /// From REM GENRE, empty if absent.
    pub genre: [u8; GENRE_MAX],
    /// From REM DATE, 0 if absent.
    pub year: i32,
    /// From REM DISCNUMBER, 0 if absent.
    pub disc: i32,
    pub track_count: i32,
    pub tracks: [Track; MAX_TRACKS],
}

impl Sheet {
    pub const fn zeroed() -> Self {
        Sheet {
            audio_path: [0; PATH_MAX],
            album: [0; TITLE_MAX],
            performer: [0; PERFORMER_MAX],
            genre: [0; GENRE_MAX],
            year: 0,
            disc: 0,
            track_count: 0,
            tracks: [Track::zeroed(); MAX_TRACKS],
        }
    }

    fn clear(&mut self) {
        *self = Sheet::zeroed();
    }

    /// A field as text, for callers and tests that want one.
    pub fn text(field: &[u8]) -> String {
        let end = c_len(field);
        String::from_utf8_lossy(&field[..end]).into_owned()
    }
}

// ---------------------------------------------------------------------------
// small string helpers
//
// These take and write byte slices and never assume the text is UTF-8: every
// field comes off a card anyone can write to.
// ---------------------------------------------------------------------------

/// The length of a NUL-terminated field.
fn c_len(field: &[u8]) -> usize {
    field.iter().position(|&b| b == 0).unwrap_or(field.len())
}

/// Truncating copy, the same `copy_bounded` the C used.
fn copy_bounded(dst: &mut [u8], src: &[u8]) {
    if dst.is_empty() {
        return;
    }
    let copy_len = src.len().min(dst.len() - 1);
    dst[..copy_len].copy_from_slice(&src[..copy_len]);
    dst[copy_len] = 0;
}

fn skip_ws(s: &[u8]) -> &[u8] {
    let mut i = 0;
    while i < s.len() && (s[i] == b' ' || s[i] == b'\t') {
        i += 1;
    }
    &s[i..]
}

/// Copies the next whitespace-delimited word, upper-cased, and returns the
/// position just after it. Commands and keywords are compared case-insensitively
/// because sheets are written by hand as often as by a ripper.
fn next_keyword<'a>(s: &'a [u8], out: &mut [u8]) -> &'a [u8] {
    let s = skip_ws(s);
    let mut n = 0usize;
    let mut i = 0usize;
    while i < s.len() && s[i] != b' ' && s[i] != b'\t' {
        if n + 1 < out.len() {
            out[n] = s[i].to_ascii_uppercase();
            n += 1;
        }
        i += 1;
    }
    if !out.is_empty() {
        out[n] = 0;
    }
    &s[i..]
}

/// ASCII case-insensitive equality, which is what `strcasecmp` does in the
/// C locale for the bytes a file name can hold.
fn eq_ignore_case(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len() && a.iter().zip(b).all(|(&x, &y)| x.eq_ignore_ascii_case(&y))
}

/// The type token that trails a bare FILE name ("FILE album.wav WAVE"). Only a
/// known keyword is dropped, so a file genuinely called "my album WAVE.flac"
/// loses its last word but "song.wav" does not lose its name.
fn is_file_type_word(word: &[u8]) -> bool {
    const TYPES: [&[u8]; 14] = [
        b"WAVE",
        b"MP3",
        b"AIFF",
        b"BINARY",
        b"MOTOROLA",
        b"FLAC",
        b"APE",
        b"WV",
        b"OGG",
        b"VORBIS",
        b"AAC",
        b"MP4",
        b"M4A",
        b"OPUS",
    ];
    TYPES.iter().any(|t| eq_ignore_case(word, t))
}

/// Reads a command's argument: `TITLE "Some Song"` and `TITLE Some Song` both
/// have to work, and the quoted form is the only one that can hold a trailing
/// keyword safely. `drop_type_word` is for FILE.
fn read_value(s: &[u8], out: &mut [u8], drop_type_word: bool) {
    let s = skip_ws(s);
    if out.is_empty() {
        return;
    }
    out[0] = 0;

    if s.first() == Some(&b'"') {
        let mut n = 0usize;
        for &b in &s[1..] {
            if b == b'"' {
                break;
            }
            if n + 1 < out.len() {
                out[n] = b;
                n += 1;
            }
        }
        out[n] = 0;
        return;
    }

    copy_bounded(out, s);

    let mut len = c_len(out);
    while len > 0 && (out[len - 1] == b' ' || out[len - 1] == b'\t') {
        len -= 1;
        out[len] = 0;
    }

    if drop_type_word {
        if let Some(space) = out[..len].iter().rposition(|&b| b == b' ') {
            if is_file_type_word(&out[space + 1..len]) {
                out[space] = 0;
                len = space;
                while len > 0 && (out[len - 1] == b' ' || out[len - 1] == b'\t') {
                    len -= 1;
                    out[len] = 0;
                }
            }
        }
    }
}

/// `strtol` as far as this file uses it: leading space, an optional sign, then
/// digits, saturating instead of wrapping. `None` when there were no digits.
fn strtoll(s: &[u8]) -> Option<i64> {
    let mut i = 0;
    while i < s.len() && (s[i] == b' ' || s[i] == b'\t' || s[i] == b'\n' || s[i] == b'\r') {
        i += 1;
    }
    let mut negative = false;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        negative = s[i] == b'-';
        i += 1;
    }
    let mut value: i64 = 0;
    let start = i;
    while i < s.len() && s[i].is_ascii_digit() {
        value = value
            .saturating_mul(10)
            .saturating_add(i64::from(s[i] - b'0'));
        i += 1;
    }
    if i == start {
        return None;
    }
    Some(if negative { -value } else { value })
}

/// mm:ss:ff, 75 frames to the second. Also accepts mm:ss, which hand-written
/// sheets do produce. Anything out of range is refused rather than clamped: a
/// wrong offset would send the decoder into the middle of another track.
fn parse_msf(s: &[u8]) -> Option<u64> {
    let mut s = skip_ws(s);
    let mut field = [0u64; 3];
    let mut count = 0usize;

    while count < 3 {
        if s.first().map_or(true, |b| !b.is_ascii_digit()) {
            return None;
        }
        let mut value: u64 = 0;
        let mut digits = 0usize;
        while let Some(&b) = s.first() {
            if !b.is_ascii_digit() {
                break;
            }
            if digits < 9 {
                // no overflow, and nothing sane is this long
                value = value * 10 + u64::from(b - b'0');
            }
            digits += 1;
            s = &s[1..];
        }
        field[count] = value;
        count += 1;
        if s.first() != Some(&b':') {
            break;
        }
        s = &s[1..];
    }

    if count < 2 {
        return None;
    }
    if field[0] > MAX_MINUTES || field[1] > 59 || field[2] > 74 {
        return None;
    }
    Some(((field[0] * 60 + field[1]) * 1000) + field[2] * 1000 / 75)
}

// ---------------------------------------------------------------------------
// locating the audio file
// ---------------------------------------------------------------------------

/// Last path component of a FILE field. Sheets written on a PC carry a whole
/// Windows path ("D:\rips\album.wav"); the name at the end is the only part
/// that means anything on the card.
fn basename_of(path: &[u8]) -> &[u8] {
    match path.iter().rposition(|&b| b == b'/') {
        Some(slash) => &path[slash + 1..],
        None => path,
    }
}

#[cfg(unix)]
fn os_bytes(name: &std::ffi::OsStr) -> Vec<u8> {
    use std::os::unix::ffi::OsStrExt;
    name.as_bytes().to_vec()
}

#[cfg(not(unix))]
fn os_bytes(name: &std::ffi::OsStr) -> Vec<u8> {
    name.to_string_lossy().into_owned().into_bytes()
}

#[cfg(unix)]
fn path_from_bytes(bytes: &[u8]) -> PathBuf {
    use std::os::unix::ffi::OsStrExt;
    PathBuf::from(std::ffi::OsStr::from_bytes(bytes))
}

#[cfg(not(unix))]
fn path_from_bytes(bytes: &[u8]) -> PathBuf {
    PathBuf::from(String::from_utf8_lossy(bytes).into_owned())
}

/// True when `path` names a regular file. The buffer is a `char` array the way
/// the C's was, so only the part before the terminator is a path.
fn file_exists(path: &[u8]) -> bool {
    let end = c_len(path);
    fs::metadata(path_from_bytes(&path[..end])).map_or(false, |m| m.is_file())
}

/// Writes `a/b` into `dst` with `snprintf`'s rule: true when it fitted.
fn join_into(dst: &mut [u8], a: &[u8], b: &[u8]) -> bool {
    let total = a.len() + 1 + b.len();
    if total >= dst.len() {
        return false;
    }
    dst[..a.len()].copy_from_slice(a);
    dst[a.len()] = b'/';
    dst[a.len() + 1..total].copy_from_slice(b);
    dst[total] = 0;
    true
}

/// FAT is case-insensitive but the sheet and the directory entry rarely agree
/// on case ("Album.WAV" against album.wav). The kernel's vfat mount is not
/// always forgiving, so the directory is scanned for a name that differs only
/// in case.
fn match_in_dir(dir: &[u8], name: &[u8], out: &mut [u8]) -> bool {
    let Ok(entries) = fs::read_dir(path_from_bytes(dir)) else {
        return false;
    };
    for entry in entries.flatten() {
        let candidate = os_bytes(&entry.file_name());
        if !eq_ignore_case(&candidate, name) {
            continue;
        }
        if join_into(out, dir, &candidate) && file_exists(out) {
            return true;
        }
    }
    false
}

/// The audio a sheet names under another extension: a disc ripped to WAV with
/// its sheet and converted afterwards, the sheet still naming the .wav.
fn match_other_extension(dir: &[u8], name: &[u8], out: &mut [u8]) -> bool {
    const EXTS: [&[u8]; 13] = [
        b".flac", b".wav", b".wv", b".ape", b".m4a", b".alac", b".aif", b".aiff", b".mp3", b".ogg",
        b".opus", b".dsf", b".dff",
    ];

    let stem = match name.iter().rposition(|&b| b == b'.') {
        Some(dot) => dot,
        None => name.len(),
    };
    if stem == 0 {
        return false;
    }
    let Ok(entries) = fs::read_dir(path_from_bytes(dir)) else {
        return false;
    };

    let mut best: Option<usize> = None;
    let mut found: [u8; 256] = [0; 256];
    for entry in entries.flatten() {
        let candidate = os_bytes(&entry.file_name());
        if candidate.len() <= stem
            || !eq_ignore_case(&candidate[..stem], &name[..stem])
            || candidate[stem] != b'.'
        {
            continue;
        }
        for (i, ext) in EXTS.iter().enumerate() {
            let better = best.map_or(true, |b| i < b);
            if eq_ignore_case(&candidate[stem..], ext) && better && candidate.len() < found.len() {
                best = Some(i);
                copy_bounded(&mut found, &candidate);
                break;
            }
        }
    }

    best.is_some() && join_into(out, dir, &found[..c_len(&found)]) && file_exists(out)
}

/// Turns the FILE field into an absolute path on the card, or gives up.
fn resolve_audio(cue_path: &[u8], field: &[u8], out: &mut [u8]) -> bool {
    if field.is_empty() {
        return false;
    }
    out[0] = 0;

    // Backslashes only ever arrive from a PC-written sheet, where they are
    // separators; treating them as such is what makes basename_of work.
    let mut rel = [0u8; LINE_MAX];
    copy_bounded(&mut rel, field);
    let rel_len = c_len(&rel);
    for b in &mut rel[..rel_len] {
        if *b == b'\\' {
            *b = b'/';
        }
    }
    let rel = &rel[..rel_len];

    let mut dir_buf = [0u8; PATH_MAX];
    copy_bounded(&mut dir_buf, cue_path);
    let mut dir_len = c_len(&dir_buf);
    if let Some(slash) = dir_buf[..dir_len].iter().rposition(|&b| b == b'/') {
        dir_len = slash;
        if dir_len == 0 {
            dir_buf[0] = b'/'; // the sheet sits in the root
            dir_len = 1;
        }
        dir_buf[dir_len] = 0;
    } else {
        copy_bounded(&mut dir_buf, b".");
        dir_len = 1;
    }
    let dir = &dir_buf[..dir_len];

    // As written, relative to the sheet unless the sheet gives an absolute path.
    if rel.first() == Some(&b'/') {
        copy_bounded(out, rel);
    } else if !join_into(out, dir, rel) {
        out[0] = 0; // did not fit: only the bare name can still work
    }
    if out[0] != 0 && file_exists(out) {
        return true;
    }

    // Same folder as the name asked for, ignoring case.
    let base = basename_of(rel);
    let out_len = c_len(out);
    if out_len > 0 {
        let mut sub = [0u8; PATH_MAX];
        copy_bounded(&mut sub, &out[..out_len]);
        let sub_len = c_len(&sub);
        if let Some(slash) = sub[..sub_len].iter().rposition(|&b| b == b'/') {
            // The C writes the terminator first and then patches the root case,
            // which leaves an empty name for a sheet in the root. Same here.
            let (dir_end, name_start, name_end) = if slash == 0 {
                sub[0] = b'/';
                sub[1] = 0;
                (1usize, 1usize, 1usize)
            } else {
                sub[slash] = 0;
                (slash, slash + 1, sub_len)
            };
            if match_in_dir(&sub[..dir_end], &sub[name_start..name_end], out) {
                return true;
            }
            out[0] = 0; // match_in_dir may have left a partial path behind
        }
    }

    // Last resort: the bare name next to the sheet. Covers the stale absolute
    // path a PC left behind, and sub-folders that no longer exist.
    if join_into(out, dir, base) && file_exists(out) {
        return true;
    }
    if match_in_dir(dir, base, out) {
        return true;
    }
    if match_other_extension(dir, base, out) {
        return true;
    }

    out[0] = 0;
    false
}

// ---------------------------------------------------------------------------
// parsing a sheet
// ---------------------------------------------------------------------------

fn parse_rem(args: &[u8], out: &mut Sheet) {
    let mut key = [0u8; 16];
    let rest = next_keyword(args, &mut key);
    let key = &key[..c_len(&key)];

    if key == b"GENRE" {
        read_value(rest, &mut out.genre, false);
    } else if key == b"DISCNUMBER" {
        let mut value = [0u8; 32];
        read_value(rest, &mut value, false);
        if let Some(disc) = strtoll(&value) {
            if (1..=999).contains(&disc) {
                out.disc = disc as i32;
            }
        }
    } else if key == b"DATE" || key == b"YEAR" {
        let mut value = [0u8; 32];
        read_value(rest, &mut value, false);
        // Taggers write "1973", "1973-05-01" and "May 1973"; the first run of
        // four digits is the only part worth keeping.
        let value: Vec<u8> = value[..c_len(&value)].to_vec();
        for (i, &b) in value.iter().enumerate() {
            if !b.is_ascii_digit() {
                continue;
            }
            if let Some(year) = strtoll(&value[i..]) {
                if (1000..=9999).contains(&year) {
                    out.year = year as i32;
                }
            }
            break;
        }
    }
}

/// `fgets`: up to `buf.len() - 1` bytes, stopping after a newline. `None` at
/// end of file, which is also what a read error looks like to the C.
fn read_line<R: Read>(f: &mut R, buf: &mut [u8]) -> Option<usize> {
    let mut n = 0usize;
    while n + 1 < buf.len() {
        let mut one = [0u8; 1];
        match f.read(&mut one) {
            Ok(0) => break,
            Ok(_) => {
                buf[n] = one[0];
                n += 1;
                if one[0] == b'\n' {
                    break;
                }
            }
            Err(_) => break,
        }
    }
    if n == 0 {
        return None;
    }
    buf[n] = 0;
    Some(n)
}

fn parse_sheet(cue_path: &[u8], out: &mut Sheet) -> bool {
    out.clear();

    let Ok(file) = File::open(path_from_bytes(cue_path)) else {
        return false;
    };
    let mut f = BufReader::new(file);

    let mut file_field = [0u8; LINE_MAX];
    let mut have_file = false;
    let mut placed = [false; MAX_TRACKS];

    let mut count = 0usize; // tracks kept, placed or not
    let mut current: Option<usize> = None; // -1 when the track was skipped
                                           // TITLE and PERFORMER mean the album before the first TRACK and the track
                                           // after it. A skipped track (data, over the cap, unreadable number) still
                                           // opens a track scope: its title belongs to it, not to the album.
    let mut in_track = false;

    let mut line = [0u8; LINE_MAX];
    let mut first_line = true;
    let mut discard_rest_of_line = false;
    let mut lines: i64 = 0;

    while let Some(got) = read_line(&mut f, &mut line) {
        lines += 1;
        if lines > MAX_LINES {
            break;
        }

        let bytes = &line[..got];
        let len = c_len(bytes);
        let mut text = &bytes[..len];
        let complete = len > 0 && text[len - 1] == b'\n';

        // An over-long line is read in chunks: the first chunk is parsed (a
        // truncated TITLE is better than a lost one), the rest is thrown away
        // so its tail is never mistaken for a command of its own.
        if discard_rest_of_line {
            discard_rest_of_line = !complete;
            continue;
        }
        discard_rest_of_line = !complete;

        while let Some(&b) = text.last() {
            if b == b'\n' || b == b'\r' {
                text = &text[..text.len() - 1];
            } else {
                break;
            }
        }

        if first_line {
            first_line = false;
            if text.len() >= 3 && text[0] == 0xEF && text[1] == 0xBB && text[2] == 0xBF {
                text = &text[3..]; // UTF-8 BOM
            }
        }

        let mut command = [0u8; 16];
        let args = next_keyword(text, &mut command);
        let command = &command[..c_len(&command)];
        if command.is_empty() {
            continue;
        }

        if command == b"REM" {
            parse_rem(args, out);
        } else if command == b"FILE" {
            // One file per sheet is what a CD rip looks like. A second FILE
            // means every offset after it is measured from a different zero,
            // so parsing stops and what is already correct is kept.
            if have_file {
                break;
            }
            read_value(args, &mut file_field, true);
            have_file = true;
        } else if command == b"TITLE" {
            match current {
                Some(track) => read_value(args, &mut out.tracks[track].title, false),
                None if !in_track => read_value(args, &mut out.album, false),
                None => {}
            }
        } else if command == b"PERFORMER" {
            match current {
                Some(track) => read_value(args, &mut out.tracks[track].performer, false),
                None if !in_track => read_value(args, &mut out.performer, false),
                None => {}
            }
        } else if command == b"TRACK" {
            current = None;
            in_track = true;

            let mut number = [0u8; 16];
            let mut kind = [0u8; 16];
            let rest = next_keyword(args, &mut number);
            next_keyword(rest, &mut kind);

            // A data track on a mixed-mode disc is not in the audio file, so
            // it is skipped; a missing type is taken as audio, since sheets
            // that omit it are audio-only rips.
            let kind = &kind[..c_len(&kind)];
            if !kind.is_empty() && kind != b"AUDIO" {
                continue;
            }
            let Some(value) = strtoll(&number) else {
                continue;
            };
            if !(1..=9999).contains(&value) {
                continue;
            }
            if count >= MAX_TRACKS {
                continue; // a sheet claiming hundreds of tracks: keep the first 99
            }

            current = Some(count);
            out.tracks[count].number = value as i32;
            count += 1;
        } else if command == b"INDEX" {
            let Some(track) = current else {
                continue;
            };
            let mut number = [0u8; 16];
            let rest = next_keyword(args, &mut number);
            if strtoll(&number) != Some(1) {
                continue; // INDEX 00 is the pre-gap, higher indexes are sub-cues
            }
            if let Some(begin) = parse_msf(rest) {
                out.tracks[track].begin_ms = begin;
                placed[track] = true;
            }
        }
        // Anything else (FLAGS, ISRC, SONGWRITER, CATALOG, PREGAP, POSTGAP, or
        // a command from a newer ripper) is not needed here.
    }

    if !have_file || count == 0 {
        return false;
    }

    // Drop the headings that never got an INDEX 01, closing the gaps so the
    // array stays contiguous. Track numbers keep what the sheet wrote.
    let mut kept = 0usize;
    for i in 0..count {
        if !placed[i] {
            continue;
        }
        if kept != i {
            out.tracks[kept] = out.tracks[i];
        }
        kept += 1;
    }
    for track in out.tracks.iter_mut().skip(kept) {
        *track = Track::zeroed();
    }
    out.track_count = kept as i32;
    if kept == 0 {
        return false;
    }

    let album_performer = out.performer;
    for i in 0..kept {
        if out.tracks[i].performer[0] == 0 {
            copy_bounded(&mut out.tracks[i].performer, &album_performer);
        }
        // The last track runs to the end of the file, and so does any track a
        // scrambled sheet placed after the one that follows it.
        out.tracks[i].end_ms = 0;
        if i + 1 < kept && out.tracks[i + 1].begin_ms > out.tracks[i].begin_ms {
            out.tracks[i].end_ms = out.tracks[i + 1].begin_ms;
        }
    }

    resolve_audio(
        cue_path,
        &file_field[..c_len(&file_field)],
        &mut out.audio_path,
    )
    // the sheet describes a file that is not on the card
}

// ---------------------------------------------------------------------------
// Markers inside a WAV
//
// A WAV can carry its own track list and needs no sheet beside it: the RIFF
// "cue " chunk is a list of points into the audio, and the "LIST"/"adtl" chunk
// that usually follows gives each one a label. It is what a vinyl rip made in
// Audition or Audacity looks like — one long file, a marker where each track
// starts — and without reading them the whole side is one row an hour long.
// ---------------------------------------------------------------------------

#[derive(Clone, Copy)]
struct Marker {
    /// The cue point's own name, which the labels refer to.
    id: u32,
    /// Where it sits, in frames from the start of the audio.
    frame: u64,
    label: [u8; TITLE_MAX],
}

impl Marker {
    const fn zeroed() -> Self {
        Marker {
            id: 0,
            frame: 0,
            label: [0; TITLE_MAX],
        }
    }
}

fn le32(p: &[u8]) -> u32 {
    u32::from(p[0]) | (u32::from(p[1]) << 8) | (u32::from(p[2]) << 16) | (u32::from(p[3]) << 24)
}

fn le16(p: &[u8]) -> u16 {
    u16::from(p[0]) | (u16::from(p[1]) << 8)
}

fn read_exact(f: &mut File, buf: &mut [u8]) -> bool {
    f.read_exact(buf).is_ok()
}

/// Reads the "cue " points and their labels, returning how many were kept.
fn wav_read_markers(f: &mut File, marks: &mut [Marker]) -> (usize, u32, u64) {
    let mut count = 0usize;
    let mut rate: u32 = 0;
    let mut channels: u16 = 0;
    let mut bits: u16 = 0;
    let mut data_bytes: u64 = 0;

    let mut header = [0u8; 12];
    if !read_exact(f, &mut header) {
        return (0, 0, 0);
    }
    if &header[..4] != b"RIFF" || &header[8..] != b"WAVE" {
        return (0, 0, 0);
    }

    for _chunk in 0..WAV_MAX_CHUNK_WALK {
        let mut head = [0u8; 8];
        if !read_exact(f, &mut head) {
            break;
        }
        let size = le32(&head[4..]);
        // i64, not i32 (and not C's off_t on the device): a two-gigabyte side
        // at 24/192 is one, and a 32-bit walk would go backwards from it.
        let Ok(body) = f.stream_position() else {
            break;
        };
        let body = body as i64;
        // Chunks are padded to an even length, and the pad byte is not counted.
        let next = body + i64::from(size) + i64::from(size & 1);

        if &head[..4] == b"fmt " && size >= 16 {
            let mut fmt = [0u8; 16];
            if read_exact(f, &mut fmt) {
                channels = le16(&fmt[2..]);
                rate = le32(&fmt[4..]);
                bits = le16(&fmt[14..]);
            }
        } else if &head[..4] == b"data" {
            data_bytes = u64::from(size);
        } else if &head[..4] == b"cue " && size >= 4 {
            let mut points = [0u8; 4];
            if read_exact(f, &mut points) {
                let mut declared = le32(&points);
                // The count is the file's word against the chunk's own length;
                // believe whichever is smaller, and never more than fits.
                let by_size = (size - 4) / 24;
                if declared > by_size {
                    declared = by_size;
                }
                let mut i = 0u32;
                while i < declared && count < marks.len() {
                    let mut point = [0u8; 24];
                    if !read_exact(f, &mut point) {
                        break;
                    }
                    marks[count].id = le32(&point);
                    // Byte 20 is the sample offset inside the data chunk, which
                    // is what every writer of these files actually fills in;
                    // dwPosition (byte 4) is play-order and often zero.
                    marks[count].frame = u64::from(le32(&point[20..]));
                    marks[count].label[0] = 0;
                    count += 1;
                    i += 1;
                }
            }
        } else if &head[..4] == b"LIST" && size >= 4 {
            let mut kind = [0u8; 4];
            if read_exact(f, &mut kind) && &kind == b"adtl" {
                let list_end = body + i64::from(size);
                for _sub in 0..WAV_MAX_CHUNK_WALK {
                    let Ok(pos) = f.stream_position() else {
                        break;
                    };
                    if (pos as i64) + 8 > list_end {
                        break;
                    }
                    let mut sub_head = [0u8; 8];
                    if !read_exact(f, &mut sub_head) {
                        break;
                    }
                    let sub_size = le32(&sub_head[4..]);
                    let Ok(sub_body) = f.stream_position() else {
                        break;
                    };
                    let sub_body = sub_body as i64;
                    let sub_next = sub_body + i64::from(sub_size) + i64::from(sub_size & 1);

                    // "labl" is the marker's name, "note" a longer comment. The
                    // name is what a track list wants.
                    if &sub_head[..4] == b"labl" && sub_size >= 5 && sub_size < 4096 {
                        let mut id_bytes = [0u8; 4];
                        if read_exact(f, &mut id_bytes) {
                            let id = le32(&id_bytes);
                            let text_len = (sub_size - 4) as usize;
                            let take = text_len.min(TITLE_MAX - 1);
                            let mut text = [0u8; TITLE_MAX];
                            if read_exact(f, &mut text[..take]) {
                                let cut = text[..take]
                                    .iter()
                                    .position(|&b| b == b'\r' || b == b'\n')
                                    .unwrap_or(take);
                                text[cut] = 0;
                                for mark in marks.iter_mut().take(count) {
                                    if mark.id == id {
                                        copy_bounded(&mut mark.label, &text[..cut]);
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    // A zero-size sub-chunk is legal and advances by its own
                    // header; only a backwards step is a malformed file.
                    if sub_next < sub_body
                        || sub_next > list_end
                        || f.seek(SeekFrom::Start(sub_next as u64)).is_err()
                    {
                        break;
                    }
                }
            }
        }

        // A zero-size chunk is legal -- "fact", an empty "LIST" -- and the walk
        // still advances by the eight bytes of the next header. Stopping on one
        // would abandon the markers that come after it.
        if next < body || f.seek(SeekFrom::Start(next as u64)).is_err() {
            break;
        }
    }

    if rate == 0 || channels == 0 || bits == 0 {
        return (0, 0, 0); // no format: nothing to turn frames into time with
    }

    let frame_bytes = u64::from(channels) * u64::from(bits / 8);
    let total_frames = if frame_bytes == 0 {
        0
    } else {
        data_bytes / frame_bytes
    };
    (count, rate, total_frames)
}

/// True when the name ends in .wav or .wave.
fn is_wav_name(path: &[u8]) -> bool {
    let len = path.len();
    (len > 4 && eq_ignore_case(&path[len - 4..], b".wav"))
        || (len > 5 && eq_ignore_case(&path[len - 5..], b".wave"))
}

fn parse_wav(wav_path: &[u8], out: &mut Sheet) -> bool {
    let Ok(file) = File::open(path_from_bytes(wav_path)) else {
        return false;
    };
    let mut f = file;

    // Ninety-nine markers are seventeen kilobytes, and this is reached from the
    // playback thread under a decoder open and from the scan thread a dozen
    // directories deep: the same reason the sheet itself is not on the stack.
    let mut marks = vec![Marker::zeroed(); MAX_TRACKS];
    let (mut count, rate, total_frames) = wav_read_markers(&mut f, &mut marks);

    if count == 0 || rate == 0 {
        return false;
    }

    marks[..count].sort_by_key(|m| m.frame);

    // Markers past the end of the audio describe a file this is not, and one
    // that lands a moment before it would open a track with nothing in it.
    let min_frames = u64::from(rate) * WAV_MIN_TRACK_MS / 1000;
    let last_usable = if total_frames > min_frames {
        total_frames - min_frames
    } else {
        0
    };
    while count > 0 && total_frames > 0 && marks[count - 1].frame >= last_usable {
        count -= 1;
    }
    // Markers closer together than a track can be are one boundary: the same
    // point written twice, or an editor's leftovers around it.
    let mut kept = 0usize;
    for i in 0..count {
        if kept > 0 && marks[i].frame < marks[kept - 1].frame + min_frames {
            continue;
        }
        marks[kept] = marks[i];
        kept += 1;
    }
    let count = kept;

    // One marker is not a track list. It is a loop point, a note, a cut -- and
    // splitting a recording in two on the strength of it would take the file
    // out of the library and put two halves of it in.
    if count < 2 {
        return false;
    }

    // A marker at the very start is the start of track one; markers that begin
    // further in leave a first track before them, which is what a rip with a
    // marker only at each boundary means.
    let first_ms = marks[0].frame * 1000 / u64::from(rate);
    let implicit_first = first_ms >= WAV_MIN_TRACK_MS;

    let mut tracks = count + usize::from(implicit_first);
    if tracks > MAX_TRACKS {
        // A sheet holds ninety-nine tracks. Trimming loses the tail of a side
        // no real rip has; refusing would lose the whole side.
        tracks = MAX_TRACKS;
    }

    out.clear();
    copy_bounded(&mut out.audio_path, wav_path);
    out.track_count = tracks as i32;

    for i in 0..tracks {
        let track = &mut out.tracks[i];
        track.number = (i + 1) as i32;

        let mark = if implicit_first {
            i as i32 - 1
        } else {
            i as i32
        };
        track.begin_ms = if mark < 0 {
            0
        } else {
            marks[mark as usize].frame * 1000 / u64::from(rate)
        };
        if mark >= 0 {
            let label = marks[mark as usize].label;
            copy_bounded(&mut track.title, &label);
        }
    }

    for i in 0..tracks.saturating_sub(1) {
        out.tracks[i].end_ms = out.tracks[i + 1].begin_ms;
    }

    true
}

/// Parses the sheet at `cue_path`. False when it is not a usable sheet: no
/// FILE, no track with an INDEX, or the audio file it names is not there.
///
/// A WAV that carries markers is parsed as its own sheet.
pub fn parse(cue_path: &[u8], out: &mut Sheet) -> bool {
    // The C leaves `out` untouched when it refuses a WAV early and clears it
    // before opening anything else; each of the two paths below keeps that.
    if cue_path.is_empty() {
        return false;
    }
    if is_wav_name(cue_path) {
        return parse_wav(cue_path, out);
    }
    parse_sheet(cue_path, out)
}

// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

/// True when the name ends in ".cue" (case-insensitive).
pub fn is_sheet(path: &[u8]) -> bool {
    let len = path.len();
    len > 4 && eq_ignore_case(&path[len - 4..], b".cue")
}

/// True when the name is a WAV. Only the name is looked at; whether there
/// really are markers is [`parse`]'s answer.
pub fn wav_has_markers(path: &[u8]) -> bool {
    is_wav_name(path)
}

/// The virtual path this player uses for one track of a sheet, 1-based:
/// `/mnt/sd/Music/Album/album.cue?track=3`.
pub fn virtual_path(cue_path: Option<&[u8]>, track_number: i32, out: &mut [u8]) {
    if out.is_empty() {
        return;
    }
    let Some(cue_path) = cue_path else {
        out[0] = 0;
        return;
    };
    if track_number < 1 {
        out[0] = 0;
        return;
    }
    let suffix = format!("?track={track_number}");
    let total = cue_path.len() + suffix.len();
    let take = total.min(out.len() - 1);
    let cue_take = cue_path.len().min(take);
    out[..cue_take].copy_from_slice(&cue_path[..cue_take]);
    let rest = &suffix.as_bytes()[..take - cue_take];
    out[cue_take..take].copy_from_slice(rest);
    out[take] = 0;
}

/// Splits a virtual path: the 1-based track number, with the sheet path in
/// `cue_out`. 0 for an ordinary path (still copied).
pub fn split_path(path: &[u8], cue_out: &mut [u8]) -> i32 {
    if !cue_out.is_empty() {
        cue_out[0] = 0;
    }

    // A real file name may well contain '?', so the suffix only counts when it
    // is the last one and the whole tail is "?track=<digits>".
    let mut number = 0i32;
    let mut sheet_len = path.len();
    if let Some(mark) = path.iter().rposition(|&b| b == b'?') {
        let tail = &path[mark..];
        if tail.len() > 7 && &tail[..7] == b"?track=" && tail[7].is_ascii_digit() {
            let digits = &tail[7..];
            let end = digits
                .iter()
                .position(|b| !b.is_ascii_digit())
                .unwrap_or(digits.len());
            if end == digits.len() {
                if let Some(value) = strtoll(digits) {
                    if (1..=MAX_TRACKS as i64).contains(&value) {
                        number = value as i32;
                        sheet_len = mark;
                    }
                }
            }
        }
    }

    if !cue_out.is_empty() {
        let len = if number > 0 {
            sheet_len.min(cue_out.len() - 1)
        } else {
            path.len().min(cue_out.len() - 1)
        };
        cue_out[..len].copy_from_slice(&path[..len]);
        cue_out[len] = 0;
    }
    number
}

/// Orders two paths that are both tracks of the same sheet, by track number.
pub fn order_tracks(a: &[u8], b: &[u8], out: &mut i32) -> bool {
    let mut sheet_a = [0u8; PATH_MAX];
    let mut sheet_b = [0u8; PATH_MAX];
    let ta = split_path(a, &mut sheet_a);
    let tb = split_path(b, &mut sheet_b);
    if ta <= 0 || tb <= 0 {
        return false;
    }
    let sa = &sheet_a[..c_len(&sheet_a)];
    let sb = &sheet_b[..c_len(&sheet_b)];
    if sa != sb {
        return false;
    }
    *out = ta - tb;
    true
}

/// A sheet's path as a filesystem path.
pub fn path(path: &[u8]) -> PathBuf {
    path_from_bytes(path)
}

/// The bytes of a filesystem path, for the entry points that take one.
///
/// Windows separators become `/` here: this module is written against the
/// device's slash-separated paths, and the string work in `resolve_audio`
/// (and in the sheet it writes) only knows one separator. Windows itself
/// accepts either.
pub fn bytes(path: &Path) -> Vec<u8> {
    #[allow(unused_mut)]
    let mut bytes = os_bytes(path.as_os_str());
    #[cfg(not(unix))]
    for byte in &mut bytes {
        if *byte == b'\\' {
            *byte = b'/';
        }
    }
    bytes
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write(dir: &Path, name: &str, bytes: &[u8]) -> PathBuf {
        let path = dir.join(name);
        fs::write(&path, bytes).unwrap();
        path
    }

    fn temp_dir(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("sonix-cue-{tag}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn an_msf_stamp_becomes_milliseconds() {
        assert_eq!(parse_msf(b"00:00:00"), Some(0));
        assert_eq!(parse_msf(b"05:23:40"), Some(323_533));
        assert_eq!(parse_msf(b"  01:02"), Some(62_000));
        assert_eq!(parse_msf(b"1:2:3"), Some(62_040));
        assert_eq!(parse_msf(b"100000:00:00"), Some(6_000_000_000));
        assert_eq!(parse_msf(b"100001:00:00"), None);
        assert_eq!(parse_msf(b"00:60:00"), None);
        assert_eq!(parse_msf(b"00:00:75"), None);
        assert_eq!(parse_msf(b"00"), None);
        assert_eq!(parse_msf(b""), None);
        assert_eq!(parse_msf(b"aa:bb"), None);
        assert_eq!(parse_msf(b"-1:00"), None);
    }

    #[test]
    fn a_value_may_be_quoted_or_bare() {
        let mut out = [0u8; 32];
        read_value(b"\"Some Song\" trailing", &mut out, false);
        assert_eq!(Sheet::text(&out), "Some Song");

        read_value(b"Some Song   ", &mut out, false);
        assert_eq!(Sheet::text(&out), "Some Song");

        read_value(b"\"unterminated", &mut out, false);
        assert_eq!(Sheet::text(&out), "unterminated");
    }

    #[test]
    fn a_bare_file_name_loses_only_a_known_type_word() {
        let mut out = [0u8; 64];
        read_value(b"album.wav WAVE", &mut out, true);
        assert_eq!(Sheet::text(&out), "album.wav");

        read_value(b"my album WAVE.flac", &mut out, true);
        assert_eq!(Sheet::text(&out), "my album WAVE.flac");

        read_value(b"album.flac", &mut out, true);
        assert_eq!(Sheet::text(&out), "album.flac");
    }

    #[test]
    fn a_sheet_names_tracks_and_where_they_start() {
        let dir = temp_dir("parse");
        write(&dir, "album.flac", b"not really audio");
        let cue = write(
            &dir,
            "album.cue",
            b"REM GENRE Rock\r\nREM DATE 1973-05-01\r\nPERFORMER \"Some Band\"\r\n\
              TITLE \"Some Album\"\r\nFILE \"album.flac\" WAVE\r\n  TRACK 01 AUDIO\r\n\
              \x20   TITLE \"First Song\"\r\n    INDEX 01 00:00:00\r\n\
              \x20 TRACK 02 AUDIO\r\n    TITLE \"Second Song\"\r\n    INDEX 01 05:23:40\r\n",
        );

        let mut sheet = Sheet::zeroed();
        assert!(parse(&bytes(&cue), &mut sheet));
        assert_eq!(sheet.track_count, 2);
        assert_eq!(Sheet::text(&sheet.album), "Some Album");
        assert_eq!(Sheet::text(&sheet.performer), "Some Band");
        assert_eq!(Sheet::text(&sheet.genre), "Rock");
        assert_eq!(sheet.year, 1973);
        assert_eq!(
            Sheet::text(&sheet.audio_path),
            Sheet::text(&bytes(&dir.join("album.flac")))
        );
        assert_eq!(sheet.tracks[0].number, 1);
        assert_eq!(Sheet::text(&sheet.tracks[0].title), "First Song");
        assert_eq!(sheet.tracks[0].begin_ms, 0);
        assert_eq!(sheet.tracks[0].end_ms, 323_533);
        assert_eq!(sheet.tracks[1].begin_ms, 323_533);
        assert_eq!(sheet.tracks[1].end_ms, 0);
        // The album's performer stands in for a track that has none.
        assert_eq!(Sheet::text(&sheet.tracks[1].performer), "Some Band");

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_heading_without_an_index_is_dropped() {
        let dir = temp_dir("heading");
        write(&dir, "a.wav", b"x");
        let cue = write(
            &dir,
            "a.cue",
            b"FILE a.wav WAVE\nTRACK 01 AUDIO\nTITLE \"Kept\"\nINDEX 01 00:00:00\n\
              TRACK 02 AUDIO\nTITLE \"Dropped\"\nTRACK 03 AUDIO\nINDEX 01 00:10:00\n",
        );
        let mut sheet = Sheet::zeroed();
        assert!(parse(&bytes(&cue), &mut sheet));
        assert_eq!(sheet.track_count, 2);
        // The dropped heading takes its title with it; the next real track has
        // none, and keeps the sheet's own number.
        assert_eq!(Sheet::text(&sheet.tracks[1].title), "");
        assert_eq!(sheet.tracks[1].number, 3);
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_sheet_with_no_file_or_no_track_is_not_a_sheet() {
        let dir = temp_dir("empty");
        let no_file = write(&dir, "a.cue", b"TRACK 01 AUDIO\nINDEX 01 00:00:00\n");
        let no_track = write(&dir, "b.cue", b"FILE a.wav WAVE\n");
        let mut sheet = Sheet::zeroed();
        assert!(!parse(&bytes(&no_file), &mut sheet));
        assert!(!parse(&bytes(&no_track), &mut sheet));
        assert!(!parse(b"", &mut sheet));
        assert!(!parse(&bytes(&dir.join("missing.cue")), &mut sheet));
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn the_audio_is_found_through_another_extension_and_another_case() {
        let dir = temp_dir("resolve");
        write(&dir, "Album.FLAC", b"x");
        let cue = write(
            &dir,
            "album.cue",
            b"FILE album.wav WAVE\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n",
        );
        let mut sheet = Sheet::zeroed();
        assert!(
            parse(&bytes(&cue), &mut sheet),
            "a stale .wav must find the .flac"
        );
        assert_eq!(
            Sheet::text(&sheet.audio_path),
            Sheet::text(&bytes(&dir.join("Album.FLAC")))
        );

        // A Windows path is reduced to its last component.
        let cue2 = write(
            &dir,
            "album2.cue",
            b"FILE \"D:\\rips\\ALBUM.flac\" WAVE\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n",
        );
        let mut sheet2 = Sheet::zeroed();
        assert!(parse(&bytes(&cue2), &mut sheet2));
        // On a case-insensitive filesystem the name as written is the file, so
        // only the case can differ between the platforms here; the C does the
        // same thing for the same reason.
        assert_eq!(
            Sheet::text(&sheet2.audio_path).to_ascii_lowercase(),
            Sheet::text(&bytes(&dir.join("Album.FLAC"))).to_ascii_lowercase()
        );
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_virtual_path_round_trips() {
        let mut buf = [0u8; 64];
        virtual_path(Some(b"/mnt/sd/a.cue"), 3, &mut buf);
        assert_eq!(Sheet::text(&buf), "/mnt/sd/a.cue?track=3");

        let mut sheet = [0u8; 64];
        assert_eq!(split_path(b"/mnt/sd/a.cue?track=3", &mut sheet), 3);
        assert_eq!(Sheet::text(&sheet), "/mnt/sd/a.cue");

        assert_eq!(split_path(b"/mnt/sd/a.cue", &mut sheet), 0);
        assert_eq!(Sheet::text(&sheet), "/mnt/sd/a.cue");

        // A '?' that is not the suffix is part of the name.
        assert_eq!(split_path(b"/mnt/sd/what?.cue", &mut sheet), 0);
        assert_eq!(split_path(b"/mnt/sd/a.cue?track=0", &mut sheet), 0);
        assert_eq!(split_path(b"/mnt/sd/a.cue?track=100", &mut sheet), 0);
        assert_eq!(split_path(b"/mnt/sd/a.cue?track=2x", &mut sheet), 0);
    }

    #[test]
    fn tracks_of_one_sheet_order_by_number() {
        let mut out = 0;
        assert!(order_tracks(b"a.cue?track=2", b"a.cue?track=10", &mut out));
        assert_eq!(out, -8);
        assert!(!order_tracks(b"a.cue?track=1", b"b.cue?track=2", &mut out));
        assert!(!order_tracks(b"a.cue", b"a.cue?track=2", &mut out));
    }

    #[test]
    fn sheet_and_wav_names_are_recognised_by_their_ending() {
        assert!(is_sheet(b"/a/b.cue"));
        assert!(is_sheet(b"/a/b.CUE"));
        assert!(!is_sheet(b"/a/b.cuex"));
        assert!(!is_sheet(b".cue"));
        assert!(!is_sheet(b""));

        assert!(wav_has_markers(b"/a/b.wav"));
        assert!(wav_has_markers(b"/a/b.WAVE"));
        assert!(!wav_has_markers(b"/a/b.wavx"));
        assert!(!wav_has_markers(b".wav"));
    }

    fn wav_bytes(markers: &[(u32, u32, &str)], rate: u32, frames: u32) -> Vec<u8> {
        let channels = 2u16;
        let bits = 16u16;
        let data_bytes = frames * u32::from(channels) * u32::from(bits / 8);

        let mut out = Vec::new();
        out.extend_from_slice(b"RIFF");
        out.extend_from_slice(&0u32.to_le_bytes()); // fixed up below
        out.extend_from_slice(b"WAVE");

        out.extend_from_slice(b"fmt ");
        out.extend_from_slice(&16u32.to_le_bytes());
        out.extend_from_slice(&1u16.to_le_bytes()); // PCM
        out.extend_from_slice(&channels.to_le_bytes());
        out.extend_from_slice(&rate.to_le_bytes());
        out.extend_from_slice(&(rate * u32::from(channels) * 2).to_le_bytes());
        out.extend_from_slice(&(channels * 2).to_le_bytes());
        out.extend_from_slice(&bits.to_le_bytes());

        out.extend_from_slice(b"data");
        out.extend_from_slice(&data_bytes.to_le_bytes());
        out.resize(out.len() + data_bytes as usize, 0);

        if !markers.is_empty() {
            out.extend_from_slice(b"cue ");
            out.extend_from_slice(&(4 + 24 * markers.len() as u32).to_le_bytes());
            out.extend_from_slice(&(markers.len() as u32).to_le_bytes());
            for &(id, frame, _) in markers {
                out.extend_from_slice(&id.to_le_bytes()); // dwName
                out.extend_from_slice(&0u32.to_le_bytes()); // dwPosition
                out.extend_from_slice(b"data");
                out.extend_from_slice(&0u32.to_le_bytes()); // chunk start
                out.extend_from_slice(&0u32.to_le_bytes()); // block start
                out.extend_from_slice(&frame.to_le_bytes()); // sample offset
            }

            let mut labels = Vec::new();
            for &(id, _, label) in markers {
                let text = label.as_bytes();
                labels.extend_from_slice(b"labl");
                labels.extend_from_slice(&(4 + text.len() as u32 + 1).to_le_bytes());
                labels.extend_from_slice(&id.to_le_bytes());
                labels.extend_from_slice(text);
                labels.push(0);
                if labels.len() % 2 == 1 {
                    labels.push(0);
                }
            }
            out.extend_from_slice(b"LIST");
            out.extend_from_slice(&(4 + labels.len() as u32).to_le_bytes());
            out.extend_from_slice(b"adtl");
            out.extend_from_slice(&labels);
        }

        let riff = (out.len() - 8) as u32;
        out[4..8].copy_from_slice(&riff.to_le_bytes());
        out
    }

    #[test]
    fn a_wav_with_markers_is_its_own_sheet() {
        let dir = temp_dir("wav");
        let rate = 44100u32;
        let frames = rate * 60; // a minute
        let marks = [
            (1u32, 0u32, "One"),
            (2, rate * 20, "Two"),
            (3, rate * 40, "Three"),
        ];
        let wav = write(&dir, "side.wav", &wav_bytes(&marks, rate, frames));

        let mut sheet = Sheet::zeroed();
        assert!(parse(&bytes(&wav), &mut sheet));
        assert_eq!(sheet.track_count, 3);
        assert_eq!(Sheet::text(&sheet.tracks[0].title), "One");
        assert_eq!(sheet.tracks[0].begin_ms, 0);
        assert_eq!(sheet.tracks[0].end_ms, 20_000);
        assert_eq!(sheet.tracks[2].begin_ms, 40_000);
        assert_eq!(sheet.tracks[2].end_ms, 0);
        assert_eq!(Sheet::text(&sheet.audio_path), Sheet::text(&bytes(&wav)));

        // One marker is a loop point, not a track list.
        let one = write(&dir, "one.wav", &wav_bytes(&[(1, 0, "Only")], rate, frames));
        let mut single = Sheet::zeroed();
        assert!(!parse(&bytes(&one), &mut single));

        // A WAV that is not RIFF at all.
        let junk = write(&dir, "junk.wav", b"RIFFxxxxNOPE");
        assert!(!parse(&bytes(&junk), &mut single));

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn markers_before_the_first_track_start_leave_a_first_track() {
        let dir = temp_dir("wav-implicit");
        let rate = 44100u32;
        let frames = rate * 60;
        // First marker ten seconds in: the ten seconds before it are track one.
        let marks = [(1u32, rate * 10, "Second"), (2, rate * 30, "Third")];
        let wav = write(&dir, "side.wav", &wav_bytes(&marks, rate, frames));

        let mut sheet = Sheet::zeroed();
        assert!(parse(&bytes(&wav), &mut sheet));
        assert_eq!(sheet.track_count, 3);
        assert_eq!(sheet.tracks[0].begin_ms, 0);
        assert_eq!(Sheet::text(&sheet.tracks[0].title), "");
        assert_eq!(sheet.tracks[1].begin_ms, 10_000);
        assert_eq!(Sheet::text(&sheet.tracks[1].title), "Second");
        assert_eq!(sheet.tracks[0].end_ms, 10_000);

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn the_fitted_field_is_the_one_that_was_parsed() {
        // A title longer than the field is cut before its spaces are trimmed,
        // which is the order the C did it in and what makes the C structs equal.
        let mut out = [0u8; 8];
        copy_bounded(&mut out, b"abcdefgh  ");
        assert_eq!(Sheet::text(&out), "abcdefg");
    }
}
