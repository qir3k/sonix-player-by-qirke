# Sonix OS v1.2.2 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.2.2

### 1. Now Playing Favorite Heart Symbol Coloring & Tinting
- **Root Cause**: In monochrome and OLED themes, `update_fav_button()` prematurely fell back to `THEME_PLAYER_CHROME_MUTED` (`#888888`), locking the favorite heart icon in a permanent dull gray across all player layouts even when "Tint controls from album art" or active accent colors were selected.
- **Resolution**:
  - Unified favorite heart coloring to dynamically use `chrome_accent()` across all player layouts.
  - The heart icon (both outline when un-favorited and filled when favorited) now properly inherits the active theme accent color and dynamically adopts album cover tinting when enabled.

### 2. Alternative Layout Streaming Badge Scaling & Clipping Fix
- **Root Cause**: In the Alternative (Studio) player layout, scaling the 52x52 px streaming badges (YouTube Music, Podcasts, Tidal, Qobuz) via `lv_image_set_scale` with an explicit (0, 0) pivot displaced the scaled bitmap relative to the centered 26x26 px widget container, clipping ~75% of the badge and leaving only a distorted fragment visible. Furthermore, background cover updates did not re-assert badge Z-order.
- **Resolution**:
  - Leveraged `iconscale_set()` to cleanly pre-scale badge bitmaps on the FPU to the native 26 px (V1) / 36 px (V2) dimension, eliminating transform shifts and clipping.
  - Ensured streaming badges are positioned and brought to the foreground over the album sleeve whenever the layout is drawn and whenever album art asynchronously finishes loading.
  - Cleanly restored intrinsic 52 px source descriptors upon exiting to the standard layout in `studio_take_back()`.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
