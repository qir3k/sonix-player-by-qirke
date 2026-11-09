# Sonix OS v1.2.3 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.2.3

### 1. Three-Dots Menu Button Centering & Symmetry (Immersive, Standard & Waveform Layouts)
- **Root Cause**: The bottom controls row (`player_controls_buttons`) sized the rightmost overflow/three-dots button (`more_btn_obj`) at 44 px wide (`COMPACT_MORE_BTN`) while the mirrored shuffle/repeat button (`repeat_btn_obj`) on the left was 36 px wide (`COMPACT_CORNER_BTN`). Because both corner buttons are edge-aligned (`LV_ALIGN_LEFT_MID` and `LV_ALIGN_RIGHT_MID`), the 44 px button placed the centered three-dots icon 22 px from the row edge, shifting it 4 px to the left compared to the shuffle icon (18 px from the edge) and making it noticeably off-center across the Immersive, Standard, and Waveform layouts.
- **Resolution**:
  - Unified `COMPACT_MORE_BTN` to 36 px matching `COMPACT_CORNER_BTN`, achieving exact pixel-perfect horizontal symmetry (18 px from edge, 28 px from display bezel on both sides).
  - Maintained `COMPACT_MORE_BTN_EXT_CLICK` (10 px padding) ensuring a generous 56x56 px touch hit target for comfortable thumb tapping.
  - Ensured identical symmetrical sizing is restored cleanly when returning from Album Art Max and Studio layouts.

### 2. Immersive Layout Favorite Heart Symbol Cropping & Alignment Fix
- **Root Cause**: In the Immersive layout, `immersive_put()` applied an artificial -2 px vertical translation (`translate_y`) to `fav_btn_icon` inside a compact 19 px button (`COMPACT_FAV_BTN_H`). With the heart icon scaled to 22 px high (`FAV_HEART_SCALE = 176`), this pushed the top lobes of the heart outside the button boundary, where container clipping cropped the top curves flat. Furthermore, the icon's color recolor and right edge alignment with the format label underneath needed seamless vertical centering without displacement.
- **Resolution**:
  - Removed negative vertical translate on `fav_btn_icon` and increased button height to 26 px (`COMPACT_FAV_BTN_H 22`), providing full top and bottom clearance for the 22 px heart icon without any clipping.
  - Aligned the visual right edge of the heart icon flush with the format label while keeping it vertically centered on the row.
  - Ensured favorite heart coloring dynamically uses `chrome_accent()` so that active theme colors and album art tinting apply immediately upon entering Immersive layout.

### 3. Streaming Badges Scaling & Placement (Standard, Waveform & Vinyl Layouts)
- **Root Cause**: While Studio layout had badge downscaling logic, Standard layout, Waveform layout, and Album Art Max never downscaled the streaming badges (YouTube Music, Tidal, Podcasts, Qobuz), rendering them at their massive unscaled 52x52 px native dimensions (~34% of sleeve width). This caused streaming badges to spill over album art and obstruct covers. Additionally, transitioning out of Studio or Vinyl layouts reset badges back to unscaled 52x52 px at static -14 px insets.
- **Resolution**:
  - Introduced unified `cover_badges_apply()` for cover art across Standard, Waveform, Alternative, Immersive, Poster, and Vinyl layouts.
  - All streaming badges are cleanly downscaled via `iconscale_set()` to native 26 px (V1) / 36 px (wide displays) with a 6 px inset, sitting neatly and proportionally in the top-right corner of the artwork sleeve without overflowing.
  - Guaranteed correct badge z-order and placement on layout switches and track changes.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
