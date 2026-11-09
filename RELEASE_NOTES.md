# Sonix OS v1.2.7 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.2.7

### 1. Gramophone Turntable Vinyl Layout Overhaul
- **Root Cause**: Previously, the vinyl layout placed a square sleeve at the top (`VINYL_TOP = 10`) with only a bottom crescent of the vinyl disc peeking out underneath (`VINYL_PEEK_PERCENT`). The disc was static and partially cut off, the album art was square, and large streaming badges (e.g. YouTube Music, Tidal) spilled over the disc and cover frame.
- **Resolution**:
  - **Gramophone Turntable Design**: Completely redesigned the Vinyl player layout to look like an authentic gramophone turntable:
    - Album art is now displayed as a circular center label (`LV_RADIUS_CIRCLE`) centered directly on the black vinyl record disc with a dynamic perimeter ring tinted in the active theme/artwork accent (`chrome_accent()`).
    - Added 4 realistic concentric sound grooves (`grooves[4]`) and a subtle ambient light reflection (`shine`) across the vinyl surface.
    - Added an authentic silver-rimmed center spindle pin hole (`spindle`) positioned directly over the center label axis.
    - Streaming badges (YouTube Music, Tidal, Podcasts, Qobuz) are neatly positioned in the top-right corner (`-10, 10`) of the artwork panel with full clearance of the disc.

### 2. Seamless Vinyl Layout Switching & State Restoration Fix
- **Root Cause**: When switching away from Vinyl layout to any other layout (Standard, Poster, Alternative, Studio, Immersive), `vinyl_put()` had left `cover_art_w`, `cover_art_h`, and `cover_art_group` shrunken to the small center label diameter (110 px), with circular radiuses and 2 px border. `vinyl_take_back()` never restored sleeve dimensions or removed the border, and `update_layout()` lacked `cover_shape_vinyl` tracking, leaving the cover image cached at small label resolution and corrupting subsequent layouts until a device reboot.
- **Resolution**:
  - `vinyl_take_back()` thoroughly restores sleeve dimensions (`compact_sleeve_side()`), border width (0), sleeve corner radius (`bp_is_tempotec_v1() ? 14 : 18`), centered image alignment, and resets placeholder geometry.
  - Integrated `cover_shape_vinyl` tracking in `update_layout()` and `reload_cover()` to automatically trigger full-resolution cover image re-decoding whenever switching between Vinyl and standard rectangular sleeve arrangements.

### 3. Transport Controls & Podcast Seek Margin Spacing
- **Root Cause**: In compact deck layouts (especially when playing podcasts or audiobooks in Studio and Standard layouts), the 42 px skip buttons (`COMPACT_SKIP_BTN`) with 8 px gap filled 144 px of the 216 px row, leaving exactly 36 px on each side. Because the corner buttons (speed indicator, repeat, and favorite heart) are also 36 px wide, the 30-second forward seek button (`(30)`) and the favorite heart touched with 0 px separation.
- **Resolution**:
  - Refined `COMPACT_SKIP_BTN` to 38 px and column padding to 6 px.
  - The centered transport now measures 132 px wide (38 + 6 + 44 + 6 + 38), leaving 42 px on each side and creating an exact, comfortable 6 px visual margin between transport buttons (`(10)`, `(30)`) and corner controls (speed gauge, repeat, and favorite heart).

### 4. Build System & Compilation Stability
- Forward-declared `compact_sleeve_side()` in `player.c` to maintain strict C99 compliance and ensure reliable cross-compilation across MIPS toolchains.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
