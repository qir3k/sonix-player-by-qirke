//! The three hashes the player actually needs, ported from
//! `src/system/core/{md5,sha1,sha256}.c`.
//!
//! None of them is a security choice made by this project. Each exists because
//! a service put it on its own front door:
//!
//! * MD5 — Qobuz signs the request for a track's URL with it.
//! * SHA-1 — Podcast Index signs every request with it.
//! * SHA-256 — a firmware image is checked against it as it arrives.
//!
//! The C versions are written here rather than taken from the libcrypto the
//! device already carries, because the library is opened at run time and may
//! be absent, and playing a track should not depend on it loading. The Rust
//! port keeps that property: no dependencies, testable against the published
//! vectors.

mod md5;
mod sha1;
mod sha256;

pub use md5::{digest as md5, hex as md5_hex, DIGEST_LEN as MD5_DIGEST_LEN};
pub use sha1::{digest as sha1, hex as sha1_hex, DIGEST_LEN as SHA1_DIGEST_LEN};
pub use sha256::{digest_hex as sha256_hex, Ctx as Sha256Ctx, DIGEST_LEN as SHA256_DIGEST_LEN};

/// SHA-256, in the shape the firmware uses it: a firmware image is hashed a
/// piece at a time as it arrives and is never in memory whole.
pub type Sha256 = Sha256Ctx;
