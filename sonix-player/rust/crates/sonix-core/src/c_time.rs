//! The parts of libc's time handling the port calls, in one place.
//!
//! Dates on a tag are local calendar dates and "today" is a local date, so
//! turning either into or out of a number is libc's job: it is the only thing
//! that knows the device's timezone, and doing it again in Rust would be a
//! second implementation of the same rules.
//!
//! This module exists because `struct tm` is not the same shape everywhere, and
//! getting it wrong is silent until it crashes.

use core::ffi::{c_char, c_int};

/// C's `time_t`.
///
/// 32 bits on the device (glibc 2.22, mipsel) and 64 on Windows and on any
/// 64-bit target. A 32-bit Linux build with glibc's time64 transition would
/// disagree, which is why this matches the target and not the pointer width
/// alone.
#[cfg(any(target_os = "windows", target_pointer_width = "64"))]
pub type TimeT = i64;
#[cfg(all(not(target_os = "windows"), target_pointer_width = "32"))]
pub type TimeT = i32;

/// `struct tm`, as libc lays it out.
///
/// The first nine fields are in the same order everywhere. What follows them is
/// not the same size: glibc and musl add `tm_gmtoff` (a `long`) and `tm_zone` (a
/// pointer), which makes the struct 56 bytes on x86-64 Linux against the 36 that
/// nine ints add up to — and both `mktime` and `localtime_r` **write those two
/// fields back**.
///
/// Declaring only the nine put twenty bytes of glibc's answer into the caller's
/// frame: a segfault, and one that only happened on Linux, because MinGW's
/// `struct tm` really is the nine fields.
///
/// The tail is declared generously rather than exactly: it only has to be big
/// enough and correctly aligned, and `usize` gives both. Every libc this targets
/// puts the extra members at the end.
#[repr(C)]
pub struct Tm {
    pub tm_sec: c_int,
    pub tm_min: c_int,
    pub tm_hour: c_int,
    pub tm_mday: c_int,
    pub tm_mon: c_int,
    pub tm_year: c_int,
    pub tm_wday: c_int,
    pub tm_yday: c_int,
    pub tm_isdst: c_int,
    /// Room for `tm_gmtoff` and `tm_zone`. See above.
    tail: [usize; 4],
}

impl Tm {
    pub fn new() -> Self {
        Tm {
            tm_sec: 0,
            tm_min: 0,
            tm_hour: 0,
            tm_mday: 0,
            tm_mon: 0,
            tm_year: 0,
            tm_wday: 0,
            tm_yday: 0,
            tm_isdst: -1,
            tail: [0; 4],
        }
    }
}

impl Default for Tm {
    fn default() -> Self {
        Self::new()
    }
}

extern "C" {
    fn mktime(tm: *mut Tm) -> TimeT;
    fn strftime(buf: *mut c_char, max: usize, fmt: *const c_char, tm: *const Tm) -> usize;
}

#[cfg(unix)]
extern "C" {
    fn localtime_r(t: *const TimeT, tm: *mut Tm) -> *mut Tm;
}

#[cfg(windows)]
extern "C" {
    // The same call under the CRT's name, and with its arguments the other way
    // round: `errno_t _localtime64_s(struct tm *, const __time64_t *)`. minGW's
    // headers only declare `localtime_s` for the UCRT, so the 64-bit CRT name is
    // the one to bind to; it is the same function `localtime_s` resolves to.
    #[link_name = "_localtime64_s"]
    fn localtime_s(tm: *mut Tm, t: *const TimeT) -> c_int;
}

/// The moment a calendar date names, or 0 when there is no date or `mktime`
/// refused it.
pub fn to_time(year: c_int, month: c_int, day: c_int) -> TimeT {
    if year <= 0 {
        return 0;
    }
    let mut tm = Tm::new();
    tm.tm_mday = if day > 0 { day } else { 1 };
    tm.tm_mon = if month > 0 { month - 1 } else { 0 };
    tm.tm_year = year - 1900;

    let when = unsafe { mktime(&mut tm) };
    if when == -1 {
        0
    } else {
        when
    }
}

/// The local date `seconds` falls on, as `YYYY-MM-DD`.
///
/// Empty when the clock cannot be read, which callers turn into a fallback.
pub fn local_date(seconds: TimeT) -> Option<String> {
    let mut tm = Tm::new();

    #[cfg(unix)]
    let ok = unsafe { !localtime_r(&seconds, &mut tm).is_null() };
    #[cfg(windows)]
    let ok = unsafe { localtime_s(&mut tm, &seconds) == 0 };

    if !ok {
        return None;
    }

    let mut buf = [0u8; 32];
    let written = unsafe {
        strftime(
            buf.as_mut_ptr() as *mut c_char,
            buf.len(),
            c"%Y-%m-%d".as_ptr(),
            &tm,
        )
    };
    if written == 0 {
        return None;
    }
    Some(String::from_utf8_lossy(&buf[..written]).into_owned())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn struct_tm_has_room_for_what_libc_writes() {
        // glibc's is 56 bytes on LP64: nine ints, then tm_gmtoff and tm_zone.
        // Anything at or above that is safe; below it, mktime writes into the
        // caller's frame.
        assert!(core::mem::size_of::<Tm>() >= 56, "too small for glibc");
        assert_eq!(core::mem::align_of::<Tm>(), core::mem::align_of::<usize>());
    }

    #[test]
    fn a_missing_year_has_no_moment() {
        assert_eq!(to_time(0, 3, 12), 0);
        assert_eq!(to_time(-1, 3, 12), 0);
    }

    #[test]
    fn the_missing_parts_settle_on_the_first_of_the_month() {
        assert_eq!(to_time(2026, 3, 0), to_time(2026, 3, 1));
        assert!(to_time(2025, 12, 31) < to_time(2026, 3, 1));
    }

    #[test]
    fn today_reads_back_as_a_date() {
        let date = local_date(1_772_000_000).expect("a real moment");
        assert_eq!(date.len(), 10, "{date} is not YYYY-MM-DD");
        assert_eq!(date.as_bytes()[4], b'-');
        assert_eq!(date.as_bytes()[7], b'-');
        assert!(date[..4].chars().all(|c| c.is_ascii_digit()));
    }
}
