# Sonix Player for TempoTec Variations V1 (by qirke)

[![Build and Release](https://github.com/qir3k/sonix-player-by-qirke/actions/workflows/build-tempotec-v1.yml/badge.svg)](https://github.com/qir3k/sonix-player-by-qirke/actions)
[![Latest Release](https://img.shields.io/github/v/release/qir3k/sonix-player-by-qirke?include_prereleases&label=release)](https://github.com/qir3k/sonix-player-by-qirke/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

An advanced, modern, audiophile custom firmware for the **TempoTec Variations V1** digital audio player.

Sonix Player for the TempoTec Variations V1 follows a clear lineage: **[Jepl4r's Sonix Player](https://github.com/Jepl4r/sonix-player)**, then **[MrFox's V1 fork](https://github.com/mr-f0xx/sonix-player-tempotec-v1)**, which originally ported the player to the TempoTec Variations V1, and now this heavily expanded, optimized, and maintained fork by **qirke**.

---

## 🌟 Why choose this fork?

If you own a **TempoTec Variations V1**, this firmware turns your compact DAP into a modern, fast, and feature-rich audiophile machine, freeing it from the slow and outdated stock vendor HiByOS.

Upstream Sonix Player was designed as a minimalist offline player. This fork heavily expands it into a complete, modern audio workstation while keeping memory usage lean enough to run flawlessly within the hardware's 55.6 MB RAM limit:

### Key highlights & unique enhancements in this fork:
* **🧩 Modular Plugins** — Enable optional streaming services, tools, games, audio controls, Bluetooth Speex resampling, and player layouts in `Settings → Plugins`. Poster is the default layout; disabled features can be enabled when wanted, with existing saved settings retained.
* **🧹 System Process & Optimize pages** — `Settings → System` separates the live process/RAM view from **Optimize**. Choose whether Optimize clears memory caches, artwork data, abandoned partial downloads, or other selected caches; it reports the cleanup result.
* **🎵 YouTube Music without a Phone or PC** — Search, browse charts, Explore, albums, and playlists right on the player. Enjoy smooth on-the-fly streaming, offline downloads to microSD, and session import for your own personal library with Invidious/Piped fallback.
* **🌊 Heavily Enhanced Tidal** — 240×320 tailored UI, persistent search history, playlist track reordering/removal, queued offline downloads with a 1 GiB free-space guard, and **"Keep on card"** (copies the currently streaming Tidal track with metadata and artwork straight into your local `Music/Tidal` collection).
* **🎙️ Deep Podcast Index Integration** — Browse and search the vast Podcast Index directory, follow up to 100 shows, filter trending charts by region/language, track heard/unheard episodes, sort episodes (newest $\leftrightarrow$ oldest), and download episodes directly to your card for offline listening.
* **📻 Advanced Internet Radio** — Complete radio-browser.info integration: filter by quality (keep only $\ge$320 kbps or lossless FLAC streams), duplicate elimination, station logo artwork caching, and support for your own custom station lists via `radio.txt`.
* **🏷️ On-Device Tag Manager & Cover Art Finder** — Edit ID3v2 (MP3) and Vorbis comments (FLAC) directly on the device glass without needing a PC. Includes online cover art searching via Apple Music's catalog and embeds sleeves directly into the audio file.
* **📜 Synced Lyrics Fetcher (LRCLIB)** — Swipe left on artwork to view synced lyrics. If a song lacks lyrics, the integrated LRCLIB client searches online over Wi-Fi, lets you preview synced or plain text, and saves it atomically as `<track>.lrc` beside your music.
* **🎨 Cover Art Fetcher** — Automatically look up high-resolution album sleeves over Wi-Fi and write them into the track tags or local album folders.
* **📊 Dual Scrobbler Suite (Last.fm & ListenBrainz)** — Native background scrobbling for both services with independent toggles, token-based setup, and offline queue caching when disconnected from Wi-Fi.
* **🎛️ Audio controls and DSP** — 10-band Graphic EQ, 10-band Parametric EQ (with live frequency curve rendering and AutoEq / Equalizer APO import), MSEB, Soundfield stereo width, headphone Crossfeed, Channel Balance, ReplayGain, and per-output Volume Limit. These controls are available across the player's supported audio paths; optional audio features can be switched off in Plugins.
* **👆 Modernized Control Center & Gestures** — Redesigned Control Center with fluid horizontal swipe gestures (swipe left/right) to navigate multiple tile pages seamlessly—no awkward sliders or handles.
* **🖼️ Refined Now Playing & Visuals** — Brand watermarks on Now Playing (YouTube Music, Qobuz, Tidal, Radio, Podcasts), perfectly vertically aligned artist and bitrate badges across radio and podcasts, and clean layout geometry designed specifically for the 2.0" 240×320 screen.
* **🎮 Touchscreen Games** — Brick Breaker, Pebble Deluxe, Minesweeper, Sudoku, Guess The Song, Stack It, Flappy Bird, 2048, and the Gearboy Game Boy emulator. Games are optional Plugins and are off by default.
* **🧹 Clean SD Card & Cache Hygiene** — Modernized unified cache structure. No abandoned, empty `*-art` folders polluting the root or `.local` on your microSD card.
* **🔄 Over-The-Air (OTA) Updates** — Check for and download new releases directly over Wi-Fi from GitHub Releases with one click in `Settings → System → Firmware update`.

---

## Contents

- [Target device](#target-device)
- [Features overview](#features-overview)
- [Supported audio formats](#supported-audio-formats)
- [Quick Start: Installation](#quick-start-installation)
- [Streaming configuration](#streaming-configuration)
- [Plugins and Defaults](#plugins-and-defaults)
- [Build from source](#build-from-source)
- [Branching model & CI](#branching-model--ci)
- [Credits and Acknowledgements](#credits-and-acknowledgements)

---

## Target device

The **TempoTec Variations V1**:

| Component | Specification |
|---|---|
| **SoC** | Ingenic X1600 (MIPS32r2 core, o32 ABI, glibc 2.22 compatibility) |
| **Display** | 2.0-inch QVGA TFT panel, **240×320 pixels** portrait |
| **DAC** | Dual Cirrus Logic CS43131 |
| **Audio Outputs** | 3.5 mm single-ended + 4.4 mm balanced headphone jacks |
| **Storage** | One MicroSD card slot (exFAT / FAT32) |
| **Controls** | Physical flank buttons (Power, Vol+, Vol-, Play/Pause, Next/Prev) + capacitive touchscreen |
| **Wireless** | 2.4 GHz Wi-Fi (802.11 b/g/n) and bi-directional Bluetooth |
| **USB** | USB Type-C (bidirectional USB DAC & USB audio host out) |
| **Firmware Package** | `v1.upt` with accompanying `v1_md5.txt` |

The player UI is built with LVGL 9.6 and customized for the V1's 240×320 geometry. The firmware is V1-specific; optional player layouts are managed from Plugins rather than removed from the source.

---

## ⚡ High-Performance Architecture for Ingenic X1600

This fork features an extensively optimized low-level architecture designed specifically around the strict hardware constraints of the **Ingenic X1600 (MIPS32r2)** processor and its **55 MB usable RAM budget**:

### 1. Kernel, ZRAM & Boot Optimization
* **Linux Kernel 4.4.94+ with High-Ratio ZRAM:** Configured with ultra-fast **LZ4** compression algorithm (`echo lz4 > /sys/block/zram0/comp_algorithm`), 20 MB zram disk size, and low swappiness (`vm.swappiness = 20`) to minimize page-fault latency under memory pressure.
* **On-Demand Daemons in `/etc/init.d/rcS`:** All automatic boot-time background services (`wpa_supplicant`, `udhcpc`, `bluetoothd`, `avahi-daemon`, `syslogd`) have been disabled from autostarting. Networking and Bluetooth stacks spawn strictly on-demand.
* **CPU Priority & Governor:** Boot governor locked to `performance` with the player binary executed with elevated scheduling priority (`nice -n -10 /usr/bin/sonix_player &`).
* **Rootfs Staging Audit & Squashfs XZ:** Build system runs `mipsel-linux-gnu-strip --strip-all` on all binaries and shared libraries (.so). `v1_packer.sh` compresses rootfs using `mksquashfs -comp xz -b 256K -noappend`, shrinking firmware package size from 50 MB down to **~20 MB**.

### 2. LVGL 9.6 Rendering & glibc Heap Tuning
* **L1 Cache-Aligned Dual Partial Buffers:** Zero full-framebuffer heap allocations. LVGL 9.6 utilizes dual partial line buffers (240 × 32 × 2 bytes RGB565) aligned to 32 bytes, fitting perfectly within the MIPS L1 data cache.
* **glibc Heap Tuning (`mallopt`):** Configured thresholds (`mallopt(M_MMAP_THRESHOLD, 64 * 1024)` and `mallopt(M_TRIM_THRESHOLD, 128 * 1024)`) combined with targeted, event-driven `malloc_trim(0)` calls when closing views or switching tracks. Typical active RAM usage drops to **15–18 MB**.
* **Decoded Artwork Cache Cap:** Max 2 simultaneous decoded images in RAM in 16-bit RGB565 format (eliminating 32-bit RGBA bloat and out-of-memory spikes).
* **UI Template Recycling (`ui_generic_list_view`):** Lists use a fixed virtual pool of only 8 recycled row widgets for the 240×320 viewport, binding data dynamically via callbacks with zero widget allocations during scrolling.
* **Lightweight 250ms Loading Indicator:** Replaced heavy 60 FPS vector spinner with a discrete 250 ms 3-dot text animation, completely decoupling UI rendering from audio decoding loops.

### 3. Touch Input & Boot Stability
* **Input Event Queue Draining:** `flush_input_events(int fd)` drains accumulated stale boot events from `/dev/input/event*` before registration.
* **Touch Settling Grace Period:** Touch indev starts disabled (`lv_indev_enable(false)`) with a one-shot 1200 ms settling timer before enabling, completely eliminating post-boot ghost clicks.

### 4. Real-Time Audio Pipeline & State Machine
* **Real-Time Priority Scheduling:** Dedicated ALSA/CS43131 playback thread runs at `SCHED_FIFO` priority 80. Playback begins only after 300 ms pre-roll buffering.
* **Atomic Audio State Machine:** Sequential pipeline (`[STOPPING] -> [CLEANUP] -> [BUFFERING] -> [PLAYING]`) eliminates clicks and thread collisions when switching sources (YouTube Music, Tidal, Radio, Local Files). Digital DAC mute (`alsa_track_mute`) drops audio cleanly from silence.
* **Decoupled Asynchronous SD Card I/O:** Zero synchronous disk I/O in the UI thread. The `metaloader` pthread worker extracts tags and album sleeves in the background with immediate UI placeholder rendering.
* **Bluetooth Panel Hold & Fast Screen Wake:** Silent screen waking during Bluetooth streaming by holding panel power and toggling backlight, preventing PMIC rail voltage drop and Bluetooth audio stutters. Instant wake on power key-down.
* **Optimized Enchanted SonixLink:** Worker threads set to `nice +10`, static 4 KB packet buffers, 500 ms throttled broadcasts, and on-demand resource cleanup.

---

## Features overview

- **Interface languages** — 12 language packs selectable on the fly (Čeština, English, Deutsch, Español, Français, Italiano, Japanese, Nederlands, Polski, Português BR, Russian, Ukrainian), with dynamic script fallbacks for East Asian, Cyrillic, and Arabic text.
- **Local playback & library** — Tag-based library browsing, folder navigation, A-Z fast scroll strip, instant search, smart playlists, play counts, audiobook resume, and lyrics display.
- **Streaming & network audio** — YouTube Music, Tidal, Qobuz, Subsonic / Navidrome / Gonic, Internet Radio directory, Podcast Index with episode downloads, Bluetooth receiver & transmitter, AirPlay, and DLNA.
- **Bluetooth audio** — High-resolution LDAC with quality/bitrate diagnostics, AAC, aptX, aptX-HD, and SBC. Native protocol support for Apple AirPods / Beats battery & ANC control, as well as EarFun earbuds over GAIA.
- **Modular player layouts** — Standard, Waveform, Alternative, Immersive, Poster, and Album Art Max layouts; Cover Flow is a separate optional album carousel. Enable layouts in `Settings → Plugins`; Music → Display offers the enabled layouts. Poster is enabled by default, and Album Art Max is V1-only.
- **Optional audio and Bluetooth controls** — Soundfield, Crossfeed, Channel Balance, Volume Limit, and Speex Bluetooth resampling can each be enabled or disabled under Plugins. They are enabled by default to preserve the existing behavior; disabling Volume Limit also removes its volume safety ceiling.
- **Smart Mix & Genius playlists** — Fully on-device music recommendations based on genre, artist, and year clustering. Generates standalone mixes directly on the player without external network requests.
- **Tools & Extras** — Metronome, scientific Calculator, quick Notes editor, on-device ID3/FLAC Tag Manager & Cover Finder, and EPUB eBook reader.
- **Power & Battery Guard** — Graceful auto-shutdown at 1%, red warning veil at 5%, and a dedicated battery charging status screen when connected to a charger while powered off.

See [FEATURES.md](FEATURES.md) for an in-depth breakdown of every subsystem.

---

## Supported audio formats

Formats are recognised by their actual header contents, not by file extension. Everything is decoded directly on the device:

### Files on the microSD card

| Codec | Extensions | Notes |
|---|---|---|
| **FLAC** | `.flac` | 16- and 24-bit, all standard rates including hi-res. Leading ID3 tags tolerated. |
| **ALAC** | `.m4a`, `.mp4`, `.alac` | Native 16-bit and 24-bit decoding; bit-identical lossless output. |
| **DSD** | `.dsf`, `.dff` | DSD64, DSD128, and DSD256 via bit-perfect DoP (or PCM conversion over Bluetooth). |
| **MP3** | `.mp3` | dr_mp3; CBR and VBR with ID3v1 and ID3v2 tag support. |
| **AAC-LC** | `.m4a`, `.m4b`, `.mp4`, `.aac` | iTunes and YouTube download containers as well as raw ADTS streams. |
| **Ogg Vorbis** | `.ogg`, `.oga` | Chained and multi-page comment headers read cleanly. |
| **Opus** | `.opus`, `.ogg` | Ogg Opus decoded and resampled natively for the DAC. |
| **WavPack** | `.wv` | Lossless and hybrid modes with metadata support. |
| **Monkey's Audio** | `.ape` | High and extra-high compression modes with APEv2 tags. |
| **WAV / AIFF / CAF** | `.wav`, `.aif`, `.aiff`, `.caf` | Decoded via `libsndfile` with full ID3 and RIFF chunk parsing. |

### Streaming audio formats

| Service / Source | Stream format | Quality |
|---|---|---|
| **Tidal** | AAC in MP4 at Low/High; FLAC for HiFi/Master | Up to 24-bit FLAC |
| **Qobuz** | MP3 320, FLAC 16/44.1, FLAC 24/96, or 24/192 | Configurable in Qobuz settings |
| **YouTube Music** | AAC in MP4 (itag 140 @ 128 kbps) | ~128 kbps |
| **Internet Radio** | MP3, AAC/ADTS, Ogg Vorbis, Opus, FLAC | Station broadcast quality |
| **Podcasts** | MP3 and AAC | Episode stream quality |
| **Subsonic / Gonic** | Transcoded or original lossless format | Server defined |
| **Bluetooth Audio** | LDAC, aptX, aptX-HD, AAC, SBC | Negotiated codec |

---

## Quick Start: Installation

> [!IMPORTANT]
> This firmware is tested specifically on the **TempoTec Variations V1**. Do not flash on other models or unverified revisions without a recovery backup.

1. Download **`v1.upt`** and **`v1_md5.txt`** from the latest release on the [Releases page](https://github.com/qir3k/sonix-player-by-qirke/releases).
2. Copy both files to the **root directory** of your microSD card (formatted as FAT32 or exFAT).
3. Insert the card into your V1, ensure the battery is charged above 50%, and go to the system update menu in settings.
4. Let the firmware installer complete and reboot automatically.
5. On the first boot, a welcome popup will invite you to visit **Settings → Plugins** to activate any desired games, streaming services, or extra tools.

### Over-The-Air (OTA) Updates
Once running this firmware, updates are easier than ever:
- Connect your V1 to Wi-Fi.
- Go to **Settings → System → Firmware update**.
- The player automatically checks GitHub Releases, displays the new version notes, downloads the update file with a live progress bar, and prompts you to restart and install!

### Returning to Stock Firmware
To restore the vendor firmware, simply place the official vendor `v1.upt` and its checksum file on the root of your microSD card and perform the update process again.

---

## Streaming configuration

Streaming services (Qobuz, Tidal, Podcast Index, Last.fm, YouTube Music) can be configured by placing a `streaming-keys.ini` file at the root of your microSD card.

Copy `sonix-player/streaming-keys.ini.example` to the root of your microSD card as `streaming-keys.ini` and fill in your keys:

```ini
[qobuz]
app_id =
app_secret =

[tidal]
client_id =
client_secret =

# InnerTube public web client key (see YOUTUBE_MUSIC_SETUP.md)
[ytmusic]
api_key =

[podcast]
api_key =
api_secret =

[lastfm]
api_key =
api_secret =
```

- **YouTube Music**: Does not require a personal paid account key! It uses the public InnerTube API key provided by the web client. See [YOUTUBE_MUSIC_SETUP.md](sonix-player/YOUTUBE_MUSIC_SETUP.md) for detailed instructions.
- **Subsonic / Navidrome**: No keys file needed—enter your server URL, username, and password directly in the on-screen UI (**Streaming → Subsonic → Server settings**). Self-signed certificates are supported.
- **ListenBrainz**: Enter your personal user token directly on the player in **Settings → Music → ListenBrainz**.

---

## Plugins and Defaults

On a fresh configuration, these are enabled: Subsonic, Audiobooks, EQ, PEQ,
MSEB, Soundfield, Crossfeed, Channel Balance, Volume Limit, Bluetooth Speex
resampling, and Poster. Other streaming providers, games, tools, layouts, and
optional features start disabled. Settings already saved by an existing
installation are preserved.

Open **Settings → Plugins** to change the selection. Most plugin changes need a
restart; disabling a feature hides its related interface and, where applicable,
stops its processing or avoids creating its optional resources. Volume Limit is
optional, but disabling it removes the per-output safety ceiling. See the
[plugin guide](sonix-player/docs/ADDONS.md) for the full registry and behavior.

---

## Build from source

### 1. Host Simulator (Linux)
Test and debug UI and features directly on your desktop:
```bash
sudo apt install build-essential pkg-config libsdl2-dev libfreetype-dev \
    libopusfile-dev libwavpack-dev libasound2-dev
cd sonix-player
make host
./sonix_player_host
```

### 2. Device Firmware (Target MIPS)
Cross-compilation uses the custom GCC MIPS toolchain:
```bash
cd sonix-player
make target
```
To package the final `v1.upt` image:
```bash
cd sonix-packer
./sonix_firmware_packer.sh
./verify_upt.sh v1.upt
```

---

## Branching model & CI

- **`dev`** — Development branch. Pushes that change paths watched by the Tests workflow build the host player and run the curated active test suites; matching pull requests run the same workflow.
- **`personal`** — Firmware/release branch. Each push starts the firmware workflow, which cross-compiles and packages the V1 image, advances its version, and publishes a GitHub Release. The Tests workflow does not currently run on `personal`, so check the `dev` results before merging.

---

## Credits and Acknowledgements

This project is built upon the wonderful work of the open-source audio community:
- **[Jepl4r](https://github.com/Jepl4r)** — original creator and maintainer of [Sonix Player](https://github.com/Jepl4r/sonix-player).
- **[mr-f0xx](https://github.com/mr-f0xx)** — author of the original [TempoTec V1 port](https://github.com/mr-f0xx/sonix-player-tempotec-v1).
- **[qirke](https://github.com/qir3k)** — author and maintainer of this fork (streaming additions, UI/UX polish, DSP optimizations, touch games, clean cache, and OTA release workflows).
- **LVGL**, **Gearboy**, **dr_libs**, **stb**, **miniz**, **SQLite**, and **FreeType** teams.

Licensed under **GNU General Public License v3.0**.
