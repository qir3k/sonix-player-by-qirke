# Features & Deep Technical Guide

Comprehensive guide to all features, capabilities, hardware controls, and design decisions in **Sonix Player for TempoTec Variations V1 (by qirke)**.

---

## Table of Contents

- [Supported Audio Formats & Containers](#supported-audio-formats--containers)
- [Audio Engine & DSP Processing](#audio-engine--dsp-processing)
- [Library, Organization & Navigation](#library-organization--navigation)
- [Smart Mix (On-Device Genius Engine)](#smart-mix-on-device-genius-engine)
- [Now Playing & Visual Deck Layouts](#now-playing--visual-deck-layouts)
- [Control Center & Gestures](#control-center--gestures)
- [Streaming & Network Services](#streaming--network-services)
- [Bluetooth & Earbuds Integration](#bluetooth--earbuds-integration)
- [Built-In Touchscreen Games](#built-in-touchscreen-games)
- [Productivity Tools & Readers](#productivity-tools--readers)
- [System, Power, Cache & Memory Management](#system-power-cache--memory-management)
- [Appearance, Themes & Fonts](#appearance-themes--fonts)

---

## Supported Audio Formats & Containers

`.wav` `.flac` `.mp3` `.ogg` `.opus` `.m4a` `.m4b` `.mp4` `.aac` `.wv` `.wvc` `.ape` `.dsf` `.dff` `.aif` `.aiff` `.aifc` `.caf`

- **CUE Sheets & Split Tracks** — Splits a single ripped disc image into up to 99 individual tracks, extracting the audio format from the underlying file (and resolving file stems even when extensions differ from the `FILE` statement). WAV files carrying internal RIFF markers work seamlessly.
- **Format & Container Tolerance** — Files are identified by their real magic bytes and container headers rather than extensions. FLAC files carrying leading ID3v2 headers, broken or non-standard ID3v2 tags (v2.4 non-syncsafe sizes, unsynchronised UTF-16 frames, zlib-compressed frames, BOM-less UTF-16, chained tags), multi-page Ogg Vorbis/Opus comment headers, and APEv2/ASF/MP4/RIFF/AIFF/CAF metadata are all decoded reliably without glitching or crashing.
- **WAV & AIFF Tags** — Reads ID3 chunks embedded in uncompressed containers by modern taggers, retrieving artwork, multi-language titles, and artist credits; fallback to WAV INFO lists when ID3 chunks are absent.
- **True Lossless ALAC** — Native first-class ALAC decoder preserving full 16-bit and 24-bit depth bit-identically (not downsampled or treated as lossy AAC).
- **Bit-Perfect DSD** — Direct DSD-over-PCM (DoP) for DSD64, DSD128, and DSD256 sent untouched to the Cirrus Logic CS43131 DACs. When streaming to Bluetooth headphones (which have no native DSD protocol), DSD is cleanly converted on the fly to 176.4 kHz PCM.

---

## Audio Engine & DSP Processing

- **Volume Curve & Independent Memory** — Uses the tuned logarithmic volume curve matched to the hardware amplifier. Four discrete volume levels are remembered independently:
  1. 3.5 mm Single-Ended output
  2. 4.4 mm Balanced output
  3. USB-C Host Audio out
  4. Bluetooth output
- **Graphic Equaliser (EQ)** — 10 bands (31 Hz, 62 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz, 16 kHz), ±12 dB adjustment. Includes 10 tuned factory presets and unlimited custom user presets.
- **Parametric Equaliser (PEQ)** — 10 fully parametric filters (20 Hz to 20 kHz, gain ±15 dB in 0.1 dB increments, Q from 0.10 to 10.00). Supports five filter shapes: Peak, Low Shelf, High Shelf, Low-Pass, and High-Pass, with global preamp from −15 dB to +6 dB. The screen renders the computed real-time frequency response curve. Supports direct loading of Equalizer APO text files and AutoEq profile exports from the `PEQ` folder on the microSD card.
- **MSEB (MageSound 8-ball)** — HiBy's celebrated acoustic tuning engine: 10 characteristics driven by 13 internal biquad filters, with configurable slider travel sensitivity (20, 40, or 100).
- **DSP on EVERYTHING** — Graphic EQ, PEQ, and MSEB run identically across **local music files, internet radio streams, streaming services, and external computer audio in USB DAC mode**! The full DSP chain operates directly on stereo PCM frames prior to the hardware DAC.
- **Dedicated DAC Page DSP Controls** — Quick EQ, PEQ, and MSEB toggle pills on the USB DAC display so sound processing can be adjusted without disconnecting the computer host.
- **Soundfield (Stereo Width)** — Mid/side stereo widening adjustable from mono (0.00) to double natural width (2.00), with 1.50 as the default engaging expansion.
- **Channel Balance** — Fine balance adjustment across ±20 dB in half-decibel (0.5 dB) increments.
- **Crossfeed (Bauer Headphone Simulation)** — Simulates natural speaker listening on headphones with adjustable crossfeed level, cutoff frequency, and interaural delay.
- **ReplayGain** — Automatic loudness normalization supporting both Track and Album modes read from metadata tags.
- **Gapless Playback** — Enabled by default. Keeps the ALSA PCM device open across tracks of matching sample rate and bit depth, eliminating mute delays and audio clicks.
- **Line Out Mode** — Sets fixed maximum output voltage for external headphone amps and locks volume adjustments to prevent accidental level jumps.

---

## Library, Organization & Navigation

- **Multi-Script Collation** — Unified sorting grouping titles across Latin, Cyrillic, Greek, CJK, and Arabic scripts, folding case and diacritics with an option to ignore leading articles ("The", "A", "An").
- **A–Z Quick Scrub Strip** — Dynamic alphabet scrubber displayed automatically when scrolling long lists for rapid jumping.
- **Unified Global Search** — Instant real-time filtering across tracks, albums, and artists.
- **Smart Disambiguation** — Albums sharing identical titles remain separated based on Album Artist or directory hierarchy.
- **Quality Badges** — Clear visual badges under titles indicating Lossy, CD (16-bit), Hi-Fi (24-bit Hi-Res), or DSD.
- **Customizable Main Menu Tiles** — Reorder or hide home tiles (All Tracks, Albums, Artists, Genres, Folders, Playlists, Years, Most Played) in `Music → Settings → Library → Rearrange main page`.
- **Batch Selection & Management** — Long-press any list row to enter multi-select mode: bulk add to playlists, queue immediately, add to favorites, or delete permanently from the microSD card.
- **Queue State Survival** — The active playback queue, exact playhead position, track index, and volume persist across device restarts.

---

## Smart Mix (On-Device Genius Engine)

- **Seed from Any Track** — Choose **Start Smart Mix** from any track's context menu, details card, or search result to create a 10, 15, 20, 25, or 30-track mix.
- **Saved as Playlists** — Mixes are automatically saved into your `Playlists` folder as `Smart Mix - <Song Title>` without interrupting current playback.
- **100% Offline & On-Device** — Built during or after library scans using local SQLite tag graphs in `.local/genius.db`. Zero network connection or cloud servers needed.
- **Intelligent Clustering** — Evaluates tracks based on shared genre clusters, artist affinity, release year proximity, and acoustic properties.

---

## Now Playing & Visual Deck Layouts

- **Six Distinct Visual Layouts** (selectable in `Settings → Music → Display options`):
  1. **Standard** — Balanced view with full artwork, progress bar, metadata, and quick transport.
  2. **Waveform** — Progress bar transforms into an animated dynamic audio waveform scrubber.
  3. **Alternative** — Compact deck emphasizing high-legibility track titles and format specs.
  4. **Immersive** — Artwork extends edge-to-edge across the entire display with frosted glass control overlays.
  5. **Poster** — Edge-to-edge album cover centered above an opaque control deck with color fades.
  6. **Album Art Max** — Full-width album cover view with minimal scrub lines and gesture navigation (double-tap sides to skip tracks).
- **Service Branding Watermarks** — Clean circular branding watermarks in the top-right corner of Now Playing for YouTube Music, Qobuz, Tidal, Internet Radio, and Podcasts.
- **Synced Lyrics & LRCLIB** — Swipe left on artwork to view synchronized `.lrc` or plain lyrics. If missing, look them up immediately on LRCLIB over Wi-Fi and save them beside the audio file.
- **Aligned Metadata Badges** — Perfectly aligned artist name, stream bitrate (e.g. 320 kbps), and format badges across both Internet Radio and Podcasts.
- **Screensaver Visualizers** — Choose between Album Art, custom picture slideshows, or real-time audio visualizers:
  - **Neon** — Eight wide segmented bars glowing in your theme accent color.
  - **LED** — Classic bench-style spectrum analyzer with sixteen horizontal segment bars and falling peak needles.

---

## Control Center & Gestures

- **Fluid Swipe Gestures** — When more than 8 control center tiles are active, swipe smoothly left or right between pages. Vertical pull-down tabs and clunky slider handles have been replaced with natural touch paging.
- **Instant DSP Toggles** — Tap illuminated pills to toggle EQ, PEQ, or MSEB instantly, with support for cycling PEQ profiles.
- **Configurable Slider** — Choose whether the lower slider controls Display Brightness or Output Volume (`Settings → Screen → Volume in control centre`).
- **One-Touch Now Playing** — Tap the currently playing track banner in the Control Center to jump straight into the full playback screen.

---

## Streaming & Network Services

- **YouTube Music (Standalone)** — Search songs, artists, and playlists, browse top charts and Explore, stream audio on the fly, and download tracks directly to microSD. Supports session cookie import from desktop browsers and fallback through Invidious / Piped.
- **Tidal & Qobuz** — High-res streaming up to 24-bit FLAC, album/artist navigation, online favorites sync, and local offline downloads.
- **Subsonic / Navidrome / Gonic** — Stream from your personal music server over HTTP/HTTPS with dual 1 MiB RAM ring buffers and support for self-signed certificates.
- **Internet Radio** — Browse thousands of worldwide stations via radio-browser.info by country, genre, or bitrate (up to 320 kbps and lossless FLAC streams), plus custom station lists via `radio.txt`.
- **Podcasts** — Browse and search the Podcast Index directory, follow shows, and stream or download episodes for offline listening.
- **Streaming Key Protection** — Services requiring API credentials gracefully prompt the user when keys are missing from `streaming-keys.ini`, preventing crashes and hiding inactive search/download icons.

---

## Bluetooth & Earbuds Integration

- **Hi-Res Bluetooth Codecs** — LDAC (with quality tier diagnostics), aptX, aptX-HD, AAC, and SBC.
- **Bi-Directional Operation** — Stream music to wireless headphones/speakers, or use the V1 as a high-fidelity Bluetooth DAC receiver for your smartphone or tablet.
- **Apple AirPods & Beats Integration** — Real-time battery percentages (left earbud, right earbud, charging case) and Noise Cancellation / Transparency mode toggles.
- **EarFun GAIA Protocol** — Custom serial control protocol for compatible EarFun earbuds, including battery monitoring, ANC modes, and Low Latency Game Mode.
- **AirPlay & DLNA** — Stream bit-perfect audio from iOS, macOS, Windows, and Linux over local Wi-Fi.

---

## Built-In Touchscreen Games

All games have been custom-tailored for the V1's 2.0" 240×320 capacitive touchscreen:

- **Brick Breaker** — Classic arcade breakout game featuring fluid, continuous touchscreen paddle dragging (no jumpy teleportation).
- **Minesweeper** — Fast, responsive minefield sweeper with calibrated touch offset (no clicking below your finger), quick tap to reveal, and long-press to flag.
- **Sudoku** — Interactive grid number puzzle with calibrated cell detection and clean touch controls.
- **Guess The Song** — Engaging music quiz that scans your library and plays real audio clips directly through your headphones or DAC for you to identify!
- **2048** — The classic sliding number puzzle with responsive swipe physics.
- **Snake** — Retro arcade snake with intuitive directional touch gestures.
- **Chrome Dino** — The beloved endless desert runner adapted for the pocket screen.
- **Gearboy (Game Boy / Game Boy Color)** — High-performance emulator running at 240×216 with nearest-neighbour scaling. Physical Play/Pause button maps to Start, and Track Skip buttons map to Select for comfortable tactile gaming.

---

## Productivity Tools & Readers

- **Scientific Calculator** — Touch-friendly calculator with standard arithmetic and scientific functions.
- **Text Notes Editor** — View and edit `.txt` notes saved in the `Notes` folder on your microSD card.
- **On-Device Tag Manager** — Edit ID3/FLAC metadata tags and look up cover artwork directly on the DAP without needing a PC.
- **Metronome** — High-precision audio metronome with customizable tempo and visual beat flash.
- **EPUB eBook Reader** — Read `.epub` books with custom font sizing, dark mode rendering, and reading position memory.

---

## System, Power, Cache & Memory Management

- **RAM-First Architecture** — Non-essential modules (games, non-audio tools) default to OFF on fresh installations, booting the DAP with ~45–50 MB of free RAM.
- **Modular Plugins Manager** — Every optional service, tool, and game can be toggled on/off in `Settings → Plugins`. Disabled modules are completely omitted from RAM allocations.
- **First-Run Welcome Prompt** — Introduces new users to the player and directs them to `Settings → Plugins` to activate their desired features.
- **Unified Cache Hygiene** — Eliminates legacy empty directories (`qobuz-art`, `radio-art`, `podcast-art`, `tidal-art`) in favor of a clean, self-sweeping unified cache.
- **Intelligent Battery Management** — Visual warning card at 5%, graceful shutdown at 1%, and a dedicated charging HUD when plugged into power while turned off.
- **Over-The-Air (OTA) Updates** — Seamless one-click firmware updates fetched directly from GitHub Releases (`Settings → System → Firmware update`).

---

## Appearance, Themes & Fonts

- **Themes** — Four built-in themes: Dark, Light, Amber on Black, and true black OLED (with zero grey backgrounds and pure white typography). Custom theme INI files supported from the microSD card.
- **12 Interface Languages** — Čeština, English, Deutsch, Español, Français, Italiano, Japanese, Nederlands, Polski, Português BR, Russian, and Ukrainian.
- **Dynamic Font Fallback** — Uses MiSans, Neon 80s, and Roboto, with dynamic glyph fallbacks for Cyrillic, Greek, East Asian CJK, and Arabic scripts. Custom TTF/OTF fonts can be loaded from the `Fonts` folder on the card.
