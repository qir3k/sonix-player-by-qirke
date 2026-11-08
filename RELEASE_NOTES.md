# Sonix OS v1.1.40 for TempoTec Variations V1

## 🛠️ Critical Fixes in v1.1.40

### 🎵 Fixed Abrupt Volume Drop / ReplayGain Delay on Track Start
- **Root Cause**: When a track was queued or played in `device_state.c`, metadata was set to a blank placeholder (`metaloader_get_placeholder`), and `replaygain_load()` was called with no gain tags (`has_track_gain = false`). The audio decoder immediately started playback at 100% full volume (0 dB gain). Approximately one second later, the asynchronous metadata reader finished, and `device_state_get()` on the UI poll re-called `replaygain_load(&current_metadata)` with the true ReplayGain values (often -6 dB to -10 dB). This caused an abrupt, jarring drop in volume one second after playback began.
- **Resolution**:
  - Restored synchronous metadata reading (`metadata_read()`) and tag extraction prior to `audio_play()` in `device_state.c`.
  - ReplayGain is now computed and applied to `gain_q15` **before the very first audio frame is decoded or transmitted to the DAC**.
  - Removed `replaygain_load` re-invocation from `device_state_get()`, eliminating mid-stream volume jumps.

---

## 🛠️ Included Fixes from v1.1.39 & v1.1.38

### 1. Restored Rotating Circle Spinner & Multilingual Layout
- Reverted `spinner_create()` to pure LVGL image rotation animation (`spin_cb` with `lv_image_set_rotation`), removing hardcoded Polish text and restoring the smooth rotating loading circle in all views while respecting the active system language.

### 2. Extended Quickpanel Status Bar & Toggle Grid Layout
- Pre-emptively hides all `0..QP_BTN_COUNT-1` button widgets in `order_apply()` before grid slot assignment, preventing rogue 9th buttons (e.g. Gain) from expanding into a 3rd row and shifting widgets into the top status bar.
- Set cross alignment to `LV_FLEX_ALIGN_START` and added corner clipping for pixel-perfect 2-row layout in the 92px buttons viewport.

### 3. USB Mass Storage & PC Connection (MicroSD Card Access)
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
