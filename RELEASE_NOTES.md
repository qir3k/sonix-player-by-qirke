# Sonix OS v1.2.1 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.2.1

### 1. Screensaver Backdrop Customization (Blurred vs. Opacity)
- **Overview**: With the Visual Effects addon enabled (`addons_enabled("visual_effects")`), the screensaver background can now be customized directly in **Settings > Screensaver**.
- **Resolution**:
  - Added a new **Screensaver Backdrop** option with two choices: **Blurred** (frosted album art backdrop) and **Opacity** (classic dimmed background with theme ground).
  - Works consistently across **all** player layouts (Default, Compact, Alternative/Studio, Immersive, Poster, Vinyl).
  - Seamlessly falls back to default opacity backdrop when the Visual Effects addon is disabled.

### 2. Immersive Player Layout Heart & Spacing Alignment
- **Root Cause**: In the immersive player layout, the favorite heart icon and audio format tag lacked right alignment relative to the margins, and the heart icon had uneven vertical padding.
- **Resolution**:
  - Corrected vertical centering of the favorite heart button relative to the track title and artist rows.
  - Aligned the heart icon and right-side metadata with the right margin border.

### 3. Vinyl Record Sleeve Player Layout Overhaul & Bugfixes
- **Root Cause**: Switching to the Vinyl player layout suffered from incorrect deck alignment, broken tonearm geometry, and caused persistent UI rendering corruptions when switching back to other layouts, requiring a device reboot.
- **Resolution**:
  - Overhauled vinyl tonearm, spinning platter, and sleeve placement geometry for the TempoTec V1 display.
  - Fixed layout restoration so switching away from Vinyl cleanly resets viewport styles and widget bounds across all alternative layouts without needing a reboot.

### 4. Build Stability
- **Resolution**: Added forward declaration for `compact_place_sleeve` in `player.c` to resolve GCC static declaration errors during target cross-compilation.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
