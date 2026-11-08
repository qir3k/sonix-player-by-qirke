# Sonix OS v1.1.36 for TempoTec Variations V1

## 🚨 Critical Boot & Display Hotfix (v1.1.36)

This release addresses the critical black-screen / non-booting failure reported on v1.1.35. All four failure domains have been systematically audited and resolved:

### 1. Root Filesystem Compression (Squashfs)
- **Root Cause**: v1.1.35 switched SquashFS compression to `-comp xz`, but the Ingenic X1600 Linux 4.4 vendor kernel does not have `CONFIG_SQUASHFS_XZ=y` compiled in. The kernel panicked immediately on boot, unable to mount rootfs.
- **Resolution**: Restored standard gzip compression (`-comp gzip -b 128K -noappend -all-root`) across all firmware packing pipelines.

### 2. Shared Library Dynamic Symbol Table Preservation
- **Root Cause**: Running `strip --strip-all` over `*.so*` libraries stripped `.dynsym`, corrupting dynamic linking runtime resolution.
- **Resolution**: Updated `Makefile` and `sonix_firmware_packer.sh` to strictly use `mipsel-linux-gnu-strip --strip-unneeded` for all shared objects (`*.so*`), restricting `--strip-all` exclusively to static/main executables (`sonix_player`, `sonix_launch`).

### 3. Hardware Backlight & Driver Initialization in rcS
- **Root Cause**: An overlaid `/etc/init.d/rcS` inadvertently overwrote the vendor startup script, preventing `mdev`, display panel kernel drivers, and PWM/GPIO backlight controls from initializing.
- **Resolution**: Deleted the overriding asset file. The clean vendor `rcS` script is preserved with complete hardware peripheral and display bring-up intact.

### 4. Non-Blocking Touch Evdev Drain & Direct Framebuffer Panning
- **Root Cause**: The input event drain loop could block indefinitely during early boot if the touch event queue had no data and lacked `O_NONBLOCK`. In addition, experimental partial render buffers conflicted with hardware panning.
- **Resolution**: 
  - Guaranteed non-blocking mode via `fcntl(fb_touch_fd, F_SETFL, flags | O_NONBLOCK)` with a 256-event safety bounding limit in `panel_touch_flush()`.
  - Fully restored rock-solid double-buffered direct framebuffer mode (`LV_DISPLAY_RENDER_MODE_DIRECT` on `fb_mem` and `fb_mem + fb_page_size`) with `FBIOPAN_DISPLAY` and clean 180° rotation page management.

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
