# Sonix OS v1.2.5 for TempoTec Variations V1

## 🛠️ Fixes & Improvements in v1.2.5

### Poster Layout Favorite Heart Left-Alignment
- **Issue**: In the Poster player layout, the favorite heart button (`fav_btn_obj`) and icon (`fav_btn_icon`) were right-aligned (`LV_FLEX_ALIGN_END`) within the metadata column (`song_side_obj`), while the sound quality / format info label (`format_label`, e.g. "24/44.1 FLAC") spanned to the left. This caused the heart icon to sit on the far right edge of the screen, misaligned with the sound quality information beneath it.
- **Resolution**:
  - Configured `song_side_obj` to use start/left cross-alignment (`LV_FLEX_ALIGN_START`) in `poster_put()`.
  - Left-aligned the heart icon (`LV_ALIGN_LEFT_MID`) inside `fav_btn_obj` and set `format_label` text alignment to `LV_TEXT_ALIGN_LEFT`.
  - Guaranteed that when switching away from Poster layout, `poster_take_back()` cleanly restores standard right-alignment (`LV_FLEX_ALIGN_END`, `LV_TEXT_ALIGN_RIGHT`, and centered heart icon) for all other layouts.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
