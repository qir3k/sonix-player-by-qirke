# Sonix Player for TempoTec Variations V1 (by qirke)

[![Build and Release](https://github.com/qir3k/sonix-player-tempotec-v1/actions/workflows/build-tempotec-v1.yml/badge.svg)](https://github.com/qir3k/sonix-player-tempotec-v1/actions)
[![Latest Release](https://img.shields.io/github/v/release/qir3k/sonix-player-tempotec-v1?include_prereleases&label=release)](https://github.com/qir3k/sonix-player-tempotec-v1/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

An advanced, modern, audiophile custom firmware for the **TempoTec Variations V1** digital audio player.

Sonix Player for the TempoTec Variations V1 follows a clear lineage: **[Jepl4r's Sonix Player](https://github.com/Jepl4r/sonix-player)**, then **[MrFox's V1 fork](https://github.com/mr-f0xx/sonix-player-tempotec-v1)**, which originally ported the player to the TempoTec Variations V1, and now this heavily expanded, optimized, and maintained fork by **qirke**.

---

## 🌟 Why choose this fork?

If you own a **TempoTec Variations V1**, this firmware turns your compact DAP into a modern, fast, and feature-rich audiophile machine, freeing it from the slow and outdated stock vendor HiByOS.

### Key highlights & unique enhancements in this fork:
* **🚀 RAM-First & Lightweight by Default** — Built around aggressive memory management inspired by Compas OS. On a fresh install, heavy non-essential modules (games, non-audio tools) start switched off so the device boots with maximal free RAM (~45–50 MB free), ensuring snappy scrolling, instant responsiveness, and stutter-free playback with huge microSD libraries.
* **🔌 Modular Plugin Architecture with First-Run Welcome** — Customize exactly what you need in `Settings → Plugins`. Disabled modules are never allocated in RAM. A helpful first-run prompt welcomes you and guides you to Plugins so you can turn on your favorite extras.
* **🎵 YouTube Music without a Phone or PC** — Search, browse charts, Explore, albums, and playlists right on the player. Enjoy smooth on-the-fly streaming, offline downloads to microSD, and session import for your own personal library.
* **🎛️ Audiophile DSP on EVERYTHING** — 10-band Graphic EQ, 10-band Parametric EQ (with live frequency curve rendering and direct import of AutoEq / Equalizer APO text files), and MSEB (MageSound 8-ball). Unlike other players, DSP processing works across **local tracks, streaming services, internet radio, and external USB DAC input**!
* **👆 Modernized Control Center & Gestures** — Redesigned Control Center with fluid horizontal swipe gestures (swipe left/right) to navigate multiple tile pages seamlessly—no awkward sliders or handles.
* **🖼️ Refined Now Playing & Visuals** — Brand watermarks on Now Playing (YouTube Music, Qobuz, Tidal, Radio, Podcasts), perfectly vertically aligned artist and bitrate badges across radio and podcasts, and clean layout geometry designed specifically for the 2.0" 240×320 screen.
* **🎮 Touchscreen-Optimized Games Suite** — Includes Brick Breaker (with smooth paddle dragging), Minesweeper (tap to reveal, hold to flag, calibrated touch offset), Sudoku, Guess The Song (interactive music quiz playing real audio clips), Flappy Bird, 2048, Snake, Chrome Dino, and the Gearboy Game Boy emulator.
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

The player UI is built with LVGL 9.6, strictly customized and compiled for the V1's 240×320 geometry. Unnecessary layouts and legacy HiBy targets have been completely removed.

---

## Features overview

- **Interface languages** — 12 language packs selectable on the fly (Čeština, English, Deutsch, Español, Français, Italiano, Japanese, Nederlands, Polski, Português BR, Russian, Ukrainian), with dynamic script fallbacks for East Asian, Cyrillic, and Arabic text.
- **Local playback & library** — Tag-based library browsing, folder navigation, A-Z fast scroll strip, instant search, smart playlists, play counts, audiobook resume, and lyrics display.
- **Streaming & network audio** — YouTube Music, Tidal, Qobuz, Subsonic / Navidrome / Gonic, Internet Radio directory, Podcast Index with episode downloads, Bluetooth receiver & transmitter, AirPlay, and DLNA.
- **Bluetooth audio** — High-resolution LDAC with quality/bitrate diagnostics, AAC, aptX, aptX-HD, and SBC. Native protocol support for Apple AirPods / Beats battery & ANC control, as well as EarFun earbuds over GAIA.
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

1. Download **`v1.upt`** and **`v1_md5.txt`** from the latest release on the [Releases page](https://github.com/qir3k/sonix-player-tempotec-v1/releases).
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

To guarantee maximum speed and free memory, this fork implements a **minimalist, RAM-first default**:
* **On by default**: Subsonic streaming, Audiobooks, Graphic Equaliser (EQ), Parametric Equaliser (PEQ), and MSEB.
* **Off by default**: Games, non-music tools (calculator, notepad, metronome, tag manager), headphones serial services (AirPods, EarFun), and unconfigured streaming providers.

You can turn any feature on in seconds under **Settings → Plugins** (changes take effect on next restart, freeing memory when disabled).

For full details on the plugin system, see [ADDONS.md](sonix-player/docs/ADDONS.md).

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

- **`dev`** — Active development branch. Every push automatically compiles the host player and runs all unit tests via GitHub Actions.
- **`personal`** — Release branch. Merging `dev` into `personal` triggers the automated CI/CD workflow, which runs test suites, cross-compiles the MIPS target, packages `v1.upt`, and automatically creates a new GitHub Release with release notes!

---

## Credits and Acknowledgements

This project is built upon the wonderful work of the open-source audio community:
- **[Jepl4r](https://github.com/Jepl4r)** — original creator and maintainer of [Sonix Player](https://github.com/Jepl4r/sonix-player).
- **[mr-f0xx](https://github.com/mr-f0xx)** — author of the original [TempoTec V1 port](https://github.com/mr-f0xx/sonix-player-tempotec-v1).
- **[qirke](https://github.com/qir3k)** — author and maintainer of this fork (streaming additions, UI/UX polish, DSP optimizations, touch games, clean cache, and OTA release workflows).
- **LVGL**, **Gearboy**, **dr_libs**, **stb**, **miniz**, **SQLite**, and **FreeType** teams.

Licensed under **GNU General Public License v3.0**.
