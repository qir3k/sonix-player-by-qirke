# The Rust port

This directory is where the player becomes Rust. It is a Cargo workspace that
builds two crates:

| Crate | What it is |
|---|---|
| `sonix-core` | Pure logic with no dependencies: base64, MD5, SHA-1, SHA-256, the JSON reader, the settings store, the duration formatters and integer roots, text decoding and tag dates. No LVGL, no ALSA, no sockets, no filesystem. |
| `sonix-ffi` | A C ABI over the above, exporting each function under **the same name and signature the C file it replaces had**, so the firmware can switch over one module at a time. Produces `libsonix_player_core.a`. |

## What is ported today

| Original C file | Rust module | Status |
|---|---|---|
| `src/system/core/base64.c` | `sonix-core/src/base64.rs` | ported, tested |
| `src/system/core/md5.c` | `sonix-core/src/hash/md5.rs` | ported, tested against RFC 1321 |
| `src/system/core/sha1.c` | `sonix-core/src/hash/sha1.rs` | ported, tested against FIPS 180-1 |
| `src/system/core/sha256.c` | `sonix-core/src/hash/sha256.rs` | ported, incremental, tested against FIPS 180-4 |
| `src/system/core/json.c` | `sonix-core/src/json.rs` | ported, tested |
| `src/system/core/config.c` | `sonix-core/src/config.rs` | ported (in-memory + INI parse/render), tested |
| `src/system/core/utils.c` | `sonix-core/src/utils.rs` | pure half ported (integer roots, duration formatters, extension check); the thread/heap/breadcrumb half stays C — see [PORTING.md](PORTING.md) |
| `src/system/text/textfile.c` | `sonix-core/src/text.rs` | ported (UTF-8 / UTF-16 / Windows-1252 decode, atomic write), tested |
| `src/system/library/tagdate.c` | `sonix-core/src/tagdate.rs` | ported (parse, precision), tested; `to_time` goes through libc `mktime` |

Everything else — the GUI, the decoders, streaming, Bluetooth, the audio path —
is still C. [PORTING.md](PORTING.md) is the map of what is left and in what
order, with the real line counts.

## Build and test on a desktop

```bash
cd sonix-player/rust
make test          # cargo test --workspace, 108 tests
make differential  # the C originals beside the Rust port, same inputs
make staticlib     # libsonix_player_core.a for the firmware to link
```

No dependencies, so this needs nothing but a Rust toolchain; `make
differential` also needs a C compiler (`CC=` to point at one). The release
profile is `opt-level = "s"`, LTO, `panic = "abort"` and stripped symbols,
because the device has 55.6 MB of RAM and a read-only rootfs updated as one
image — size beats the last few percent of speed everywhere.

### The differential test

`tests/differential` is the one that matters. It compiles the original C files
with their exported symbols renamed (`-Dmd5=c_md5`, …), links them beside the
Rust archive, and calls both sides with the same inputs: 400 base64 round trips
including the URL-safe alphabet and every cap, 300 random buffers through each
hash (SHA-256 fed in uneven chunks), 250 generated JSON documents plus a
hand-picked corpus walked token by token, a settings file driven through both
implementations and compared byte for byte, 40,000 random integers (and 40,000
64-bit ones) plus every power of two on either side of itself through the square
roots, the formatters
compared on the whole buffer at every capacity from 0 to 63 (so a terminator
written one byte too far fails), 3,000 generated text files of four kinds
(raw bytes, ASCII, UTF-8, UTF-16 with and without a BOM) decoded on both sides,
reads at ten different limits including exactly past the end, writes compared
byte for byte, and the date parser over a corpus plus every pair through the
precision rule.

It currently runs **1,084,531 checks with 0 failures**, and it is how the two
bugs listed at the end of [PORTING.md](PORTING.md) were found — neither was
visible to the unit tests.

`utils.c` is compiled for that test as the *prefix of the real file* (everything
above its platform section, includes unchanged), so there is no second copy of
those functions to drift. `textfile.c` needs one define on Windows
(`-Dfsync=_commit`, the same call under the CRT's name) and is otherwise
unmodified.

One comparison is skipped on Windows on purpose: the C config save uses
`rename()`, which replaces the destination only on POSIX, so the C original
cannot overwrite an existing settings file there. That is a portability bug in
the C code, not behaviour worth reproducing.

## Linking it into the firmware

The point of `sonix-ffi` is that a C call site does not change when a
definition moves from C to Rust. Switching a module over:

1. Remove its `.c` from the object list in `sonix-player/Makefile`
   (`SYSTEM_SRCS` / `GUI_SRCS`).
2. Add `libsonix_player_core.a` to the link (`TARGET_LIBS`).

```make
TARGET_LIBS += $(CURDIR)/rust/libsonix_player_core.a
```

Leaving both in the build is a duplicate-symbol error at link time, which is
loud and immediate rather than a subtle half-port.

Nothing in this branch touches `Makefile` yet: the archive and the C objects
cannot both be linked, and the firmware build has not been run against it. Do
the swap for one module first (`md5.c` is the smallest), on a device you can
recover, and watch `make check-abi` for new dynamic symbols.

## What is deliberately *not* different

The port keeps the C behaviour, including the parts that look odd, because the
callers depend on them:

* `base64_decode` still skips whitespace, padding and garbage, still accepts
  both the standard and the URL-safe alphabet, and still refuses — before
  allocating — anything whose worst case exceeds its cap.
* `json_*` still reads a lenient dialect rather than strict JSON, still
  saturates `json_long` at the target's 32-bit `long` (use `json_llong` for ids
  the service does not promise to keep small), and `Tok` is still 20 bytes of
  `#[repr(C)]` so the C side can read the array directly.
* `config_*` still truncates with `snprintf` semantics (31/31/127 characters),
  still keeps a fixed-size table, and `config_get` still hands back a pointer
  that stays valid until that key is set again.
* `json_free` still frees C-heap memory, so a C caller's `free` and Rust's
  `malloc` agree.
* `textfile_decode` still drops every code point below 0x20 (and 0x7F, and a
  stray BOM), still turns tabs into four spaces and `\r\n` into one `\n`, and
  still treats a high byte at the very end of the buffer as a character the read
  limit cut in half — which is why a Windows-1252 file whose last byte is
  accented is only recognised as 1252 if something follows it.
* `formatDoubleSeconds` still clamps a negative or NaN time to zero, and still
  returns what the text *wanted* rather than what fitted, as `snprintf` does.
  Past 2^32 seconds C's cast is undefined and Rust saturates; no track reaches
  that, and the harness does not compare it.
* `tagdate_parse` still keeps whatever part of a date is real (`2026-13` is a
  year, `2026-02-99` is a year and a month) and `tagdate_better` still prefers
  the fuller one, ties going to whichever the caller already had.

## Cross-compiling for the device

`mipsel-unknown-linux-gnu` is a Tier 3 Rust target: `rustup target add` will not
find it and there is no prebuilt `std`. Building for the Variations V1 needs a
nightly toolchain and `-Z build-std`:

```bash
rustup toolchain install nightly
rustup component add rust-src --toolchain nightly
make target
```

`.cargo/config.toml` points the link at the same `mipsel-rockbox-linux-gnu`
toolchain the C firmware builds with, and sets `target-cpu=mips32r2` to match
the C side's `-march=mips32r2`. The C build also passes `-mno-madd4` and
`-mplt`, which have no stable `-C target-feature` spelling; check the resulting
archive against the C objects before trusting it (`make check-abi` in
`sonix-player/` reads the dynamic symbols out of the target binary for exactly
this kind of drift).

This has not been run on hardware from this branch. Treat the cross build as
unverified until it boots.
