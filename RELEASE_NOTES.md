# Sonix OS v1.2.8 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.2.8

### 1. Gramophone Vinyl Layout Album Art Centering Fix
- **Root Cause**: In Vinyl layout, `compact_place_sleeve()` had previously anchored `cover_art_group` to `LV_ALIGN_TOP_MID`. When `vinyl_put()` subsequently called `lv_obj_set_pos()`, LVGL applied the coordinates as an additional offset relative to `TOP_MID`, adding the horizontal center offset twice and displacing the circular cover art far to the top-right of the turntable.
- **Resolution**:
  - Anchored `cover_art_group`, `vinyl_record.disc`, and `vinyl_record.spindle` explicitly to `LV_ALIGN_TOP_LEFT` using calculated box bounds.
  - The circular album art now sits perfectly concentric in the dead center of the vinyl grooves and spindle hole.

### 2. Universal Favorite Heart Symbol Right-Alignment (Standard, Poster & Vinyl Layouts)
- **Root Cause**: While Immersive layout used a compact 26 px button with `translate_x = 2` to align the heart flush with the right edge of the audio format line, Standard and Vinyl layouts kept a wider 34 px button with centered icon (shifting the heart 6 px to the left), and Poster layout had overridden column alignment to `LV_FLEX_ALIGN_START` (nearly centering the heart on the screen).
- **Resolution**:
  - Reverted Poster layout to `LV_FLEX_ALIGN_END` and unified `fav_btn_obj` to 26 px width (`COMPACT_FAV_BTN_H 22`) with `translate_x = 2` and 8 px extended click padding across Standard, Poster, Vinyl, and Immersive layouts.
  - The favorite heart is now pixel-perfect, right-aligned flush with the audio format label across all player layouts.

### 3. Top-Right Three-Dots Menu Button Touch Interception Fix (Studio & Alternative Layouts)
- **Root Cause**: The invisible pull-down gesture strip (`top_edge`), positioned at the very top of `player_screen` across the entire 240 px width, was moved to the foreground and captured `LV_EVENT_PRESSED`. This absorbed screen taps intended for the top-right overflow button (`more_btn_obj`) in the header bar, making the three dots almost impossible to trigger without swiping beneath the top strip.
- **Resolution**:
  - Added an advanced hit-test handler (`top_edge_hit_test_cb`) using `LV_EVENT_HIT_TEST` on `top_edge`.
  - Touches aimed at `more_btn_obj` (and `lyrics_find_btn`) bypass `top_edge` entirely (`info->res = false`), providing instant, effortless tap responsiveness with a generous 16 px touch radius.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
