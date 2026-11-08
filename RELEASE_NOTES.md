# Sonix OS v1.1.39 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.1.39

### 1. Restored Rotating Circle Spinner & Multilingual Layout
- **Root Cause**: In recent commits, `spinner.c` was altered to display hardcoded Polish text `"Ładowanie..."` via a timer instead of rotating the visual icon. Screens using the spinner (e.g. Podcasts, Qobuz, Tidal, YouTube Music, Wi-Fi dialogs) already have localized captions underneath (`t("loading")` in the user's selected language, such as English `"Loading ..."`). This resulted in ugly duplicated text (`"Ładowanie..."` over `"Loading ..."`) and stripped the spinning visual animation.
- **Resolution**: Reverted `spinner_create()` to pure LVGL image rotation animation (`spin_cb` with `lv_image_set_rotation`), completely removing hardcoded text and restoring the smooth rotating loading circle in all views while respecting the active system language.

### 2. Bluetooth Teardown & btmon Diagnostic Process Cleanup
- **Resolution**: Added `stop_process("btmon")` to `bring_stack_down()` and `bluetooth_power_down_stack()`, preventing the 1.2 MB `btmon` diagnostic logger from lingering when Bluetooth is turned off.

### 3. Extended Quickpanel Status Bar & Toggle Grid Layout (from v1.1.38)
- Pre-emptively hides all `0..QP_BTN_COUNT-1` button widgets in `order_apply()` before grid slot assignment, preventing rogue 9th buttons (e.g. Gain) from expanding into a 3rd row and shifting widgets into the top status bar.
- Set cross alignment to `LV_FLEX_ALIGN_START` and added corner clipping for pixel-perfect 2-row layout in the 92px buttons viewport.

### 4. USB Mass Storage & PC Connection (MicroSD Card Access) (from v1.1.38)
- Added hardware support for onboard TCS1421 Type-C CC controller (`/sys/devices/platform/tcs1421/tcs1421_cfg`), explicitly asserting `"Sink"` mode when connected to a host PC so the device correctly negotiates as a USB peripheral instead of idling in dual-role (DRP) charge-only mode.
- Dynamically poll all `/sys/class/power_supply/*/online` nodes.
- Added safe audio release draining (up to 500ms), `sync()`, and `umount2(..., MNT_DETACH)` fallback during unmount.
- Auto-resolves parent block device (e.g. `/dev/mmcblk0`) when exporting partitioned cards.
- Removed permanent `await_unplug` lockouts on temporary export busy states.

---

## ℹ️ Memory & System Architecture Clarification
- **Player RAM Footprint**: `sonix_player` uses only **~18.7 MB** of RAM on the Ingenic X1600 SoC.
- **System Daemons**: The remaining memory reported in the Processes page belongs to hardware and networking daemons:
  - Hardware Control: `sys_server` (~3.4 MB) handles volume knobs, power buttons, and hardware detection.
  - Bluetooth Audio Stack: `bluealsa` (~3.9 MB), `bt-agent` (~3.7 MB), `bluetoothd` (~3.0 MB) provide active A2DP audio when Bluetooth is turned on.
  - Wi-Fi Stack: `wpa_supplicant` (~2.4 MB) maintains the wireless connection when Wi-Fi is active.
  - USB Debugging: `adbd` / `adbserver.sh` (~2.9 MB) run only when Developer Options ADB is enabled.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
