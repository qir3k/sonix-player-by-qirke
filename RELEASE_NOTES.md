# Sonix OS v1.1.46 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.1.46

### 1. Build & Compilation Stability
- **Root Cause**: An undeclared font reference (`font_ui_12`) in `player.c` caused target cross-compilation in GitHub Actions to fail (`Process completed with exit code 2`).
- **Resolution**: Updated `player.c` to use standard UI fonts (`font_ui_14` on V1, `font_ui_16` on larger boards) with dynamic layout width calculation for the live indicator badge.

### 2. Alternative & Studio Layout Streaming Badges
- **Root Cause**: Streaming service badges (YouTube Music, Podcasts, Tidal, Radio) were rendered at full $52 \times 52\text{ px}$ resolution and anchored to the outer container edges rather than the centered album sleeve, causing them to hover awkwardly over dark empty space.
- **Resolution**:
  - Scaled down badges via `lv_image_set_scale` to a clean proportional size ($26\text{ px}$ on V1, $36\text{ px}$ on V2).
  - Anchored badges cleanly to the top-right corner of the album artwork sleeve with proper inset padding from rounded sleeve borders.
  - Plakietki dynamically slide with the album artwork during horizontal lyrics swipe gestures (`lyrics_shift`).

### 3. YouTube Music Bitrate & Format Detection
- **Root Cause**: YouTube MP4/M4A audio streams do not populate `avg_bitrate` in the `esds` atom, and cache-backed playback bypassed bitrate calculation, causing the format label to display only `"AAC"` without a bitrate.
- **Resolution**:
  - Implemented comprehensive YouTube Music stream detection in `player.c` and `trackmenu.c`.
  - Automatically calculate bitrate from total downloaded file size and track duration once stream download finishes.
  - Added fallback to nominal bitrate configuration (`128 kbps` normal, `256 kbps` high quality) with standard CBR itag rounding. Format label now reliably displays e.g. **`128 kbps AAC`** or **`256 kbps AAC`**.

### 4. Immersive & Alternative Layout Refinements
- **Immersive Layout**: Raised track metadata, title, artist, and favorite heart icon by several pixels, giving proper vertical breathing room from the bottom deck border.
- **Alternative / Studio Layout**:
  - Enlarged favorite heart icon and applied active theme accent color.
  - Enabled artwork color tinting on transport and deck controls when "Tint controls from album art" is enabled.
  - Vertically centered the audio quality pill (e.g. `16/44.1 FLAC`) next to the quality indicator mark.
  - Enlarged the hit-test bounds for the top-right 3-dots kebab menu button for easier tapping.
- **Vinyl Record Sleeve Layout**: Enabled the vinyl record sleeve layout option in Music Settings (`PLAYER_LAYOUT_VINYL`).

### 5. Smart Mix Modal Dialog Polishing
- Removed distracting white outer border from the Smart Mix "Building Smart Mix..." loading dialog.
- Perfectly aligned the spinning loader animation and themed background card.

### 6. YouTube Music Podcast Mode & Playback Speed
- Fixed podcast detection for YouTube Music episodes to correctly invoke podcast controls and display the playback speed selector.

### 7. Core Touch, Playback, and System Performance (from v1.1.41)
- **Streaming List Item Touch Responsiveness**: Tapping the middle of track rows reliably triggers playback across YouTube Music, Tidal, Qobuz, Podcasts, and Radio.
- **Sheet Swipe Right-to-Left**: Smooth horizontal swipe navigation to Now Playing from scrollable lists.
- **Playback Spontaneous Pauses**: Debounced headphone jack detection (600ms) to eliminate accidental pauses from plug micro-bounce.
- **Memory Optimization (~14MB Saved)**: Background Bluetooth and ADB daemons are fully torn down on boot when disabled in settings.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from settings or recovery menu.
