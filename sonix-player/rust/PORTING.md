# Porting plan: the C player to Rust

## The size of the thing

Measured on `dev` at the time this branch was cut:

| What | Lines |
|---|---|
| `src/**/*.c` | 508,336 |
| — of which generated LVGL image data (`gui/shell/icons.c`, `gui/shell/main_menu_icons.c`) | 262,780 |
| — of which vendored decoders (`stb_vorbis.c`, `miniz`, …) | ~5,800 |
| `src/**/*.h` | 345,130 |
| — of which the embedded web UI (`net/webpage.h`) | 104,078 |
| `src/gb/**` (Gearboy, vendored C++) | 22,461 |
| `tests/**/*.c` | 15,080 |
| `launcher/sonix_launch.c` | 147 |

So roughly **245,000 lines of hand-written player C**, ~200,000 lines of
hand-written headers, ~263,000 lines of generated bitmap data, ~22,000 lines of
vendored C++ emulator, and a 104,000-line pile of HTML inside a header.

Nobody rewrites that in a session, and a claim otherwise would be a lie. What is
achievable — and what this branch starts — is the thing that actually gets to
"my player running on Rust": **port it module by module, with the firmware
shipping the whole time.**

## What "running on Rust" can mean

Three end states, in increasing order of cost:

1. **Rust core, C UI** (this branch's approach). Every module that is logic
   rather than pixels moves to Rust and the C firmware calls it through
   `sonix-ffi`. The player stays shippable after every single commit, and each
   move is testable in isolation. The GUI stays C because it is LVGL callbacks
   all the way down.
2. **Rust everywhere except LVGL itself.** The `lv_*` calls get a `lvgl-sys`
   binding and the ~114,000 lines of GUI C become Rust, with LVGL — a large C
   library that is not ours and not going to be rewritten — staying C underneath.
   This is the honest ceiling for a one-person port.
3. **Rust UI with no LVGL.** Writing a replacement renderer for a 240×320
   device. This is a separate project, not a port, and it would throw away
   every widget behaviour the current UI depends on.

Recommendation: (1) until it is done, then (2) GUI screen by screen. Not (3).

## Constraints the port has to live inside

| Constraint | Consequence |
|---|---|
| Ingenic X1600, MIPS32r2, o32 ABI | `mipsel-unknown-linux-gnu` is **Tier 3**: no `rustup target add`, no prebuilt `std`. Needs nightly + `-Z build-std`. |
| glibc 2.22 | the linked `std` must not call symbols newer than that; verify against the toolchain's sysroot. |
| 55.6 MB RAM total, ~10 MB free at runtime | the settings store's fixed table, the token-array JSON reader and the no-allocation-on-the-hot-path style all exist for this. Keep them. |
| Read-only rootfs, one UBIFS writable partition | `std::fs` is fine, but atomic-write-and-rename must be kept for anything that survives a power cut. |
| `-march=mips32r2 -mno-madd4 -mplt` | the Rust build must not emit MIPS32r6 instructions or fused multiply-adds. `target-cpu=mips32r2` covers the first; the other two have no stable `-C target-feature` spelling and need checking against the C objects. |
| Device image size | `std` adds code. Measure the archive before deciding `std` versus `no_std` + `alloc` for the hot modules. The current crates use `std` for file and lock access; only `sonix-core`'s logic would need changing. |

## Status

| Module | Lines | State |
|---|---|---|
| `system/core/base64.c` | 40 | **ported** |
| `system/core/md5.c` | 100 | **ported** |
| `system/core/sha1.c` | 120 | **ported** |
| `system/core/sha256.c` | 125 | **ported** |
| `system/core/json.c` | 540 | **ported** |
| `system/core/config.c` | 300 | **ported** |
| `system/core/utils.c` | 480 | **pure half ported** — roots, formatters, `has_extension`; the platform half (threads, `mmap`, `mallinfo`, breadcrumbs) stays C |
| `system/text/textfile.c` | 210 | **ported** |
| `system/library/tagdate.c` | 140 | **ported** |
| `system/library/id3chap.c` | 207 | **ported** |
| `system/addons/addons.c` | 178 | **ported** |
| everything else | ~243,000 | C |

### The one module that cannot be swapped by deleting its file

`utils.c` is a grab bag: the top half is arithmetic and string formatting, the
bottom half is `/proc`, `sched_setscheduler`, `sigaltstack` and `mallinfo`. The
top half is ported and exported from `sonix-ffi` under its own names, but the
file has to stay in the build until the bottom half moves — and that half should
move to a `utils_platform.c` companion rather than to Rust, because it is the
kind of code that wants to keep working exactly as it always has until the
device is on the bench. Moving those functions out and dropping the pure ones
is a mechanical edit, worth doing as its own commit with a flashed player to
hand.

## Inventory, in the order it should be attacked

Line counts are `.c` only.

### Tier 1 — no device dependencies, high leverage

| Area | Lines | Why here |
|---|---|---|
| `system/core/` (`logging`, `lang`, `region`, `respath`, `glibc_compat`) | ~1,400 | everything includes it; pure string and formatting work (`utils` is done) |
| `system/core/db` | 46 | thin SQLite wrapper |
| `system/notes/` (`notesstore.c`) | 303 | small and self-contained, but needs POSIX `mkdir`/`localtime_r` shims before it can be verified off the device |
| `system/input/` | 1,143 | key mapping and gesture state, pure logic |
| `system/playback/playlist.c` + `sleeptimer.c` | ~1,300 | queue, shuffle, repeat — completely testable |
| `system/library/cue.c`, `vorbischap.c` | ~1,070 | parsers with fixed grammars (`tagdate` and `id3chap` are done) |
| `system/audio/eq.c` (the filter maths, not the ALSA glue) | ~1,000 | biquads; needs golden data from the C build |

### Tier 2 — network and protocol, testable against recorded responses

| Area | Lines | Notes |
|---|---|---|
| `system/net/http.c`, `hls.c`, `tls.c` | ~2,600 | TLS is dlopen'd libcrypto; keep that boundary, port the request/response logic |
| `system/streaming/` (Tidal, Qobuz, YT Music, radio, podcast) | 20,424 | mostly JSON walking — the biggest single win from the JSON port |
| `system/lastfm`, `system/listenbrainz` | 2,581 | small HTTP + signing clients |
| `system/remote/sonixlink.c` | 3,191 | protocol framing |
| `system/library/lrclib.c`, `lyrics.c`, `tagwrite.c` | ~2,100 | text and tag surgery, and exactly the sort of thing Rust makes safe |

### Tier 3 — device glue, port last

| Area | Lines | Notes |
|---|---|---|
| `system/library/library.c`, `metadata.c`, `albumart.c`, `genius*` | ~21,500 | the scanning/indexing core; heavy but self-contained |
| `system/decode/` | 16,511 | mostly vendored C decoders; only `decode.c`, `mp4.c` and friends are ours. The decoders stay C (or get Rust rewrites as separate work) |
| `system/audio/` | 12,315 | ALSA and the CS43131 path; `unsafe` and hardware |
| `system/device/` | 12,524 | `/sys` and `/proc` reads, power, USB, charging |
| `system/bluetooth/` | 12,654 | BlueZ/D-Bus; the hardest single subsystem and the least worth porting early |
| `system/ebook/` | 2,812 | EPUB rendering |
| `gui/**` (hand-written) | ~114,000 | LVGL callbacks; needs bindings first (end state 2) |

## Why this order

* Tier 1 modules have no hardware in them, so they are the ones a desktop test
  can fully cover, and they are what every other module calls.
* The JSON port unlocks Tier 2: 20,000 lines of streaming code is dominated by
  "walk this response and pull these fields", and that is exactly what
  `sonix-core/src/json.rs` now does identically to the C version.
* Bluetooth, audio and the GUI are where the port would get *slower and worse*
  if it went first: they are `unsafe`-heavy, hardware-bound, and impossible to
  verify without a device on the bench.

## Verification, and what it costs

Each module gets three levels:

1. **Unit tests** in the Rust module, for the behaviour the header documents.
   Done for everything ported so far (122 tests: 96 in `sonix-core`, 26 through
   the C ABI in `sonix-ffi`).
2. **A differential harness** (`tests/differential`, run with `make
   differential`): the original `.c` files are compiled with their symbols
   renamed (`-Djson_parse=c_json_parse …`), linked beside the Rust ones, and
   both are fed identical inputs — 400 random base64 round trips, 300 random
   buffers through each hash in uneven chunks, 250 generated JSON documents
   plus a hand-picked corpus walked token by token, and a settings file driven
   through both implementations. Any disagreement fails the run. **Done, and
   passing: 1,088,004 checks, 0 failures.**
3. **Golden vectors** from the C implementation where the arithmetic is not
   self-evident — the equaliser coefficients, ReplayGain, the smart-mix
   scoring. The C build can be made to print them. Needed once the audio and
   library modules move.

### What the harness found

It is worth recording, because none of them was visible to the unit tests:

* `json_str_exact` left the partial bytes it had written in the caller's buffer
  when it rejected a truncation, where C's `invalid:` label clears it. A caller
  that ignores the false return — and some do — would have shown half a value.
* `c_long` was defined by pointer width. Windows is LLP64: a 64-bit build still
  has a 32-bit `long`, so `config_set_int` read 64 bits of register for a
  32-bit argument and landed on `i32::MAX`. The target is ILP32 and never hit
  it, which is exactly why desktop tests matter.
* `struct tm` was declared as its nine `int` fields. glibc's is 56 bytes, not
  36, because it appends `tm_gmtoff` and `tm_zone` — and `mktime` writes those
  two back. `tagdate_to_time` therefore wrote twenty bytes past its own struct
  into the caller's frame: a segfault, and one that happened *only on Linux*,
  because MinGW's `struct tm` really is the nine fields. Running the harness on
  Windows alone had said it was fine.
* `json_long` clamped to 32 bits unconditionally. That is right for the device
  (`long` is 32 there) and for Windows, and wrong on LP64, where C's `strtol`
  returns the whole 64 and Rust was handing back `i32::MAX`. It now clamps to
  whatever width `c_long` has on the target being built.

The last two were both found by running the differential harness on Linux as
well as on Windows — the first time it had been tried anywhere but the host it
was written on. Neither is reachable on the device by the Windows build alone,
which is the argument for running it in both places from now on.

One difference is expected and stays: the C settings save uses `rename()`, which
replaces the destination on POSIX but not on Windows, so on a Windows host the
C original cannot overwrite an existing settings file while the Rust one can.
That is a portability bug in the C code, not a behaviour to copy, so the harness
skips the file-byte comparison there and runs it on a POSIX host.

Device-facing modules are the exception: they need a flashed player and a
microSD card, and no amount of desktop testing replaces that.

## Deliberately staying C

* **LVGL 9.6** — 200,000+ lines of third-party widget library. Porting it is
  not this project's job; binding it is.
* **Gearboy** (`src/gb`, 22,461 lines of C++) — a vendored Game Boy emulator,
  GPL-compatible and working. Rewriting it buys nothing.
* **dr_libs / stb / miniz / libsndfile / libopusfile / libwavpack / FreeType** —
  vendored decoders. Replace only with a Rust crate of proven quality, one at a
  time, and only where the decoder is not on the audio-critical path.
* **`sonix_launch.c`** — 147 lines, `-nostdlib`, static. A Rust panic handler
  would be larger than the whole program.
