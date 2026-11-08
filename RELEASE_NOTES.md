# Sonix OS v1.1.41 for TempoTec Variations V1

## 🛠️ Critical Fixes & Improvements in v1.1.41

### 1. Streaming List Item Touch Responsiveness
- **Root Cause**: In `ytmusicpage.c`, `item_clicked_cb` checked `lv_obj_get_parent(lv_event_get_target(e)) == results_list`. When tapping in the middle of a row (on song title or subtitle labels), `lv_event_get_target(e)` returned the inner label or `texts` container rather than the row button. This caused the parent check to fail, routing the click to the album/tracks table where index mismatches either dropped the tap or played the wrong track. Kebab menu (3 dots) worked because it had an independent button callback.
- **Resolution**:
  - Updated `item_clicked_cb` to retrieve the attached row button via `lv_event_get_current_target(e)`.
  - Set `lv_obj_set_clickable(..., false)` on internal row labels and thumbnail containers across YouTube Music (`ytmusicpage.c`), Tidal (`tidalpage.c`), Qobuz (`qobuzpage.c`), Podcasts (`podcastpage.c`), and Internet Radio (`radiopage.c`), guaranteeing row click events fire reliably on every tap.

### 2. Player Sheet Swipe Right-to-Left on Scrollable Lists
- **Root Cause**: In LVGL 9, touching any scrollable container (such as track search result lists in Tidal, YouTube Music, or library views) immediately assigns `indev->scroll_obj`. In `sheet_drag_cb` (`player.c`) and `back_drag_cb` (`switcher.c`), an immediate `if (lv_indev_get_scroll_obj(indev)) { dragging = false; return; }` check was present, which aborted horizontal swipe gestures on the very first touch pixel of any list view.
- **Resolution**:
  - Removed premature `lv_indev_get_scroll_obj(indev)` abort from `sheet_drag_cb` and `back_drag_cb`.
  - Directional arbitration now compares displacement vectors: if vertical movement dominates (`LV_ABS(dy) >= DRAG_COMMIT_PX && LV_ABS(dy) > LV_ABS(dx)`), horizontal sheet dragging yields cleanly to vertical list scrolling.
  - When horizontal displacement dominates (`LV_ABS(dx) >= DRAG_COMMIT_PX && LV_ABS(dx) >= 2 * LV_ABS(dy)`), the horizontal swipe commits and smoothly pulls in the Now Playing sheet.

### 3. Spontaneous Playback Pauses & Jack Detection Debounce
- **Root Cause**: The status bar periodically polls `headphone_jack_state()`. If a reading returned `JACK_NONE`, `player_output_unplugged()` automatically paused playback. In `alsa-controls.c`, transient read errors on `/sys/class/switch/headset/state` (`state < 0`) were mistakenly treated as `JACK_NONE`. Combined with a short `JACK_SETTLE_MS` of only 120ms (shorter than the 250ms polling cycle), any brief socket contact bounce or physical plug wiggle immediately triggered a false unplug event, randomly pausing Tidal and local SD card playback.
- **Resolution**:
  - In `headphone_jack_state_raw()`, if `state < 0`, the current stable jack state is preserved rather than falling back to `JACK_NONE`.
  - Increased `JACK_SETTLE_MS` from 120ms to 600ms, effectively debouncing physical vibrations and mechanical contact bounce across multiple polling frames.

### 4. Background Bluetooth Daemons Teardown (~11MB RAM Saved)
- **Root Cause**: The stock rootfs included `/etc/init.d/S80_bt_init`, which `rcS` executed in the background on every boot regardless of the Bluetooth toggle setting. This started `brcm_patchram_plus`, `bluetoothd`, `bluealsa`, `bt-agent`, and an extra `dbus-daemon`, consuming ~11 MB of RAM even when Bluetooth and Wi-Fi were turned off.
- **Resolution**:
  - Removed `S80_bt_init` and `K80_bt_init` from rootfs squashfs packaging in `sonix_firmware_packer.sh`. Sonix Player manages Bluetooth completely on-demand when enabled by the user.
  - In `bluetooth.c`, `bluetooth_init()` now dispatches an explicit teardown job on boot if Bluetooth is disabled, ensuring any lingering processes are terminated.
  - Added `bt-agent` process monitoring to `bluetooth_power_down_stack()`.

### 5. ADB Daemon Teardown & USB Mass Storage
- **Root Cause**: `adb_apply_saved_state()` in `adb.c` only started ADB when enabled, but never killed running `adbd` / `adbserver.sh` processes when disabled. With `adbd` active, `usb.c` intentionally suppressed USB Mass Storage export to prevent device conflicts, leaving the device in charge-only mode when connected to a PC.
- **Resolution**:
  - `adb_apply_saved_state()` now calls `adb_apply(false)` when ADB is disabled in settings, terminating `adbd` and `adbserver.sh` and freeing ~2.9 MB RAM.
  - With ADB down, the dedicated USB storage gadget cleanly exports the MicroSD card to host PCs upon connection.

### 6. Smart Mix Large Library Support
- **Root Cause**: In `geniusbuild.c`, if a MicroSD card library contained more than 4096 tracks (`b->rows_seen > GENIUSBUILD_MAX_TRACKS`), the builder aborted the entire scan with `GENIUSBUILD_LIMIT` (-5), resulting in no `genius.db` being generated and Smart Mix always reporting "No Smart Mix data available". In addition, `ITEMS_TIDAL` in `trackmenu.c` mistakenly showed Smart Mix for streaming tracks that are not part of the local database.
- **Resolution**:
  - Capped the similarity graph at `GENIUSBUILD_MAX_TRACKS` (4096 tracks) without failing the build, allowing Smart Mix databases to build successfully on libraries of any size.
  - Bound SQLite query limit to `GENIUSBUILD_MAX_TRACKS` in `library.c`.
  - Removed `smartmix_start` from `ITEMS_TIDAL` in `trackmenu.c`.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
