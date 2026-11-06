//! Integer square roots, the duration formatters and the extension check,
//! ported from the non-platform half of `src/system/core/utils.c`.
//!
//! The rest of that file — threads, `mmap`, `mallinfo`, the breadcrumbs for the
//! crash log — stays C: it is `/proc`, `sched_setscheduler` and `sigaltstack`,
//! which is exactly the kind of thing that should keep working the way it
//! always has until the device is on the bench.

/// Square root, in integers, digit by digit: the largest `r` with `r * r <= v`.
///
/// No libm and no FPU. These are called per pixel while a finger is on the
/// screen (the halo under the record in Cover Flow, the rounded ends of the
/// waveform's bars) and per column while a track is being measured, all on
/// threads that must not stall the interface. Nothing in the algorithm can
/// overflow, which is the reason it is written out rather than done in floating
/// point and rounded.
pub fn isqrt32(v: u32) -> u32 {
    let mut rest = v;
    let mut root: u32 = 0;
    let mut bit: u32 = 1 << 30;

    while bit > rest {
        bit >>= 2;
    }
    while bit != 0 {
        if rest >= root + bit {
            rest -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    root
}

/// The same at 64 bits, for sums of squares over a whole track.
pub fn isqrt64(v: u64) -> u64 {
    let mut rest = v;
    let mut root: u64 = 0;
    let mut bit: u64 = 1 << 62;

    while bit > rest {
        bit >>= 2;
    }
    while bit != 0 {
        if rest >= root + bit {
            rest -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    root
}

/// Case-insensitive check of whether `name` ends with `ext` (e.g. `".wav"`).
///
/// `strcasecmp` folds ASCII only in the C locale, which is the only folding
/// that matters here: the extensions are all ASCII.
pub fn has_extension(name: &str, ext: &str) -> bool {
    let name = name.as_bytes();
    let ext = ext.as_bytes();
    if name.len() < ext.len() {
        return false;
    }
    name[name.len() - ext.len()..].eq_ignore_ascii_case(ext)
}

/// The tail of `snprintf`: copies what fits including the terminator, and
/// returns the length the whole text wanted — not the length written.
fn snprintf_copy(out: &mut [u8], text: &str) -> usize {
    let bytes = text.as_bytes();
    if !out.is_empty() {
        let n = bytes.len().min(out.len() - 1);
        out[..n].copy_from_slice(&bytes[..n]);
        out[n] = 0;
    }
    bytes.len()
}

/// Formats seconds as `HH:MM:SS`, hiding the hours when there are none and
/// always printing two seconds digits. Returns the number of characters the
/// result wants, as `snprintf` does.
///
/// A negative time does not exist, and casting one to `u32` used to turn it into
/// `1193046:28:15` — the figure seen on the progress bar of a long audiobook.
/// The real cause was elsewhere (a 32-bit byte counter, see `audio.c`), but a
/// formatter that prints an absurd figure instead of zero for an impossible
/// value is a second bug, not a symptom of the first.
pub fn format_double_seconds(total_seconds: f64, out: &mut [u8]) -> usize {
    // The negated comparison also catches NaN.
    let total_seconds = if total_seconds > 0.0 {
        total_seconds
    } else {
        0.0
    };

    // Rust's float-to-integer cast saturates, where C's is undefined for a value
    // that does not fit. Saturating is the sane version of what the C meant.
    let total_secs = total_seconds as u32;

    let hours = total_secs / 3600;
    let rem = total_secs % 3600;
    let minutes = rem / 60;
    let seconds = rem % 60;

    if hours > 0 {
        snprintf_copy(out, &format!("{hours}:{minutes:02}:{seconds:02}"))
    } else {
        snprintf_copy(out, &format!("{minutes}:{seconds:02}"))
    }
}

/// Formats a pair of times as `HH:MM:SS/HH:MM:SS`, each part as
/// [`format_double_seconds`] writes it.
pub fn format_double_progress(current_secs: f64, total_secs: f64, out: &mut [u8]) {
    if out.is_empty() {
        return;
    }

    let written = format_double_seconds(current_secs, out);

    // `snprintf` returns what it wanted to write, not what it wrote, so a value
    // at or past the capacity means the buffer is already full.
    if written >= out.len() {
        return;
    }

    let mut at = written;
    let mut left = out.len() - written;

    // Room for '/' and the terminator.
    if left < 2 {
        return;
    }
    out[at] = b'/';
    at += 1;
    left -= 1;

    format_double_seconds(total_secs, &mut out[at..at + left]);
}

/// A list row's `3:05` — minutes never roll into hours, which is the form every
/// streaming row shows. Nothing (empty) for a track without a length.
pub fn format_row_duration(seconds: i32, out: &mut [u8]) {
    if out.is_empty() {
        return;
    }
    if seconds <= 0 {
        out[0] = 0;
        return;
    }
    snprintf_copy(out, &format!("{}:{:02}", seconds / 60, seconds % 60));
}

#[cfg(test)]
mod tests {
    use super::*;

    fn text(out: &[u8]) -> String {
        let end = out.iter().position(|&b| b == 0).unwrap_or(out.len());
        String::from_utf8_lossy(&out[..end]).into_owned()
    }

    #[test]
    fn isqrt_is_exact_at_the_edges() {
        assert_eq!(isqrt32(0), 0);
        assert_eq!(isqrt32(1), 1);
        assert_eq!(isqrt32(2), 1);
        assert_eq!(isqrt32(3), 1);
        assert_eq!(isqrt32(4), 2);
        assert_eq!(isqrt32(15), 3);
        assert_eq!(isqrt32(16), 4);
        assert_eq!(isqrt32(u32::MAX), 65535);

        assert_eq!(isqrt64(0), 0);
        assert_eq!(isqrt64(4), 2);
        assert_eq!(isqrt64(u64::MAX), 4_294_967_295);
    }

    /// The property that matters: it is the *largest* r with r*r <= v, not an
    /// approximation that can sit one either side of it.
    #[test]
    fn isqrt_never_overshoots_and_never_falls_short() {
        for v in (0u32..200_000).chain((0u32..1000).map(|i| u32::MAX - i)) {
            let r = isqrt32(v);
            assert!(u64::from(r) * u64::from(r) <= u64::from(v), "v={v} r={r}");
            let next = u64::from(r) + 1;
            assert!(next * next > u64::from(v), "v={v} r={r}");
        }
        for v in [
            0u64,
            1,
            4_294_967_296,
            u64::MAX - 1,
            u64::MAX,
            1_000_000_000_000,
        ] {
            let r = isqrt64(v);
            assert!(r * r <= v);
            if let Some(next) = r.checked_add(1) {
                if let Some(sq) = next.checked_mul(next) {
                    assert!(sq > v, "v={v} r={r}");
                }
            }
        }
    }

    #[test]
    fn extension_check_is_case_insensitive_and_anchored() {
        assert!(has_extension("track.FLAC", ".flac"));
        assert!(has_extension("a.WAV", ".wav"));
        assert!(has_extension("track.flac", ".flac"));
        assert!(!has_extension("track.flac", ".wav"));
        assert!(!has_extension("flac", ".flac"));
        assert!(!has_extension("track", ".tracktrack"));
        assert!(has_extension("x.ogg", ".ogg"));
        // A name that ends with the extension text but not as an extension is
        // still a match for this function, which is what the C version does.
        assert!(has_extension("nottrack.flac", ".flac"));
        assert!(has_extension("track.oga", ".oga"));
        assert!(!has_extension("", ".flac"));
    }

    #[test]
    fn seconds_are_formatted_like_the_player_shows_them() {
        let mut buf = [0u8; 32];
        format_double_seconds(0.0, &mut buf);
        assert_eq!(text(&buf), "0:00");
        format_double_seconds(5.9, &mut buf);
        assert_eq!(text(&buf), "0:05", "seconds truncate, never round up");
        format_double_seconds(65.0, &mut buf);
        assert_eq!(text(&buf), "1:05");
        format_double_seconds(3600.0, &mut buf);
        assert_eq!(text(&buf), "1:00:00");
        format_double_seconds(3661.0, &mut buf);
        assert_eq!(text(&buf), "1:01:01");
        format_double_seconds(-5.0, &mut buf);
        assert_eq!(text(&buf), "0:00", "a negative time shows as zero");
        format_double_seconds(f64::NAN, &mut buf);
        assert_eq!(text(&buf), "0:00", "and so does NaN");
    }

    #[test]
    fn seconds_report_the_length_wanted_even_when_they_do_not_fit() {
        let mut small = [0u8; 4];
        let wanted = format_double_seconds(3661.0, &mut small);
        assert_eq!(wanted, 7, "\"1:01:01\" is seven characters");
        assert_eq!(text(&small), "1:0", "three characters plus the terminator");
    }

    #[test]
    fn progress_joins_both_sides() {
        let mut buf = [0u8; 32];
        format_double_progress(65.0, 300.0, &mut buf);
        assert_eq!(text(&buf), "1:05/5:00");

        format_double_progress(0.0, 3600.0, &mut buf);
        assert_eq!(text(&buf), "0:00/1:00:00");
    }

    #[test]
    fn progress_leaves_what_does_not_fit_alone() {
        // Exactly enough for "1:05" and nothing else.
        let mut tight = [0u8; 4];
        format_double_progress(65.0, 300.0, &mut tight);
        assert_eq!(text(&tight), "1:0", "no room, so no slash is written");

        let mut empty = [0u8; 0];
        format_double_progress(65.0, 300.0, &mut empty);
    }

    #[test]
    fn row_duration_never_rolls_into_hours() {
        let mut buf = [0u8; 24];
        format_row_duration(185, &mut buf);
        assert_eq!(text(&buf), "3:05");
        format_row_duration(59, &mut buf);
        assert_eq!(text(&buf), "0:59");
        format_row_duration(3600, &mut buf);
        assert_eq!(text(&buf), "60:00");
        format_row_duration(7325, &mut buf);
        assert_eq!(text(&buf), "122:05");
    }

    #[test]
    fn row_duration_is_empty_without_a_length() {
        let mut buf = [0u8; 24];
        buf[0] = b'x';
        format_row_duration(0, &mut buf);
        assert_eq!(text(&buf), "");
        buf[0] = b'x';
        format_row_duration(-1, &mut buf);
        assert_eq!(text(&buf), "");
    }
}
