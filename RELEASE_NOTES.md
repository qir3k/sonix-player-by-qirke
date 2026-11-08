# Sonix OS v1.1.37 for TempoTec Variations V1

## 🚨 Boot & Display Recovery Fix (v1.1.37)

This release fully restores verified boot and display behavior by aligning firmware packaging and compression with the known-good **v1.1.30** baseline:

### 1. SquashFS Compression Format (LZO Restored)
- **Root Cause**: Starting in v1.1.35, root filesystem compression was changed to `-comp xz` and subsequently `-comp gzip`. The Ingenic X1600 Linux 4.4 vendor kernel on TempoTec Variations V1 **only has `CONFIG_SQUASHFS_LZO=y` compiled in**. When formatted with XZ or GZIP, the kernel immediately panics at boot (*Unable to mount root fs on unknown-block*), leaving the device completely dark and unbootable.
- **Resolution**: Reverted `sonix_firmware_packer.sh` back to exact v1.1.30 SquashFS compression: `mksquashfs "$SQUASH_DIR" "$ROOTFS_NEW" -comp lzo -all-root`.

### 2. Vendor Shared Libraries Intact in Staging Rootfs
- **Root Cause**: Running `strip` across the entire unpacked `$SQUASH_DIR` staging rootfs stripped and altered precompiled vendor shared libraries from older Ingenic/MIPS toolchains, breaking runtime dynamic link loading.
- **Resolution**: Removed staging-wide strip operations from `sonix_firmware_packer.sh`, matching v1.1.30. Only `sonix_player` and `sonix_launch` are stripped by the Makefile during compilation.

### 3. Preserved Vendor Startup Script (/etc/init.d/rcS)
- **Root Cause**: An overlaid `/etc/init.d/rcS` was inadvertently present in `assets/V1`, overwriting the stock vendor script responsible for `mdev`, display controller modules (`insmod`), and backlight PWM/GPIO initialization.
- **Resolution**: The overriding `rcS` asset was deleted. The stock vendor initialization script is preserved untouched.

### 4. Direct Double-Buffered Framebuffer Panning & Non-Blocking Evdev Drain
- **Resolution**:
  - Restored zero-copy direct double-buffered hardware panning (`LV_DISPLAY_RENDER_MODE_DIRECT` on `fb_mem` and `fb_mem + fb_page_size`) with `FBIOPAN_DISPLAY` and clean 180° rotation page management.
  - Ensured `panel_touch_flush()` sets `O_NONBLOCK` via `fcntl` and safely bounds the read drain loop (`drain_count < 256`) to guarantee that UI start is never blocked.
  - Ensured all startup scripts in `/etc/init.d/` and `/usr/bin/` have strict Unix LF line endings.

---

## 🚀 Key Improvements & Architecture Updates

### 🎵 Atomic Audio State Machine
- Strict sequential transition pipeline: `[STOPPING]` ➔ `[CLEANUP]` ➔ `[BUFFERING]` ➔ `[PLAYING]`.
- Seamless switching between local tracks, YouTube, Tidal, and Web Radio without audio glitches, DAC pops, or ALSA underruns.
- Systematic `pthread_join`, socket teardown, and glibc `malloc_trim(0)` on every stream transition to eliminate heap fragmentation.

### 🧩 Addons & Plugin Ecosystem
- **Pocket Arcade & Games**: Integrated portable gaming runtime without bloating core playback loops.
- **Visual Effects & Layouts**: Vinyl turntable view and advanced transparency/tinting available as optional addons.
- **Waveform Seekbar**: Dynamically enabled via `layout_waveform` addon.

### ⚡ Optimized LVGL 9.6 Performance
- Fixed 60 FPS refresh pacing (`DISPLAY_FRAME_US = 16667us`) synchronized with panel vblank.
- Zero-copy direct video memory mapping in reserved video RAM, reducing system heap pressure by hundreds of kilobytes.
- Lightweight loading animations replacing heavy continuous vector redraws.

---

## 📦 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
