# Sonix OS v1.1.38 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.1.38

### 1. Extended Quickpanel Status Bar & Toggle Grid Layout
- **Root Cause**: In `order_apply()` (`quickpanel.c`), button widgets were not hidden before slot assignment. When an extra or disabled button (such as Gain `| + |`) was not in the 8 active slots, it remained as an extra 9th child in `quick_row`. Because `quick_row` had flex cross alignment set to `LV_FLEX_ALIGN_CENTER`, the 3 resulting rows (140px total height) were vertically centered within `buttons_viewport` (92px height). This shifted Row 1 up by 24px directly underneath the topbar status bar and pushed Row 3 (Gain) down directly over the page indicator dots and brightness slider.
- **Resolution**:
  - `order_apply()` now systematically hides all `0..QP_BTN_COUNT-1` button widgets before placing assigned buttons.
  - Flex alignment for `quick_row` and `extra_row` set to `LV_FLEX_ALIGN_START` with corner clipping enabled (`lv_obj_set_style_clip_corner`), guaranteeing clean, bounded 2-row button layouts.

### 2. USB Mass Storage & PC Connection (MicroSD Card Access)
- **Root Cause**:
  - The Variations V1 uses an onboard TCS1421 Type-C CC controller (`/sys/devices/platform/tcs1421/tcs1421_cfg`). Previously, `typec_force_sink()` only attempted to write to `/sys/class/typec/port%d/port_type` (which does not exist on this board), leaving the TCS1421 in `NormalDRP` (dual-role mode). In this state, host computers failed to negotiate peripheral mode, resulting in charge-only behavior.
  - `storage_export()` called `audio_stop()` asynchronously; if an audio decoder or SQLite handle had not completely closed before the unmount check, `storage_export()` failed and permanently set `await_unplug = true`, locking the device into charge-only mode until the cable was physically disconnected.
  - Partition export: exporting `/dev/mmcblk0p1` rather than the parent block device `/dev/mmcblk0` prevented PC operating systems (especially Windows) from recognizing partition tables.
- **Resolution**:
  - `typec_force_sink()` now writes `"Sink\n"` to `/sys/devices/platform/tcs1421/tcs1421_cfg` when present.
  - `vbus_present()` scans all `/sys/class/power_supply/*/online` nodes dynamically.
  - Added safe audio release draining (up to 500ms), `sync()`, and `umount2(..., MNT_DETACH)` fallback during unmount.
  - Auto-resolves parent block device (e.g. `/dev/mmcblk0`) when exporting partitioned cards.
  - Removed permanent `await_unplug` lockouts on temporary export busy states.

---

## ℹ️ Firmware Compression & Size Note
- Working firmware images (v1.1.24–v1.1.30, v1.1.37, v1.1.38) are **~47.9 MB – 49 MB**.
- The non-booting build (v1.1.35) weighed ~36.9 MB solely due to `-comp xz` SquashFS compression. The Ingenic X1600 Linux 4.4 vendor kernel only supports LZO decompression (`CONFIG_SQUASHFS_LZO=y`). Using XZ causes an instant kernel panic on boot. The ~47.9MB size with LZO is the correct and verified bootable format.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
