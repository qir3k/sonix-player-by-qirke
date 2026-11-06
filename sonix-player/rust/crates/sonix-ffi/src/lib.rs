//! The C ABI for the ported core.
//!
//! # What this is for
//!
//! A firmware built from `src/**/*.c` cannot be replaced by Rust in one commit.
//! This crate is the seam that lets it happen anyway: every symbol below is
//! named exactly as the C function it replaces, with the same signature and the
//! same struct layouts, so a C call site does not change when the definition
//! moves from C to Rust.
//!
//! Switching a module over is therefore two steps:
//!
//! 1. Delete the `.c` file from the build (the `[SYSTEM_SRCS]`/`[GUI_SRCS]`
//!    list in `sonix-player/Makefile`).
//! 2. Link `libsonix_core.a`.
//!
//! Leaving both in the build is a duplicate-symbol error at link time, which is
//! loud and immediate rather than a subtle half-port.
//!
//! # Symbols provided
//!
//! | C file | Symbols |
//! |---|---|
//! | `core/base64.c`   | `base64_decode` |
//! | `core/md5.c`      | `md5`, `md5_hex` |
//! | `core/sha1.c`     | `sha1`, `sha1_hex` |
//! | `core/sha256.c`   | `sha256_init`, `sha256_update`, `sha256_final_hex` |
//! | `core/config.c`   | `config_*`, `config_store_*` |
//! | `core/json.c`     | `json_*` |
//! | `core/utils.c`    | `utils_isqrt32`, `utils_isqrt64`, `has_extension`, `formatDoubleSeconds`, `formatDoubleProgress`, `format_row_duration` |
//! | `text/textfile.c` | `textfile_decode`, `textfile_read`, `textfile_write` |
//! | `library/tagdate.c` | `tagdate_parse`, `tagdate_better`, `tagdate_to_time` |
//! | `library/id3chap.c` | `id3chap_read`, `id3chap_present` |
//! | `addons/addons.c` | `addons_count`, `addons_at`, `addons_find`, `addons_enabled`, `addons_set`, `addons_apply_defaults`, `addons_any_game_enabled` |
//! | `notes/notesstore.c` | `notes_dir`, `notes_path`, `notes_is_note_name`, `notes_list`, `notes_read`, `notes_fits_editor`, `notes_file_name`, `notes_write`, `notes_name_ok`, `notes_display_name`, `notes_default_name` |
//!
//! `utils.c` is the one exception to "delete the file and link": it is a mixed
//! file, and the half below `format_row_duration` is pthreads, `mmap`,
//! `mallinfo` and `sigaltstack`, which has no business in a desktop build and no
//! Rust equivalent worth inventing. Its pure half is ported and exported here,
//! but the file cannot leave the build until that platform half moves to a
//! companion translation unit. Everything else in the table is a swap.
//!
//! # `long`
//!
//! C's `long` is 32 bits on the target (mipsel, o32) and 64 on a desktop
//! Linux. [`c_long`] follows the pointer width, which is what both C ABIs do,
//! so a `long` crosses this boundary unchanged in either build.

#![allow(clippy::missing_safety_doc)]
// The aliases below are the C typedefs' own names. Renaming them to Rust's
// casing would hide which C declaration each one has to keep matching, and
// that correspondence is the entire point of this crate.
#![allow(non_camel_case_types)]

use core::ffi::{c_char, c_int, c_void};
use core::ptr;
use core::slice;
use std::ffi::CStr;
use std::io::Read as _;
use std::sync::{Mutex, OnceLock};

use sonix_core::{addons, base64, config, hash, id3chap, json, notes, tagdate, text, utils};

/// C's `long`.
///
/// Pointer width is *not* enough to decide this. Windows is LLP64: `long` is
/// 32 bits even in a 64-bit build, and every other target this firmware has is
/// LP64 or ILP32. Getting this wrong is silent — the C caller pushes a 32-bit
/// value and the Rust side reads 64 bits of register — and it is exactly what
/// the differential harness caught.
#[cfg(any(target_os = "windows", target_pointer_width = "32"))]
pub type c_long = i32;
/// C's `long` on an LP64 target.
#[cfg(all(not(target_os = "windows"), target_pointer_width = "64"))]
pub type c_long = i64;

/// `size_t` is `usize`; C's `bool` is `_Bool`, which Rust's `bool` matches.
type size_t = usize;

extern "C" {
    // The C heap, deliberately: `base64_decode` hands back a buffer that C
    // frees, and `json_tok_t` arrays live where a C `json_free` expects them.
    //
    // Declared with `c_void`, the way libc declares them: declaring them with
    // `*mut u8` is ABI-identical but makes the compiler warn that the runtime's
    // own `malloc`/`free` are being redeclared with a different signature, and a
    // staticlib links against libc's.
    fn malloc(size: size_t) -> *mut c_void;
    fn free(ptr: *mut c_void);
}

/// C's `time_t`, and `struct tm` with it: see [`sonix_core::c_time`], which is
/// where the layout and the libc calls live so that there is only one copy of
/// each.
pub use sonix_core::c_time::TimeT as c_time_t;

/// A C path, byte for byte on POSIX.
#[cfg(unix)]
fn path_from_bytes(bytes: &[u8]) -> std::path::PathBuf {
    use std::os::unix::ffi::OsStrExt;
    std::path::Path::new(std::ffi::OsStr::from_bytes(bytes)).to_path_buf()
}

/// The same on Windows, where a path is UTF-16 and the bytes have to be text.
#[cfg(not(unix))]
fn path_from_bytes(bytes: &[u8]) -> std::path::PathBuf {
    std::path::PathBuf::from(String::from_utf8_lossy(bytes).into_owned())
}

/// Reads a C string as bytes, or an empty slice for a null pointer.
unsafe fn c_bytes<'a>(s: *const c_char) -> &'a [u8] {
    if s.is_null() {
        &[]
    } else {
        CStr::from_ptr(s).to_bytes()
    }
}

/// Reads a C string, or `""`.
unsafe fn c_str<'a>(s: *const c_char) -> &'a str {
    core::str::from_utf8(c_bytes(s)).unwrap_or("")
}

// ---------------------------------------------------------------------------
// base64
// ---------------------------------------------------------------------------

/// Decodes `text_len` characters into a freshly allocated buffer the caller
/// frees. Null when nothing comes out, when the result would exceed
/// `max_bytes`, or when memory runs out.
///
/// The buffer carries one extra zero byte past `*out_size`: a Tidal manifest is
/// text (JSON or XML) and its recipient hands it to a parser that wants a
/// terminated string.
#[no_mangle]
pub unsafe extern "C" fn base64_decode(
    text: *const c_char,
    text_len: size_t,
    max_bytes: size_t,
    out_size: *mut size_t,
) -> *mut u8 {
    if out_size.is_null() {
        return ptr::null_mut();
    }
    *out_size = 0;
    if text.is_null() || text_len == 0 {
        return ptr::null_mut();
    }

    let input = slice::from_raw_parts(text as *const u8, text_len);
    let Some(decoded) = base64::decode(input, max_bytes) else {
        return ptr::null_mut();
    };

    let out = malloc(decoded.len() + 1) as *mut u8;
    if out.is_null() {
        return ptr::null_mut();
    }
    ptr::copy_nonoverlapping(decoded.as_ptr(), out, decoded.len());
    *out.add(decoded.len()) = 0;
    *out_size = decoded.len();
    out
}

// ---------------------------------------------------------------------------
// md5 / sha1 / sha256
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn md5(data: *const c_void, len: size_t, out: *mut u8) {
    if out.is_null() {
        return;
    }
    let bytes = if data.is_null() || len == 0 {
        &[][..]
    } else {
        slice::from_raw_parts(data as *const u8, len)
    };
    ptr::copy_nonoverlapping(hash::md5(bytes).as_ptr(), out, hash::MD5_DIGEST_LEN);
}

#[no_mangle]
pub unsafe extern "C" fn md5_hex(data: *const c_void, len: size_t, out: *mut c_char) {
    if out.is_null() {
        return;
    }
    let bytes = if data.is_null() || len == 0 {
        &[][..]
    } else {
        slice::from_raw_parts(data as *const u8, len)
    };
    let hex = hash::md5_hex(bytes);
    debug_assert_eq!(hex.len(), 32);
    ptr::copy_nonoverlapping(hex.as_ptr(), out as *mut u8, hex.len());
    *out.add(hex.len()) = 0;
}

#[no_mangle]
pub unsafe extern "C" fn sha1(data: *const c_void, len: size_t, out: *mut u8) {
    if out.is_null() {
        return;
    }
    let bytes = if data.is_null() || len == 0 {
        &[][..]
    } else {
        slice::from_raw_parts(data as *const u8, len)
    };
    ptr::copy_nonoverlapping(hash::sha1(bytes).as_ptr(), out, hash::SHA1_DIGEST_LEN);
}

#[no_mangle]
pub unsafe extern "C" fn sha1_hex(data: *const c_void, len: size_t, out: *mut c_char) {
    if out.is_null() {
        return;
    }
    let bytes = if data.is_null() || len == 0 {
        &[][..]
    } else {
        slice::from_raw_parts(data as *const u8, len)
    };
    let hex = hash::sha1_hex(bytes);
    debug_assert_eq!(hex.len(), 40);
    ptr::copy_nonoverlapping(hex.as_ptr(), out as *mut u8, hex.len());
    *out.add(hex.len()) = 0;
}

/// The C `sha256_ctx`. `sonix_core::hash::Sha256Ctx` is `#[repr(C)]` and
/// field-for-field the same, so a C context on the stack is a valid one here
/// and neither side copies anything.
pub type sha256_ctx = hash::Sha256Ctx;

#[no_mangle]
pub unsafe extern "C" fn sha256_init(c: *mut sha256_ctx) {
    if c.is_null() {
        return;
    }
    ptr::write(c, sha256_ctx::new());
}

#[no_mangle]
pub unsafe extern "C" fn sha256_update(c: *mut sha256_ctx, data: *const c_void, len: size_t) {
    if c.is_null() {
        return;
    }
    let bytes = if data.is_null() || len == 0 {
        &[][..]
    } else {
        slice::from_raw_parts(data as *const u8, len)
    };
    (*c).update(bytes);
}

/// Writes the digest as lower-case hex. The context is spent afterwards, as the
/// C one was.
#[no_mangle]
pub unsafe extern "C" fn sha256_final_hex(c: *mut sha256_ctx, out: *mut c_char) {
    if c.is_null() || out.is_null() {
        return;
    }
    let ctx = ptr::read(c);
    let hex = ctx.finalize_hex();
    debug_assert_eq!(hex.len(), 64);
    ptr::copy_nonoverlapping(hex.as_ptr(), out as *mut u8, hex.len());
    *out.add(hex.len()) = 0;
}

// ---------------------------------------------------------------------------
// config
// ---------------------------------------------------------------------------

/// Opaque to C, as `config_store_t` always was.
#[repr(C)]
pub struct config_store_t {
    _private: [u8; 0],
}

static MAIN_STORE: OnceLock<Mutex<config::Store>> = OnceLock::new();
static EBOOK_STORE: OnceLock<Mutex<config::Store>> = OnceLock::new();

fn main_store() -> &'static Mutex<config::Store> {
    MAIN_STORE.get_or_init(|| Mutex::new(config::Store::new(config::MAX_ENTRIES)))
}

fn ebook_store() -> &'static Mutex<config::Store> {
    EBOOK_STORE.get_or_init(|| Mutex::new(config::Store::new(config::EBOOK_MAX_ENTRIES)))
}

fn handle_of(store: &'static Mutex<config::Store>) -> *mut config_store_t {
    store as *const Mutex<config::Store> as *mut config_store_t
}

fn store_of(handle: *mut config_store_t) -> Option<&'static Mutex<config::Store>> {
    let main = handle_of(main_store());
    let ebook = handle_of(ebook_store());
    if handle == main {
        Some(main_store())
    } else if handle == ebook {
        Some(ebook_store())
    } else {
        None
    }
}

/// Loads the file. A missing or unreadable file is not an error: the defaults
/// passed to the getters are used and the file appears on the first save.
#[no_mangle]
pub unsafe extern "C" fn config_store_init(
    store: *mut config_store_t,
    path: *const c_char,
) -> bool {
    let Some(slot) = store_of(store) else {
        return false;
    };
    let mut s = slot.lock().unwrap();
    s.clear();

    if path.is_null() {
        return false;
    }
    let path = c_str(path);
    if path.is_empty() || !s.set_path(path) {
        return false;
    }

    match std::fs::read(path) {
        Ok(bytes) => {
            s.load(&bytes);
            true
        }
        Err(_) => {
            // The file is not there yet; defaults apply and the first save
            // creates it.
            false
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn config_init(path: *const c_char) -> bool {
    config_store_init(handle_of(main_store()), path)
}

#[no_mangle]
pub extern "C" fn config_main_store() -> *mut config_store_t {
    handle_of(main_store())
}

#[no_mangle]
pub extern "C" fn config_ebook_store() -> *mut config_store_t {
    handle_of(ebook_store())
}

/// The path as a C string, or null if `config_init` was never called.
///
/// The bytes live in the store for as long as the store does, which is the
/// whole life of the process, so this pointer stays valid.
#[no_mangle]
pub unsafe extern "C" fn config_path() -> *const c_char {
    config_store_path(handle_of(main_store()))
}

#[no_mangle]
pub unsafe extern "C" fn config_store_path(store: *mut config_store_t) -> *const c_char {
    let Some(slot) = store_of(store) else {
        return ptr::null();
    };
    let s = slot.lock().unwrap();
    match s.path() {
        Some(p) => p.as_ptr() as *const c_char,
        None => ptr::null(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn config_store_get(
    store: *mut config_store_t,
    section: *const c_char,
    key: *const c_char,
    fallback: *const c_char,
) -> *const c_char {
    let Some(slot) = store_of(store) else {
        return fallback;
    };
    let s = slot.lock().unwrap();
    match s.value_ptr(c_str(section), c_str(key)) {
        Some(p) => p as *const c_char,
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn config_get(
    section: *const c_char,
    key: *const c_char,
    fallback: *const c_char,
) -> *const c_char {
    config_store_get(handle_of(main_store()), section, key, fallback)
}

#[no_mangle]
pub unsafe extern "C" fn config_store_get_int(
    store: *mut config_store_t,
    section: *const c_char,
    key: *const c_char,
    fallback: c_long,
) -> c_long {
    let Some(slot) = store_of(store) else {
        return fallback;
    };
    let s = slot.lock().unwrap();
    match s.get_int(c_str(section), c_str(key)) {
        Some(v) => v as c_long,
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn config_get_int(
    section: *const c_char,
    key: *const c_char,
    fallback: c_long,
) -> c_long {
    config_store_get_int(handle_of(main_store()), section, key, fallback)
}

#[no_mangle]
pub unsafe extern "C" fn config_get_bool(
    section: *const c_char,
    key: *const c_char,
    fallback: bool,
) -> bool {
    let slot = main_store();
    let s = slot.lock().unwrap();
    s.get_bool(c_str(section), c_str(key)).unwrap_or(fallback)
}

#[no_mangle]
pub unsafe extern "C" fn config_store_set(
    store: *mut config_store_t,
    section: *const c_char,
    key: *const c_char,
    value: *const c_char,
) {
    let Some(slot) = store_of(store) else {
        return;
    };
    if section.is_null() || key.is_null() || value.is_null() {
        return;
    }
    let mut s = slot.lock().unwrap();
    s.set(c_str(section), c_str(key), c_str(value));
}

#[no_mangle]
pub unsafe extern "C" fn config_set(
    section: *const c_char,
    key: *const c_char,
    value: *const c_char,
) {
    config_store_set(handle_of(main_store()), section, key, value);
}

#[no_mangle]
pub unsafe extern "C" fn config_store_set_int(
    store: *mut config_store_t,
    section: *const c_char,
    key: *const c_char,
    value: c_long,
) {
    let Some(slot) = store_of(store) else {
        return;
    };
    let mut s = slot.lock().unwrap();
    // `long` is 32 bits on the target and 64 on a desktop build; the store's
    // getter reads back an i32 either way, so clamp rather than truncate.
    let value = value.clamp(i32::MIN as c_long, i32::MAX as c_long) as i32;
    s.set_int(c_str(section), c_str(key), value);
}

#[no_mangle]
pub unsafe extern "C" fn config_set_int(section: *const c_char, key: *const c_char, value: c_long) {
    config_store_set_int(handle_of(main_store()), section, key, value);
}

#[no_mangle]
pub unsafe extern "C" fn config_set_bool(section: *const c_char, key: *const c_char, value: bool) {
    config_store_set(handle_of(main_store()), section, key, bool_ptr(value));
}

/// A `const char *` to the literal "1" or "0", so `config_set_bool` can reuse
/// `config_set` without a temporary buffer.
fn bool_ptr(value: bool) -> *const c_char {
    if value {
        c"1".as_ptr()
    } else {
        c"0".as_ptr()
    }
}

/// Writes the file if anything changed since the last save. Returns false when
/// the file could not be written (read-only filesystem, no space).
///
/// Written beside the real file and renamed over it. Rename is atomic, so a
/// power cut leaves either the old settings or the new ones, never half a file,
/// which on the next boot would read as no settings at all.
#[no_mangle]
pub unsafe extern "C" fn config_store_save(store: *mut config_store_t) -> bool {
    let Some(slot) = store_of(store) else {
        return false;
    };
    let mut s = slot.lock().unwrap();

    let Some(path) = s.path() else {
        return true; // never initialised: nothing to save
    };
    if !s.is_dirty() {
        return true;
    }

    let temp = format!("{path}.tmp");
    let bytes = s.render();

    if std::fs::write(&temp, &bytes).is_err() {
        let _ = std::fs::remove_file(&temp);
        return false;
    }
    if std::fs::rename(&temp, path).is_err() {
        let _ = std::fs::remove_file(&temp);
        return false;
    }

    s.mark_clean();
    true
}

#[no_mangle]
pub unsafe extern "C" fn config_save() -> bool {
    config_store_save(handle_of(main_store()))
}

/// Moves every key of `section` from one store to the other and returns how
/// many moved.
#[no_mangle]
pub unsafe extern "C" fn config_store_move_section(
    from: *mut config_store_t,
    to: *mut config_store_t,
    section: *const c_char,
) -> c_int {
    let Some(from_slot) = store_of(from) else {
        return 0;
    };
    let Some(to_slot) = store_of(to) else {
        return 0;
    };
    if section.is_null() {
        return 0;
    }
    let section = c_str(section);

    // Two locks, always in the same order, so the main store is never waiting
    // on the reader's and vice versa.
    let (first, second) = if (from_slot as *const _) < (to_slot as *const _) {
        (from_slot, to_slot)
    } else {
        (to_slot, from_slot)
    };
    let a = first.lock().unwrap();
    let b = second.lock().unwrap();
    let (mut from_store, mut to_store) = if ptr::eq(from_slot, first) {
        (a, b)
    } else {
        (b, a)
    };

    from_store.move_section(&mut to_store, section) as c_int
}

// ---------------------------------------------------------------------------
// json
// ---------------------------------------------------------------------------

/// The C `json_type_t`.
pub type json_type_t = c_int;

/// The C `json_tok_t`, field for field.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct json_tok_t {
    pub r#type: json_type_t,
    pub start: c_int,
    pub end: c_int,
    pub size: c_int,
    pub parent: c_int,
}

/// The C `json_doc_t`, field for field. The document holds a pointer to the
/// text, not a copy: the text must stay alive and unmodified for as long as the
/// document is read.
#[repr(C)]
pub struct json_doc_t {
    pub text: *const c_char,
    pub toks: *mut json_tok_t,
    pub count: c_int,
}

impl json_doc_t {
    const EMPTY: json_doc_t = json_doc_t {
        text: ptr::null(),
        toks: ptr::null_mut(),
        count: 0,
    };
}

/// Builds a read-only view over a document C is holding.
unsafe fn view<'a>(doc: *const json_doc_t) -> Option<json::Doc<'a>> {
    if doc.is_null() {
        return None;
    }
    let d = &*doc;
    if d.toks.is_null() || d.count <= 0 {
        return None;
    }
    let toks = slice::from_raw_parts(d.toks as *const json::Tok, d.count as usize);
    let text: &[u8] = if d.text.is_null() {
        &[]
    } else {
        CStr::from_ptr(d.text).to_bytes()
    };
    Some(json::Doc::from_parts(text, toks))
}

/// Parses `text`. False on malformed JSON, out of memory, or nesting past the
/// limit. On success the caller must call `json_free`.
#[no_mangle]
pub unsafe extern "C" fn json_parse(text: *const c_char, doc: *mut json_doc_t) -> bool {
    json_parse_limited(text, doc, c_int::MAX)
}

/// The same with at most `max_tokens` token objects, for unpaginated endpoints
/// on memory-limited devices.
#[no_mangle]
pub unsafe extern "C" fn json_parse_limited(
    text: *const c_char,
    doc: *mut json_doc_t,
    max_tokens: c_int,
) -> bool {
    if doc.is_null() {
        return false;
    }
    ptr::write(doc, json_doc_t::EMPTY);

    if text.is_null() || max_tokens <= 0 {
        return false;
    }

    let bytes = CStr::from_ptr(text).to_bytes();
    let Some(parsed) = json::Doc::parse_limited(bytes, max_tokens as usize) else {
        return false;
    };

    let count = parsed.token_count();
    let toks = malloc(count * core::mem::size_of::<json::Tok>()) as *mut json_tok_t;
    if toks.is_null() {
        return false;
    }

    // `json::Tok` is `#[repr(C)]` with the same five `int` fields as
    // `json_tok_t`, so this is the same bytes, not a conversion.
    ptr::copy_nonoverlapping(parsed.tokens().as_ptr() as *const json_tok_t, toks, count);

    doc.write(json_doc_t {
        text,
        toks,
        count: count as c_int,
    });
    true
}

#[no_mangle]
pub unsafe extern "C" fn json_free(doc: *mut json_doc_t) {
    if doc.is_null() {
        return;
    }
    let d = &mut *doc;
    if !d.toks.is_null() {
        free(d.toks as *mut c_void);
    }
    *d = json_doc_t::EMPTY;
}

/// The root token, or -1 when the document is empty.
#[no_mangle]
pub unsafe extern "C" fn json_root(doc: *const json_doc_t) -> c_int {
    if doc.is_null() || (*doc).count <= 0 {
        return -1;
    }
    0
}

/// The value of `key` in object `obj`, or -1.
#[no_mangle]
pub unsafe extern "C" fn json_get(doc: *const json_doc_t, obj: c_int, key: *const c_char) -> c_int {
    match view(doc) {
        Some(v) => v.get(obj, c_str(key)),
        None => -1,
    }
}

/// Element `index` of array `arr`, or -1.
#[no_mangle]
pub unsafe extern "C" fn json_at(doc: *const json_doc_t, arr: c_int, index: c_int) -> c_int {
    match view(doc) {
        Some(v) => v.at(arr, index),
        None => -1,
    }
}

/// Element count of an array (or pair count of an object); 0 for anything else.
#[no_mangle]
pub unsafe extern "C" fn json_len(doc: *const json_doc_t, tok: c_int) -> c_int {
    match view(doc) {
        Some(v) => v.len(tok) as c_int,
        None => 0,
    }
}

/// Walks a dotted path: `json_path(doc, root, "tracks.items")` is two
/// `json_get` calls. A number between dots indexes an array.
#[no_mangle]
pub unsafe extern "C" fn json_path(
    doc: *const json_doc_t,
    from: c_int,
    path: *const c_char,
) -> c_int {
    match view(doc) {
        Some(v) => v.path(from, c_str(path)),
        None => -1,
    }
}

/// Copies the string with escapes resolved. Truncates to `size`; false when the
/// token is not a string, leaving `out` empty.
#[no_mangle]
pub unsafe extern "C" fn json_str(
    doc: *const json_doc_t,
    tok: c_int,
    out: *mut c_char,
    size: size_t,
) -> bool {
    json_string(doc, tok, out, size, false)
}

/// Identity/credential extraction: reject truncation, embedded NUL or unpaired
/// Unicode surrogates, leaving `out` empty.
#[no_mangle]
pub unsafe extern "C" fn json_str_exact(
    doc: *const json_doc_t,
    tok: c_int,
    out: *mut c_char,
    size: size_t,
) -> bool {
    json_string(doc, tok, out, size, true)
}

unsafe fn json_string(
    doc: *const json_doc_t,
    tok: c_int,
    out: *mut c_char,
    size: size_t,
    exact: bool,
) -> bool {
    if out.is_null() || size == 0 {
        return false;
    }
    let buffer = slice::from_raw_parts_mut(out as *mut u8, size);
    match view(doc) {
        Some(v) => v.string_into(tok, buffer, exact).is_some(),
        None => {
            buffer[0] = 0;
            false
        }
    }
}

/// Clamps a parsed integer to this target's `long`, which is what `strtol`
/// would have done.
///
/// The device's `long` is 32 bits, so an id past `i32` saturates there; on an
/// LP64 host `long` is the whole 64 and nothing saturates. Hard-coding 32 was
/// wrong: the differential harness found it the first time it ran on Linux,
/// where C's `json_long` was returning five-billion values that Rust had
/// clamped to `i32::MAX`.
#[cfg(any(target_os = "windows", target_pointer_width = "32"))]
fn clamp_to_c_long(v: i64) -> c_long {
    v.clamp(i64::from(c_long::MIN), i64::from(c_long::MAX)) as c_long
}

/// On an LP64 target `long` is already 64 bits wide.
#[cfg(all(not(target_os = "windows"), target_pointer_width = "64"))]
fn clamp_to_c_long(v: i64) -> c_long {
    v
}

#[no_mangle]
pub unsafe extern "C" fn json_long(doc: *const json_doc_t, tok: c_int, fallback: c_long) -> c_long {
    match view(doc) {
        Some(v) => clamp_to_c_long(v.llong(tok, i64::from(fallback))),
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn json_llong(doc: *const json_doc_t, tok: c_int, fallback: i64) -> i64 {
    match view(doc) {
        Some(v) => v.llong(tok, fallback),
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn json_double(doc: *const json_doc_t, tok: c_int, fallback: f64) -> f64 {
    match view(doc) {
        Some(v) => v.double(tok, fallback),
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn json_bool(doc: *const json_doc_t, tok: c_int, fallback: bool) -> bool {
    match view(doc) {
        Some(v) => v.boolean(tok, fallback),
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn json_obj_str(
    doc: *const json_doc_t,
    obj: c_int,
    key: *const c_char,
    out: *mut c_char,
    size: size_t,
) -> bool {
    match view(doc) {
        Some(v) => json_str(doc, v.get(obj, c_str(key)), out, size),
        None => {
            if !out.is_null() && size > 0 {
                *out = 0;
            }
            false
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn json_obj_long(
    doc: *const json_doc_t,
    obj: c_int,
    key: *const c_char,
    fallback: c_long,
) -> c_long {
    match view(doc) {
        Some(v) => clamp_to_c_long(v.obj_llong(obj, c_str(key), i64::from(fallback))),
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn json_obj_llong(
    doc: *const json_doc_t,
    obj: c_int,
    key: *const c_char,
    fallback: i64,
) -> i64 {
    match view(doc) {
        Some(v) => v.obj_llong(obj, c_str(key), fallback),
        None => fallback,
    }
}

#[no_mangle]
pub unsafe extern "C" fn json_obj_bool(
    doc: *const json_doc_t,
    obj: c_int,
    key: *const c_char,
    fallback: bool,
) -> bool {
    match view(doc) {
        Some(v) => v.obj_boolean(obj, c_str(key), fallback),
        None => fallback,
    }
}

// ---------------------------------------------------------------------------
// utils
// ---------------------------------------------------------------------------

#[no_mangle]
pub extern "C" fn utils_isqrt32(v: u32) -> u32 {
    utils::isqrt32(v)
}

#[no_mangle]
pub extern "C" fn utils_isqrt64(v: u64) -> u64 {
    utils::isqrt64(v)
}

#[no_mangle]
pub unsafe extern "C" fn has_extension(name: *const c_char, ext: *const c_char) -> bool {
    utils::has_extension(c_str(name), c_str(ext))
}

/// `snprintf` returns what the text wanted, not what fitted; so does this.
#[no_mangle]
pub unsafe extern "C" fn formatDoubleSeconds(
    total_seconds: f64,
    buffer: *mut c_char,
    max_len: size_t,
) -> c_int {
    // `snprintf(NULL, 0, ...)` is legal C and means "just tell me the length".
    let wanted = if buffer.is_null() || max_len == 0 {
        utils::format_double_seconds(total_seconds, &mut [])
    } else {
        let out = slice::from_raw_parts_mut(buffer as *mut u8, max_len);
        utils::format_double_seconds(total_seconds, out)
    };
    wanted as c_int
}

#[no_mangle]
pub unsafe extern "C" fn formatDoubleProgress(
    current_secs: f64,
    total_secs: f64,
    buffer: *mut c_char,
    max_len: size_t,
) {
    if buffer.is_null() || max_len == 0 {
        return;
    }
    let out = slice::from_raw_parts_mut(buffer as *mut u8, max_len);
    utils::format_double_progress(current_secs, total_secs, out);
}

#[no_mangle]
pub unsafe extern "C" fn format_row_duration(seconds: c_int, out: *mut c_char, size: size_t) {
    if out.is_null() || size == 0 {
        return;
    }
    utils::format_row_duration(seconds, slice::from_raw_parts_mut(out as *mut u8, size));
}

// ---------------------------------------------------------------------------
// textfile
// ---------------------------------------------------------------------------

/// Decodes to UTF-8 in a freshly allocated buffer the caller frees.
///
/// Never null for a null or empty input: an empty file is an empty string, not
/// a failure, and the C version allocates for it too.
#[no_mangle]
pub unsafe extern "C" fn textfile_decode(raw: *const u8, len: size_t) -> *mut c_char {
    let input = if raw.is_null() || len == 0 {
        &[][..]
    } else {
        slice::from_raw_parts(raw, len)
    };

    let decoded = text::decode(input);
    let out = malloc(decoded.len() + 1) as *mut u8;
    if out.is_null() {
        return ptr::null_mut();
    }
    if !decoded.is_empty() {
        ptr::copy_nonoverlapping(decoded.as_ptr(), out, decoded.len());
    }
    *out.add(decoded.len()) = 0;
    out as *mut c_char
}

/// Reads up to `max` bytes of a file. `*truncated` says whether there was more.
///
/// The buffer is `max` bytes (or one, when `max` is zero) and belongs to the
/// caller, exactly as the C version's does.
#[no_mangle]
pub unsafe extern "C" fn textfile_read(
    path: *const c_char,
    max: size_t,
    raw: *mut *mut u8,
    bytes: *mut size_t,
    truncated: *mut bool,
) -> bool {
    if raw.is_null() || bytes.is_null() || truncated.is_null() {
        return false;
    }
    *raw = ptr::null_mut();
    *bytes = 0;
    *truncated = false;

    let path = path_from_bytes(c_bytes(path));
    let Ok(mut file) = std::fs::File::open(&path) else {
        return false;
    };

    let cap = max.max(1);
    let mut buf = vec![0u8; cap];
    let mut got = 0usize;
    while got < max {
        match file.read(&mut buf[got..max]) {
            Ok(0) => break,
            Ok(n) => got += n,
            Err(ref e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(_) => return false,
        }
    }

    // One more byte is how "the file was longer than we read" is answered.
    let mut probe = [0u8; 1];
    let more = matches!(file.read(&mut probe), Ok(1));

    let buffer = malloc(cap) as *mut u8;
    if buffer.is_null() {
        return false;
    }
    if got > 0 {
        ptr::copy_nonoverlapping(buf.as_ptr(), buffer, got);
    }

    *raw = buffer;
    *bytes = got;
    *truncated = more;
    true
}

/// Writes `text` to `path` through a temporary file, so a card pulled out
/// mid-write leaves the old file whole instead of a truncated one.
#[no_mangle]
pub unsafe extern "C" fn textfile_write(path: *const c_char, text: *const c_char) -> bool {
    if path.is_null() {
        return false;
    }
    text::write_text_file(&path_from_bytes(c_bytes(path)), c_bytes(text))
}

// ---------------------------------------------------------------------------
// tagdate
// ---------------------------------------------------------------------------

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct tagdate_t {
    pub year: c_int,
    pub month: c_int,
    pub day: c_int,
}

impl tagdate_t {
    fn as_date(&self) -> tagdate::Date {
        tagdate::Date {
            year: self.year,
            month: self.month,
            day: self.day,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn tagdate_parse(text: *const c_char, out: *mut tagdate_t) -> bool {
    if text.is_null() || out.is_null() {
        return false;
    }
    *out = tagdate_t::default();
    match tagdate::parse(c_str(text)) {
        Some(date) => {
            (*out).year = date.year;
            (*out).month = date.month;
            (*out).day = date.day;
            true
        }
        None => false,
    }
}

#[no_mangle]
pub unsafe extern "C" fn tagdate_better(a: *const tagdate_t, b: *const tagdate_t) -> bool {
    let a = a.as_ref().map(tagdate_t::as_date);
    let b = b.as_ref().map(tagdate_t::as_date);
    tagdate::better(a.as_ref(), b.as_ref())
}
#[no_mangle]
pub unsafe extern "C" fn tagdate_to_time(d: *const tagdate_t) -> c_time_t {
    if d.is_null() {
        return 0;
    }
    sonix_core::c_time::to_time((*d).year, (*d).month, (*d).day)
}

// ---------------------------------------------------------------------------
// id3chap
// ---------------------------------------------------------------------------

#[repr(C)]
#[derive(Copy, Clone)]
pub struct id3chap_t {
    pub start: f64,
    pub title: [u8; 128],
}

impl Default for id3chap_t {
    fn default() -> Self {
        id3chap_t {
            start: 0.0,
            title: [0; 128],
        }
    }
}

/// Reads up to `max` chapters into `out`. A null `out` only asks whether the
/// file has any, and stops at the first one.
#[no_mangle]
pub unsafe extern "C" fn id3chap_read(
    path: *const c_char,
    out: *mut id3chap_t,
    max: c_int,
) -> c_int {
    if path.is_null() {
        return 0;
    }
    let path = path_from_bytes(c_bytes(path));

    let sink = if out.is_null() {
        None
    } else {
        // A non-null pointer with max <= 0 still counts every chapter — the C
        // only stops early when there is nowhere to write at all.
        Some(slice::from_raw_parts_mut(
            out as *mut id3chap::Chapter,
            if max > 0 { max as usize } else { 0 },
        ))
    };

    id3chap::read_file(&path, sink) as c_int
}

#[no_mangle]
pub unsafe extern "C" fn id3chap_present(path: *const c_char) -> bool {
    if path.is_null() {
        return false;
    }
    id3chap::present(&path_from_bytes(c_bytes(path)))
}

// ---------------------------------------------------------------------------
// addons
// ---------------------------------------------------------------------------

const ADDONS_SECTION: &CStr = c"addons";

#[no_mangle]
pub extern "C" fn addons_count() -> c_int {
    addons::count() as c_int
}

#[no_mangle]
pub extern "C" fn addons_at(index: c_int) -> *const addons::Addon {
    match addons::at(index) {
        Some(addon) => addon as *const _,
        None => ptr::null(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn addons_find(id: *const c_char) -> *const addons::Addon {
    if id.is_null() {
        return ptr::null();
    }
    match addons::find(c_str(id)) {
        Some(addon) => addon as *const _,
        None => ptr::null(),
    }
}

/// Whether a module is switched on. Unknown ids answer true: a gate is a way to
/// leave something out, and a misspelt one must not quietly remove a feature.
#[no_mangle]
pub unsafe extern "C" fn addons_enabled(id: *const c_char) -> bool {
    if id.is_null() {
        return true;
    }
    let fallback = addons::is_default_on(c_str(id));
    config_get_bool(ADDONS_SECTION.as_ptr(), id, fallback)
}

/// Switches one on or off and stores it. False if the setting could not be
/// stored, or the id is not a module.
#[no_mangle]
pub unsafe extern "C" fn addons_set(id: *const c_char, on: bool) -> bool {
    if id.is_null() || addons_find(id).is_null() {
        return false;
    }
    let previous = addons_enabled(id);
    config_set_bool(ADDONS_SECTION.as_ptr(), id, on);
    if addons_enabled(id) != on || !config_save() {
        config_set_bool(ADDONS_SECTION.as_ptr(), id, previous);
        config_save();
        return false;
    }
    true
}

#[no_mangle]
pub unsafe extern "C" fn addons_apply_defaults(
    is_default: Option<extern "C" fn(*const c_char) -> bool>,
) {
    let Some(is_default) = is_default else {
        return;
    };
    for addon in addons::table() {
        config_set_bool(ADDONS_SECTION.as_ptr(), addon.id, is_default(addon.id));
    }
    config_save();
}

#[no_mangle]
pub unsafe extern "C" fn addons_any_game_enabled() -> bool {
    let mut buf = [0 as c_char; 32];
    for id in addons::game_ids() {
        let bytes = id.as_bytes();
        if bytes.len() + 1 > buf.len() {
            continue;
        }
        for (i, &b) in bytes.iter().enumerate() {
            buf[i] = b as c_char;
        }
        buf[bytes.len()] = 0;
        if addons_enabled(buf.as_ptr()) {
            return true;
        }
    }
    false
}

// ---------------------------------------------------------------------------
// notes
// ---------------------------------------------------------------------------

/// `snprintf` into a caller's buffer: truncate, terminate, and say whether the
/// whole thing fitted.
unsafe fn write_c_string(text: &str, out: *mut c_char, size: size_t) -> bool {
    if out.is_null() || size == 0 {
        return false;
    }
    let bytes = text.as_bytes();
    let n = bytes.len().min(size - 1);
    if n > 0 {
        ptr::copy_nonoverlapping(bytes.as_ptr(), out as *mut u8, n);
    }
    *(out as *mut u8).add(n) = 0;
    bytes.len() < size
}

#[repr(C)]
pub struct note_entry_t {
    pub name: [c_char; notes::NAME_MAX + 1],
}

impl Default for note_entry_t {
    fn default() -> Self {
        note_entry_t {
            name: [0; notes::NAME_MAX + 1],
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn notes_dir(
    out: *mut c_char,
    size: size_t,
    card_root: *const c_char,
) -> bool {
    if card_root.is_null() {
        return false;
    }
    match notes::dir(c_str(card_root)) {
        Some(text) => write_c_string(&text, out, size),
        None => false,
    }
}

#[no_mangle]
pub unsafe extern "C" fn notes_path(
    out: *mut c_char,
    size: size_t,
    dir: *const c_char,
    name: *const c_char,
) -> bool {
    if dir.is_null() || name.is_null() {
        return false;
    }
    match notes::path(c_str(dir), c_str(name)) {
        Some(text) => write_c_string(&text, out, size),
        None => false,
    }
}

#[no_mangle]
pub unsafe extern "C" fn notes_is_note_name(name: *const c_char) -> bool {
    if name.is_null() {
        return false;
    }
    notes::is_note_name(c_str(name))
}

#[no_mangle]
pub unsafe extern "C" fn notes_list(
    dir: *const c_char,
    out: *mut note_entry_t,
    max: c_int,
) -> c_int {
    if dir.is_null() || out.is_null() || max <= 0 {
        return 0;
    }
    let names = notes::list(&path_from_bytes(c_bytes(dir)), max as usize);
    for (i, name) in names.iter().enumerate() {
        let slot = &mut *out.add(i);
        let bytes = name.as_bytes();
        let n = bytes.len().min(notes::NAME_MAX);
        for (j, &b) in bytes[..n].iter().enumerate() {
            slot.name[j] = b as c_char;
        }
        slot.name[n] = 0;
    }
    names.len() as c_int
}

/// The note's text, decoded to UTF-8 in a buffer the caller frees.
#[no_mangle]
pub unsafe extern "C" fn notes_read(path: *const c_char, truncated: *mut bool) -> *mut c_char {
    if !truncated.is_null() {
        *truncated = false;
    }
    if path.is_null() {
        return ptr::null_mut();
    }
    let Some((raw, more)) = notes::read_raw(&path_from_bytes(c_bytes(path))) else {
        return ptr::null_mut();
    };

    let decoded = text::decode(&raw);
    let out = malloc(decoded.len() + 1) as *mut u8;
    if out.is_null() {
        return ptr::null_mut();
    }
    if !decoded.is_empty() {
        ptr::copy_nonoverlapping(decoded.as_ptr(), out, decoded.len());
    }
    *out.add(decoded.len()) = 0;
    if !truncated.is_null() {
        *truncated = more;
    }
    out as *mut c_char
}

#[no_mangle]
pub unsafe extern "C" fn notes_fits_editor(text: *const c_char) -> bool {
    if text.is_null() {
        return false;
    }
    notes::fits_editor(c_str(text))
}

#[no_mangle]
pub unsafe extern "C" fn notes_file_name(
    name: *const c_char,
    out: *mut c_char,
    size: size_t,
) -> bool {
    if name.is_null() {
        return false;
    }
    write_c_string(&notes::file_name(c_str(name)), out, size)
}

#[no_mangle]
pub unsafe extern "C" fn notes_write(
    dir: *const c_char,
    name: *const c_char,
    text: *const c_char,
) -> bool {
    if dir.is_null() || name.is_null() {
        return false;
    }
    notes::write(&path_from_bytes(c_bytes(dir)), c_str(name), c_str(text))
}

#[no_mangle]
pub unsafe extern "C" fn notes_name_ok(name: *const c_char) -> bool {
    if name.is_null() {
        return false;
    }
    notes::name_ok(c_str(name))
}

#[no_mangle]
pub unsafe extern "C" fn notes_display_name(name: *const c_char, out: *mut c_char, size: size_t) {
    if out.is_null() || size == 0 {
        return;
    }
    // The whole name goes into the caller's buffer before the extension is cut
    // off, which is what the C did: with a name too long for `size`, what fits
    // is what decides where the dot is. See [`notes::display_name_into`].
    let buf = slice::from_raw_parts_mut(out as *mut u8, size);
    notes::display_name_into(c_str(name), buf);
}

#[no_mangle]
pub unsafe extern "C" fn notes_default_name(dir: *const c_char, out: *mut c_char, size: size_t) {
    if out.is_null() || size == 0 {
        return;
    }
    let folder = if dir.is_null() {
        None
    } else {
        Some(path_from_bytes(c_bytes(dir)))
    };
    write_c_string(&notes::default_name(folder.as_deref()), out, size);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use core::ffi::CStr;

    /// Every one of these tests calls the `extern "C"` functions the way the
    /// firmware will, over raw pointers, so the ABI (struct layouts, null
    /// handling, NUL termination, malloc ownership) is what is being tested —
    /// not just the core logic underneath, which sonix-core tests separately.
    fn cstring(s: &str) -> std::ffi::CString {
        std::ffi::CString::new(s).unwrap()
    }

    unsafe fn read_back(buf: &[i8]) -> String {
        CStr::from_ptr(buf.as_ptr()).to_str().unwrap().to_owned()
    }

    #[test]
    fn base64_round_trip_through_the_abi() {
        let input = cstring("aGVsbG8=");
        let mut size = 0usize;
        unsafe {
            let out = base64_decode(input.as_ptr(), 8, 64, &mut size);
            assert!(!out.is_null());
            assert_eq!(size, 5);
            assert_eq!(slice::from_raw_parts(out, size), b"hello");
            assert_eq!(*out.add(size), 0, "the caller relies on the terminator");
            free(out as *mut c_void);
        }
    }

    #[test]
    fn base64_refuses_what_the_c_version_refused() {
        let input = cstring("====");
        let mut size = 0usize;
        unsafe {
            assert!(base64_decode(input.as_ptr(), 4, 64, &mut size).is_null());
            assert_eq!(size, 0);
            assert!(base64_decode(ptr::null(), 0, 64, &mut size).is_null());
        }
    }

    #[test]
    fn hashes_match_the_published_vectors() {
        unsafe {
            let mut md5_buf = [0i8; 33];
            md5_hex(b"abc".as_ptr() as *const c_void, 3, md5_buf.as_mut_ptr());
            assert_eq!(read_back(&md5_buf), "900150983cd24fb0d6963f7d28e17f72");

            let mut sha1_buf = [0i8; 41];
            sha1_hex(b"abc".as_ptr() as *const c_void, 3, sha1_buf.as_mut_ptr());
            assert_eq!(
                read_back(&sha1_buf),
                "a9993e364706816aba3e25717850c26c9cd0d89d"
            );

            let mut ctx = std::mem::MaybeUninit::<sha256_ctx>::uninit();
            sha256_init(ctx.as_mut_ptr());
            // Fed in two pieces, the way a firmware download arrives.
            sha256_update(ctx.as_mut_ptr(), b"ab".as_ptr() as *const c_void, 2);
            sha256_update(ctx.as_mut_ptr(), b"c".as_ptr() as *const c_void, 1);
            let mut sha256_buf = [0i8; 65];
            sha256_final_hex(ctx.as_mut_ptr(), sha256_buf.as_mut_ptr());
            assert_eq!(
                read_back(&sha256_buf),
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
            );
        }
    }

    #[test]
    fn sha256_context_layout_matches_the_c_struct() {
        // { uint32_t h[8]; uint64_t bits; unsigned char block[64]; size_t have }
        assert_eq!(core::mem::align_of::<sha256_ctx>(), 8);
        assert_eq!(
            core::mem::size_of::<sha256_ctx>(),
            112,
            "32 + 8 + 64 + size_t, rounded to the u64 alignment"
        );
        assert_eq!(core::mem::offset_of!(sha256_ctx, h), 0);
        assert_eq!(core::mem::offset_of!(sha256_ctx, bits), 32);
        assert_eq!(core::mem::offset_of!(sha256_ctx, block), 40);
    }

    #[test]
    fn json_document_survives_a_round_trip_through_c_memory() {
        let text = cstring(r#"{"tracks":{"items":[{"title":"So What","id":12345}]}}"#);
        unsafe {
            let mut doc = std::mem::MaybeUninit::<json_doc_t>::uninit();
            assert!(json_parse(text.as_ptr(), doc.as_mut_ptr()));
            let doc = doc.assume_init();

            // root object, "tracks", its object, "items", the array, the
            // item object, "title" and its value, "id" and its value.
            assert_eq!(doc.count, 10);
            assert_eq!(json_root(&doc), 0);

            let items = json_path(&doc, 0, cstring("tracks.items").as_ptr());
            assert_eq!(json_len(&doc, items), 1);

            let item = json_at(&doc, items, 0);
            let mut title = [0i8; 32];
            assert!(json_obj_str(
                &doc,
                item,
                cstring("title").as_ptr(),
                title.as_mut_ptr(),
                32
            ));
            assert_eq!(read_back(&title), "So What");
            assert_eq!(
                json_obj_long(&doc, item, cstring("id").as_ptr(), -1) as i64,
                12345
            );

            // The token array really is in C memory: the ABI handed back a
            // plain pointer that C could free itself.
            assert!(!doc.toks.is_null());

            let mut doc = doc;
            json_free(&mut doc);
            assert!(doc.toks.is_null());
            assert!(doc.text.is_null());
            assert_eq!(doc.count, 0);
        }
    }

    #[test]
    fn json_rejects_and_leaves_no_allocation_behind() {
        let text = cstring(r#"{"a":1,}"#);
        unsafe {
            let mut doc = std::mem::MaybeUninit::<json_doc_t>::uninit();
            assert!(!json_parse(text.as_ptr(), doc.as_mut_ptr()));
            let doc = doc.assume_init();
            assert!(doc.toks.is_null());
            assert_eq!(doc.count, 0);
            assert_eq!(json_root(&doc), -1);
        }
    }

    #[test]
    fn json_reads_a_null_document_without_crashing() {
        unsafe {
            assert_eq!(json_root(ptr::null()), -1);
            assert_eq!(json_get(ptr::null(), 0, cstring("a").as_ptr()), -1);
            assert_eq!(json_str(ptr::null(), 0, ptr::null_mut(), 0), false);
            assert_eq!(json_obj_long(ptr::null(), 0, cstring("a").as_ptr(), 7), 7);
        }
    }

    #[test]
    fn config_round_trips_through_a_file() {
        let dir = std::env::temp_dir().join(format!("sonix-cfg-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("device_config.ini");
        let _ = std::fs::remove_file(&path);
        let c_path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();

        unsafe {
            assert!(!config_init(c_path.as_ptr()), "no file yet");
            config_set(
                cstring("ui").as_ptr(),
                cstring("theme").as_ptr(),
                cstring("dark").as_ptr(),
            );
            config_set_int(cstring("eq").as_ptr(), cstring("band0").as_ptr(), -3);
            config_set_bool(
                cstring("clock").as_ptr(),
                cstring("configured").as_ptr(),
                true,
            );
            assert!(config_save());

            unsafe fn get(section: &str, key: &str) -> String {
                // The CStrings are held in locals: the fallback is handed back
                // by pointer when a key is missing, and a temporary would be
                // gone by the time it is read.
                let section = cstring(section);
                let key = cstring(key);
                let fallback = cstring("<none>");
                let p = config_get(section.as_ptr(), key.as_ptr(), fallback.as_ptr());
                CStr::from_ptr(p).to_str().unwrap().to_owned()
            }
            assert_eq!(get("ui", "theme"), "dark");
            assert_eq!(get("eq", "band0"), "-3");
            assert_eq!(get("clock", "configured"), "1");
            assert_eq!(get("ui", "missing"), "<none>");

            assert_eq!(
                config_get_int(cstring("eq").as_ptr(), cstring("band0").as_ptr(), 0),
                -3
            );
            assert!(config_get_bool(
                cstring("clock").as_ptr(),
                cstring("configured").as_ptr(),
                false
            ));

            // A fresh load of what was written parses back identically.
            assert!(config_init(c_path.as_ptr()));
            assert_eq!(get("ui", "theme"), "dark");
            assert_eq!(
                CStr::from_ptr(config_path()).to_str().unwrap(),
                path.to_str().unwrap()
            );
        }

        let _ = std::fs::remove_file(&path);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn config_moves_a_section_between_files() {
        unsafe {
            let main = config_main_store();
            let ebook = config_ebook_store();
            config_store_set(
                main,
                cstring("ebook").as_ptr(),
                cstring("book1").as_ptr(),
                cstring("42").as_ptr(),
            );
            let moved = config_store_move_section(main, ebook, cstring("ebook").as_ptr());
            assert!(moved >= 1);
            let p = config_store_get(
                ebook,
                cstring("ebook").as_ptr(),
                cstring("book1").as_ptr(),
                ptr::null(),
            );
            assert_eq!(CStr::from_ptr(p).to_str().unwrap(), "42");
        }
    }

    #[test]
    fn config_store_handles_are_opaque_but_distinct() {
        assert!(!config_main_store().is_null());
        assert!(!config_ebook_store().is_null());
        assert_ne!(config_main_store(), config_ebook_store());
    }

    #[test]
    fn unknown_store_handle_is_ignored() {
        let bogus = 0x10 as *mut config_store_t;
        unsafe {
            assert!(!config_store_init(bogus, cstring("/tmp/x").as_ptr()));
            assert!(!config_store_save(bogus));
            assert!(config_store_get(
                bogus,
                cstring("a").as_ptr(),
                cstring("b").as_ptr(),
                ptr::null()
            )
            .is_null());
        }
    }

    #[test]
    fn long_matches_the_c_data_model() {
        // Not `size_of::<usize>()`: Windows is LLP64, where a 64-bit build
        // still has a 32-bit long. The differential harness found this the
        // hard way, with a set_int that landed on i32::MAX.
        #[cfg(target_os = "windows")]
        assert_eq!(core::mem::size_of::<c_long>(), 4, "LLP64");

        #[cfg(all(not(target_os = "windows"), target_pointer_width = "64"))]
        assert_eq!(core::mem::size_of::<c_long>(), 8, "LP64");

        #[cfg(target_pointer_width = "32")]
        assert_eq!(
            core::mem::size_of::<c_long>(),
            4,
            "ILP32, which the player is"
        );
    }

    #[test]
    fn json_tok_layout_matches_the_c_struct() {
        assert_eq!(core::mem::size_of::<json_tok_t>(), 20);
        assert_eq!(core::mem::offset_of!(json_tok_t, parent), 16);
    }

    #[test]
    fn tagdate_layout_matches_the_c_struct() {
        assert_eq!(core::mem::size_of::<tagdate_t>(), 12);
        assert_eq!(core::mem::offset_of!(tagdate_t, day), 8);
    }

    #[test]
    fn isqrt_crosses_the_boundary() {
        assert_eq!(utils_isqrt32(0), 0);
        assert_eq!(utils_isqrt32(17), 4);
        assert_eq!(utils_isqrt32(u32::MAX), 65535);
        assert_eq!(utils_isqrt64(u64::MAX), 4_294_967_295);
    }

    #[test]
    fn extension_check_crosses_the_boundary() {
        unsafe {
            let name = cstring("track.FLAC");
            let ext = cstring(".flac");
            assert!(has_extension(name.as_ptr(), ext.as_ptr()));
            let other = cstring(".wav");
            assert!(!has_extension(name.as_ptr(), other.as_ptr()));
            // A null pointer is an empty string here, which no extension matches.
            assert!(!has_extension(ptr::null(), ext.as_ptr()));
        }
    }

    #[test]
    fn seconds_report_a_length_even_without_a_buffer() {
        let want = unsafe { formatDoubleSeconds(3661.0, ptr::null_mut(), 0) };
        assert_eq!(want, 7, "snprintf(NULL, 0, ...) answers the length");

        let mut buf = [0i8; 32];
        let written = unsafe { formatDoubleSeconds(65.0, buf.as_mut_ptr(), buf.len()) };
        assert_eq!(written, 4);
        assert_eq!(unsafe { read_back(&buf[..5]) }, "1:05");
    }

    #[test]
    fn seconds_terminate_inside_the_capacity() {
        // Four bytes: three characters and the terminator, and the wanted
        // length still reported whole, as snprintf does.
        let mut buf = [0x55i8; 4];
        let written = unsafe { formatDoubleSeconds(3661.0, buf.as_mut_ptr(), 4) };
        assert_eq!(written, 7, "\"1:01:01\" is seven characters");
        assert_eq!(unsafe { read_back(&buf) }, "1:0");

        // With room to spare, nothing past the terminator is touched.
        let mut roomy = [0x55i8; 16];
        let written = unsafe { formatDoubleSeconds(3661.0, roomy.as_mut_ptr(), roomy.len()) };
        assert_eq!(written, 7);
        assert_eq!(unsafe { read_back(&roomy) }, "1:01:01");
        assert!(
            roomy[8..].iter().all(|&b| b == 0x55),
            "the call wrote past what it needed"
        );
    }

    #[test]
    fn progress_and_row_duration_cross_the_boundary() {
        let mut buf = [0i8; 32];
        unsafe { formatDoubleProgress(65.0, 300.0, buf.as_mut_ptr(), buf.len()) };
        assert_eq!(unsafe { read_back(&buf) }, "1:05/5:00");

        let mut row = [0i8; 24];
        unsafe { format_row_duration(185, row.as_mut_ptr(), row.len()) };
        assert_eq!(unsafe { read_back(&row) }, "3:05");

        // A track with no length is an empty string, not "0:00".
        let mut empty = [0x33i8; 8];
        unsafe { format_row_duration(0, empty.as_mut_ptr(), empty.len()) };
        assert_eq!(empty[0], 0);
    }

    #[test]
    fn decode_returns_c_owned_utf8() {
        let raw = b"caf\xE9 x";
        let out = unsafe { textfile_decode(raw.as_ptr(), raw.len()) };
        assert!(!out.is_null());
        let text = unsafe { CStr::from_ptr(out) }.to_bytes().to_vec();
        unsafe { free(out as *mut c_void) };
        assert_eq!(text, "café x".as_bytes());
    }

    #[test]
    fn decode_of_nothing_is_an_empty_string_not_null() {
        let out = unsafe { textfile_decode(ptr::null(), 0) };
        assert!(!out.is_null());
        assert_eq!(unsafe { CStr::from_ptr(out) }.to_bytes(), b"");
        unsafe { free(out as *mut c_void) };
    }

    #[test]
    fn a_file_round_trips_through_write_and_read() {
        let dir = std::env::temp_dir();
        let path = dir.join(format!("sonix-ffi-textfile-{}.txt", std::process::id()));
        let path_c = cstring(path.to_str().unwrap());

        // Written without a trailing newline, it comes back with one.
        assert!(unsafe { textfile_write(path_c.as_ptr(), cstring("hello").as_ptr()) });
        let body = std::fs::read(&path).unwrap();
        assert_eq!(body, b"hello\n");

        // No temporary file is left behind.
        let leftover = dir.join(format!("sonix-ffi-textfile-{}.txt.tmp", std::process::id()));
        assert!(!leftover.exists());

        let mut raw: *mut u8 = ptr::null_mut();
        let mut bytes = 99usize;
        let mut truncated = true;
        assert!(unsafe {
            textfile_read(path_c.as_ptr(), 1024, &mut raw, &mut bytes, &mut truncated)
        });
        assert_eq!(bytes, 6);
        assert!(!truncated);
        let read = unsafe { slice::from_raw_parts(raw, bytes).to_vec() };
        unsafe { free(raw as *mut c_void) };
        assert_eq!(read, b"hello\n");

        // A short limit reports the cut.
        let mut raw2: *mut u8 = ptr::null_mut();
        let mut bytes2 = 0usize;
        let mut truncated2 = false;
        assert!(unsafe {
            textfile_read(path_c.as_ptr(), 3, &mut raw2, &mut bytes2, &mut truncated2)
        });
        assert_eq!(bytes2, 3);
        assert!(truncated2, "there was more in the file");
        unsafe { free(raw2 as *mut c_void) };

        // Exactly the whole file is not a truncation.
        let mut raw3: *mut u8 = ptr::null_mut();
        let mut bytes3 = 0usize;
        let mut truncated3 = true;
        assert!(unsafe {
            textfile_read(path_c.as_ptr(), 6, &mut raw3, &mut bytes3, &mut truncated3)
        });
        assert_eq!(bytes3, 6);
        assert!(!truncated3);
        unsafe { free(raw3 as *mut c_void) };

        // And a file that is not there is a failure, not an empty read.
        let mut raw4: *mut u8 = ptr::null_mut();
        let mut bytes4 = 0usize;
        let mut truncated4 = false;
        assert!(!unsafe { textfile_read(ptr::null(), 8, &mut raw4, &mut bytes4, &mut truncated4) });
        assert!(raw4.is_null());

        std::fs::remove_file(&path).unwrap();

        // An empty note still produces a file, and it ends in a newline.
        let empty_path = dir.join(format!(
            "sonix-ffi-textfile-empty-{}.txt",
            std::process::id()
        ));
        let empty_c = cstring(empty_path.to_str().unwrap());
        assert!(unsafe { textfile_write(empty_c.as_ptr(), cstring("").as_ptr()) });
        let empty_body = std::fs::read(&empty_path).unwrap();
        std::fs::remove_file(&empty_path).unwrap();
        assert_eq!(empty_body, b"", "an empty note is not given a newline");
    }

    #[test]
    fn tagdate_crosses_the_boundary() {
        let mut date = tagdate_t::default();
        unsafe {
            assert!(tagdate_parse(cstring("2026-03-12").as_ptr(), &mut date));
            assert_eq!((date.year, date.month, date.day), (2026, 3, 12));

            assert!(tagdate_parse(cstring("2026-13").as_ptr(), &mut date));
            assert_eq!((date.year, date.month, date.day), (2026, 0, 0));

            assert!(!tagdate_parse(cstring("n/a").as_ptr(), &mut date));
            assert_eq!((date.year, date.month, date.day), (0, 0, 0));
        }
    }

    #[test]
    fn the_fuller_date_wins_across_the_boundary() {
        let year = tagdate_t {
            year: 2026,
            month: 0,
            day: 0,
        };
        let day = tagdate_t {
            year: 2026,
            month: 3,
            day: 12,
        };
        unsafe {
            assert!(tagdate_better(&day, &year));
            assert!(!tagdate_better(&year, &day));
            assert!(!tagdate_better(&day, &day));
            assert!(tagdate_better(&year, ptr::null()));
            assert!(!tagdate_better(ptr::null(), ptr::null()));
        }
    }

    #[test]
    fn a_tag_date_becomes_one_moment() {
        // The parts a tag left out settle on the first of the month, so a month
        // and its first day are the same moment.
        let month = tagdate_t {
            year: 2026,
            month: 3,
            day: 0,
        };
        let first = tagdate_t {
            year: 2026,
            month: 3,
            day: 1,
        };
        let earlier = tagdate_t {
            year: 2025,
            month: 12,
            day: 31,
        };

        unsafe {
            assert_eq!(tagdate_to_time(&month), tagdate_to_time(&first));
            assert!(tagdate_to_time(&earlier) < tagdate_to_time(&first));
            assert_eq!(tagdate_to_time(ptr::null()), 0);

            let none = tagdate_t::default();
            assert_eq!(tagdate_to_time(&none), 0, "no year, no moment");
        }
    }
    #[test]
    fn a_note_entry_matches_the_c_struct() {
        assert_eq!(core::mem::size_of::<note_entry_t>(), notes::NAME_MAX + 1);
    }

    #[test]
    fn time_t_matches_the_c_data_model() {
        // 32 bits on the device, 64 on every desktop this is tested on.
        #[cfg(any(target_os = "windows", target_pointer_width = "64"))]
        assert_eq!(core::mem::size_of::<c_time_t>(), 8);

        #[cfg(all(not(target_os = "windows"), target_pointer_width = "32"))]
        assert_eq!(core::mem::size_of::<c_time_t>(), 4);
    }
}
