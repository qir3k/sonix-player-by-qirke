//! The date a tag carries, at whatever precision it carries it, ported from
//! `src/system/library/tagdate.c`.
//!
//! A date tag is a moment of any precision: `"2026"`, `"2026-03"`,
//! `"2026-03-12"`, `"2026-03-12T08:00:00"`. The parts the tag left out are zero
//! here rather than guessed at — a podcast episode dated only to its year must
//! not be shown as "1 January 2026" — and a caller that needs one moment to sort
//! by asks for the conversion, which settles the missing parts on purpose.
//!
//! [`to_time`] is not here: it needs `mktime`, and `mktime` needs libc and a
//! timezone. It lives in `sonix-ffi` beside the other platform calls.

/// The three parts, as many of them as the tag spelled out. Laid out to match
/// the C `tagdate_t` so a pointer can cross the FFI boundary unchanged.
#[derive(Copy, Clone, Default, Debug, PartialEq, Eq)]
#[repr(C)]
pub struct Date {
    pub year: i32,
    pub month: i32,
    pub day: i32,
}

impl Date {
    /// How many of the three parts this date names: the basis of [`better`],
    /// and of nothing else. The display formats itself from the fields that are
    /// there.
    pub fn precision(&self) -> i32 {
        if self.day > 0 {
            3
        } else if self.month > 0 {
            2
        } else if self.year > 0 {
            1
        } else {
            0
        }
    }
}

/// Takes a year, month and day as the tag spelled them, drops the parts that do
/// not name a real part of a date (month 13, day 0) and keeps the rest: the date
/// a tag gives is still better than the one it does not, and a file dated
/// `"2026-13"` is dated to its year however wrong the month is.
fn put_parts(year: i32, month: i32, day: i32) -> Date {
    let year = if (1..=9999).contains(&year) { year } else { 0 };
    let month = if year != 0 && (1..=12).contains(&month) {
        month
    } else {
        0
    };
    let day = if month != 0 && (1..=31).contains(&day) {
        day
    } else {
        0
    };
    Date { year, month, day }
}

/// `count` decimal digits at the start of `s`, or -1 when there are not that
/// many or one is not a digit. Reading past the end is -1, which is what the C
/// version gets from reading the terminator.
fn digit_at(s: &[u8], count: usize) -> i32 {
    let mut value = 0i32;
    for i in 0..count {
        let Some(&c) = s.get(i) else { return -1 };
        if !c.is_ascii_digit() {
            return -1;
        }
        value = value * 10 + i32::from(c - b'0');
    }
    value
}

/// The ISO shape, from just after the four year digits: a separator and a
/// month, then another separator and a day, each stopping at whatever follows
/// (a time, a zone, the far end of an ID3v2.4 range, or the end of the text).
fn parse_iso_rest(p: &[u8], year: i32) -> Date {
    let mut i = 0usize;
    let mut month = 0i32;
    let mut day = 0i32;

    if i < p.len() && (p[i] == b'-' || p[i] == b'/') {
        // One or two digits for each part: "2026-3-2" is written as often as
        // "2026-03-02", and both are the same date.
        i += 1;
        let two = digit_at(&p[i..], 2);
        let (m, take) = if two < 0 {
            (digit_at(&p[i..], 1), 1)
        } else {
            (two, 2)
        };
        if m >= 0 {
            month = m;
            i += take;
            if i < p.len() && (p[i] == b'-' || p[i] == b'/') {
                i += 1;
                day = digit_at(&p[i..], 2);
                if day < 0 {
                    day = digit_at(&p[i..], 1);
                }
            }
        }
        // "2026-" or "2026-x": a year and a stray mark, nothing more.
    }

    put_parts(year, month, day)
}

/// Reads the date out of a tag's text, whichever shape it comes in: ISO with
/// dashes (`"2026-03-12"`), the same with slashes, the compact `"20260312"`,
/// and any of them followed by a time.
///
/// What is missing stays missing: `"2026-13"` is a year, `"2026-02-99"` is a
/// year and a month, and a text with no date in it is `None`.
pub fn parse(text: &str) -> Option<Date> {
    let bytes = text.as_bytes();
    let mut start = 0usize;
    while start < bytes.len() && (bytes[start] == b' ' || bytes[start] == b'\t') {
        start += 1;
    }
    let text = &bytes[start..];

    // Digits first, and how many of them, before anything is cut apart: the two
    // shapes differ only in whether separators interrupt them.
    let digits = text.iter().take_while(|b| b.is_ascii_digit()).count();

    if digits == 8 {
        // The compact shape, "20260312". Only whole — "202603121" is not a date
        // — and never the opening of the ISO one, whose separators stop the
        // count at four.
        let date = put_parts(
            digit_at(text, 4),
            digit_at(&text[4..], 2),
            digit_at(&text[6..], 2),
        );
        return if date.year > 0 { Some(date) } else { None };
    }

    if digits == 4 {
        let date = parse_iso_rest(&text[4..], digit_at(text, 4));
        return if date.year > 0 { Some(date) } else { None };
    }

    None
}

/// True when `a` names more of the date than `b`.
///
/// A file can carry the same date twice at different precision — TYER's bare
/// year and TDRC's full moment in one ID3 tag — and the frames arrive in
/// whatever order the tagger wrote them. The fuller date wins; on a tie the
/// caller keeps what it has.
pub fn better(a: Option<&Date>, b: Option<&Date>) -> bool {
    fn precision(d: Option<&Date>) -> i32 {
        d.map(|d| d.precision()).unwrap_or(0)
    }
    precision(a) > precision(b)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn d(year: i32, month: i32, day: i32) -> Date {
        Date { year, month, day }
    }

    #[test]
    fn the_three_shapes() {
        assert_eq!(parse("2026-03-12"), Some(d(2026, 3, 12)));
        assert_eq!(parse("2026/03/12"), Some(d(2026, 3, 12)));
        assert_eq!(parse("20260312"), Some(d(2026, 3, 12)));
    }

    #[test]
    fn precision_is_whatever_the_tag_spelled() {
        assert_eq!(parse("2026"), Some(d(2026, 0, 0)));
        assert_eq!(parse("2026-03"), Some(d(2026, 3, 0)));
        assert_eq!(parse("2026-03-12"), Some(d(2026, 3, 12)));
    }

    #[test]
    fn one_or_two_digits_per_part() {
        assert_eq!(parse("2026-3-2"), Some(d(2026, 3, 2)));
        assert_eq!(parse("2026/3/12"), Some(d(2026, 3, 12)));
        assert_eq!(parse("2026-03-2"), Some(d(2026, 3, 2)));
    }

    #[test]
    fn anything_after_the_date_is_ignored() {
        assert_eq!(parse("2026-03-12T08:00:00"), Some(d(2026, 3, 12)));
        assert_eq!(parse("2026-03-12 08:00"), Some(d(2026, 3, 12)));
        assert_eq!(parse("2026-03-12Z"), Some(d(2026, 3, 12)));
        // An ID3v2.4 range: the first part is the date wanted.
        assert_eq!(parse("2026-03-12/2026-03-13"), Some(d(2026, 3, 12)));
    }

    #[test]
    fn impossible_parts_are_dropped_not_guessed() {
        assert_eq!(parse("2026"), Some(d(2026, 0, 0)));
        assert_eq!(parse("2026-13"), Some(d(2026, 0, 0)), "month 13");
        assert_eq!(parse("2026-00"), Some(d(2026, 0, 0)), "month zero");
        assert_eq!(
            parse("2026-02-99"),
            Some(d(2026, 2, 0)),
            "a month with no valid day left"
        );
        assert_eq!(parse("2026-02-00"), Some(d(2026, 2, 0)));
        assert_eq!(parse("2026-03-32"), Some(d(2026, 3, 0)));
    }

    #[test]
    fn years_outside_the_range_are_no_date() {
        assert_eq!(parse("0000"), None);
        assert_eq!(parse("0000-01-01"), None);
        assert_eq!(parse("10000-01-01"), None, "five digits is not this shape");
    }

    #[test]
    fn leading_space_and_tab_are_skipped() {
        assert_eq!(parse("  2026-03-12"), Some(d(2026, 3, 12)));
        assert_eq!(parse("\t20260312"), Some(d(2026, 3, 12)));
    }

    #[test]
    fn text_without_a_date() {
        for text in [
            "",
            " ",
            "n/a",
            "-2026-03-12",
            "12/03/2026",
            "202603121",
            "2026031",
        ] {
            let got = parse(text);
            assert_eq!(got, None, "expected no date from {text:?}");
        }
    }

    #[test]
    fn a_stray_separator_leaves_just_the_year() {
        assert_eq!(parse("2026-"), Some(d(2026, 0, 0)));
        assert_eq!(parse("2026-x"), Some(d(2026, 0, 0)));
        assert_eq!(parse("2026-03x"), Some(d(2026, 3, 0)));
    }

    #[test]
    fn the_fuller_date_wins() {
        let year = d(2026, 0, 0);
        let month = d(2026, 3, 0);
        let day = d(2026, 3, 12);
        assert!(better(Some(&day), Some(&month)));
        assert!(better(Some(&month), Some(&year)));
        assert!(better(Some(&year), Some(&Date::default())));
        assert!(!better(Some(&year), Some(&day)));
        assert!(!better(Some(&day), Some(&day)), "a tie keeps what it has");
        assert!(!better(None, None));
        assert!(better(Some(&year), None));
        assert!(!better(None, Some(&year)));
    }

    #[test]
    fn precision_counts_the_parts() {
        assert_eq!(Date::default().precision(), 0);
        assert_eq!(d(2026, 0, 0).precision(), 1);
        assert_eq!(d(2026, 3, 0).precision(), 2);
        assert_eq!(d(2026, 3, 12).precision(), 3);
    }
}
