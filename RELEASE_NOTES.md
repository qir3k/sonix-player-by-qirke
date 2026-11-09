# Sonix OS v1.2.9 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.2.9

### 1. SmartMix Creation Loading Spinner Cropping Fix
- **Root Cause**: In the SmartMix build dialog (`genius_begin`), `spinner_create(card, &icon_loader_big)` was followed by an explicit `lv_obj_set_size(spinner, bp_pick(36, 48), bp_pick(36, 48))`. The source bitmap for `icon_loader_big` is 56x56 px with a pivot at `(28, 28)`. In LVGL 9, resizing an image widget modifies only its layout bounding box without scaling the underlying bitmap image. On TempoTec Variations V1, constraining the widget bounds to 36x36 px caused the rotating 56x56 circular loader to extend outside its layout box, overlapping adjacent text and being clipped by the parent modal card during its rotation cycle.
- **Resolution**:
  - Removed the restrictive `lv_obj_set_size` call, allowing `spinner_create` to preserve the native 56x56 geometry and true central rotation pivot matching all other system dialogs and loading overlays (such as `toast` and streaming services).
  - Explicitly enabled `lv_obj_set_overflow_visible(card, true)` on the modal card container to ensure smooth, unclipped rendering during high-speed rotation.
  - The SmartMix loading indicator now spins smoothly and completely intact without any edge clipping or displacement.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
