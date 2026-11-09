# Sonix OS v1.2.6 for TempoTec Variations V1

## 🛠️ Fixes & Improvements in v1.2.6

### 1. Gramophone Vinyl Player Layout Overhaul
- **Gramophone LP Design**: Completely redesigned the Vinyl player layout to look like an authentic gramophone turntable:
  - The album art is presented as a circular center label (`LV_RADIUS_CIRCLE`) in the center of the vinyl record disc with dynamic `chrome_accent()` perimeter ring.
  - Realistic concentric sound grooves (`grooves[4]`) and subtle light sheen reflection line (`shine`) on the vinyl disc body.
  - Authentic silver-rimmed spindle center pin hole (`spindle`) positioned over the label axis.
  - Streaming badges (YouTube Music, Tidal, Podcasts, Qobuz) are neatly placed in the upper-right corner without obscuring the turntable disc.

### 2. Seamless Layout Switching & State Restoration Fix
- **Issue**: Switching away from the Vinyl layout previously left sleeve dimensions shrunk to label size, corrupted placeholder positioning, and caused cover art rendering glitches that forced a device reboot.
- **Resolution**:
  - `vinyl_take_back()` thoroughly restores sleeve dimensions (`compact_sleeve_side()`), border width, corner radiuses, and badge attachments.
  - Added `cover_shape_vinyl` tracking to `update_layout()` and `reload_cover()`, properly triggering cover re-decoding when switching between Vinyl and standard rectangular sleeve layouts.
  - Restored proper placeholder icon alignment and geometry on layout exit.

### 3. Transport Controls & Podcast Seek Margin Spacing
- **Issue**: In compact deck layouts (e.g. podcast playback and studio layout), the forward seek button (`(30)`) and the favorite heart button collided with virtually 0 px separation.
- **Resolution**:
  - Adjusted `COMPACT_SKIP_BTN` to 38 px and column spacing to 6 px, guaranteeing a comfortable, symmetrical 6 px margin between transport seek buttons and corner controls (speed gauge, repeat, and favorite heart).

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
