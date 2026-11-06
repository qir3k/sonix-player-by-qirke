//! The platform-free half of the Sonix Player Rust port.
//!
//! Everything in this crate is pure logic: no LVGL, no ALSA, no sockets, no
//! filesystem. That is deliberate. These are the pieces the C original also
//! wrote by hand with no dependency on the device, so they can be ported to
//! Rust, tested on a desktop against the same vectors, and then either linked
//! into the existing firmware through [`sonix-ffi`](../../sonix-ffi) or called
//! natively as the rest of the player follows.
//!
//! Each module names the C file it replaces in its own header comment. The
//! behaviour is meant to be identical, including the lenient parts: the JSON
//! reader still accepts a handful of things strict JSON does not, because
//! Tidal and Qobuz send them and the C version coped.

pub mod base64;
pub mod config;
pub mod hash;
pub mod json;
pub mod tagdate;
pub mod text;
pub mod utils;

/// The names the C core layer used, so a ported call site reads the same as
/// the one it replaced: `md5_hex(data)` in C becomes `md5_hex(data)` here.
pub use base64::decode as base64_decode;
pub use hash::{md5, md5_hex, sha1, sha1_hex, Sha256};
pub use json::Doc as JsonDoc;
pub use tagdate::Date as TagDate;
pub use text::decode as textfile_decode;
