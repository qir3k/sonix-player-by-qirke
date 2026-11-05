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
//! | C file (`src/system/core/`) | Symbols |
//! |---|---|
//! | `base64.c`   | `base64_decode` |
//! | `md5.c`      | `md5`, `md5_hex` |
//! | `sha1.c`     | `sha1`, `sha1_hex` |
//! | `sha256.c`   | `sha256_init`, `sha256_update`, `sha256_final_hex` |
//! | `config.c`   | `config_*`, `config_store_*` |
//! | `json.c`     | `json_*` |
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
use std::sync::{Mutex, OnceLock};

use sonix_core::{base64, config, hash, json};

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
    fn malloc(size: size_t) -> *mut u8;
    fn free(ptr: *mut u8);
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

    let out = malloc(decoded.len() + 1);
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
        free(d.toks as *mut u8);
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

#[no_mangle]
pub unsafe extern "C" fn json_long(doc: *const json_doc_t, tok: c_int, fallback: c_long) -> c_long {
    match view(doc) {
        Some(v) => v.long(tok, fallback as i32) as c_long,
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
        Some(v) => v.obj_long(obj, c_str(key), fallback as i32) as c_long,
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
            free(out);
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
}
