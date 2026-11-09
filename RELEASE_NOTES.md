# Sonix OS v1.2.10 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.2.10

### 1. Gramophone Vinyl Layout Album Art Enlargement & Turntable Spin Animation
- **Root Cause**: In Vinyl layout, the circular album art label was configured at only 50% of the vinyl disc diameter (`max_d * 50 / 100`), making the cover appear disproportionately small compared to the black outer grooves. In addition, no rotation animation was driving the record or artwork during playback.
- **Resolution**:
  - Increased circular center album art label size to 68% of the vinyl disc diameter (`(max_d * 68 / 100) & ~1`), providing a prominent, beautifully proportioned cover display while keeping concentric grooves and the chrome rim visible.
  - Implemented real-time turntable spin animation (`vinyl_spin_update` and `vinyl_spin_anim_cb`): when playback is active (`AUDIO_STATUS_PLAYING`) and the now-playing sheet is open, the album art spins smoothly in sync with the gliding groove shine line (`r->shine`) around the central spindle at a realistic, battery-friendly ~24 RPM (2.5s per turn).
  - Pausing playback freezes the rotation in place without resetting the angle, and switching away from Vinyl layout smoothly resets artwork rotation back to 0°.

### 2. Waveform Layout Favorite Heart Centering Fix
- **Root Cause**: In Waveform/Alternative layout, the favorite button (`fav_btn_obj`) is reparented into a circular floating pill container (`alt_fav_circle`, 36×36 px). The button retained its compact rectangular geometry (26×22 px) with `translate_x = 2`, displacing the heart icon off-center relative to the dark background circle.
- **Resolution**:
  - In Waveform layout, `fav_btn_obj` is explicitly sized to match `alt_fav_size` (36×36 px) and centered inside `alt_fav_circle`.
  - Removed local `translate_x` and `translate_y` offsets on `fav_btn_icon` in Waveform layout so the heart is centered with mathematical precision inside the circle.
  - Preserved right-aligned `translate_x = 2` flush positioning when returning to Standard, Poster, Vinyl, and Immersive layouts.

### 3. Control Center (Quick Panel) Page 2 Vertical Centering
- **Root Cause**: In the Quick Panel swipeable button grid, `quick_row` and `extra_row` were statically configured with `LV_FLEX_ALIGN_START` for track cross alignment. When Page 2 contained only a single button or row (≤ 4 buttons), items were pinned to the top ceiling of the grid with an empty lower row area.
- **Resolution**:
  - In `order_apply()`, dynamically evaluate the button counts for both pages: if Page 2 (or Page 1) has only one row (≤ 4 buttons), `lv_obj_set_flex_align` sets `LV_FLEX_ALIGN_CENTER` on the cross axis, centering the row vertically in the card.
  - When 2 full rows (5–8 buttons) are present, alignment seamlessly uses `LV_FLEX_ALIGN_START`, ensuring identical appearance across fully-populated pages.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
