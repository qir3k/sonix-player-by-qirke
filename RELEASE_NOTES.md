# Sonix OS v1.2.18 for TempoTec Variations V1

## 🛠️ Fixes in v1.2.18

### 1. Music no longer stops when a loading screen appears
- **Root Cause**: `player_pause_for_loading()` paused whatever was playing and stopped an active radio station. Every streaming page's spinner and the shared busy card (cover art, lyrics, artwork search, Wi-Fi work) called it, so a search or a spinner silenced the music that was already playing.
- **Resolution**:
  - The function is now a no-op, kept only so the pages that call it keep compiling; `show_busy()` no longer calls it at all.
  - Starting a new stream still stops the previous one, where it belongs: in the page that starts it.

### 2. Each service keeps its own favourites again
- **Root Cause**: YouTube Music, Tidal and Qobuz songs saved to the card live in `Music/YouTube Music`, `Music/Tidal` and `Music/Qobuz`, which the music library indexes. Starring one of them from a list — the service's own *Downloaded* page, Music, Search, a playlist, or the phone app — wrote the path into the card's favourites table. YouTube Music hearts therefore turned up under **Music → Favourites**.
- **Resolution**:
  - Starring a track is now decided by where the song lives: a YouTube Music song is liked in the player's own YouTube Music list (by video id), a Tidal or Qobuz song on its account, and only the card's own music reaches the card's favourites.
  - The video id is written into the tags kept beside a YouTube Music download, and the Tidal/Qobuz track id into the tags kept beside theirs, so a saved song can still be starred after its playback cache has been swept.
  - Favourites rows that older builds wrote for songs in those three folders are removed once at boot; the files themselves are untouched, and **Music → Favourites** is now the card's music only.
  - Bulk "add to favourites" leaves service songs out, and the phone app's star follows the same rule.

---

## 💾 Flashing Instructions
1. Download `v1.upt` and verify its MD5 against `v1_md5.txt`.
2. Copy `v1.upt` to the root directory of a FAT32/exFAT formatted MicroSD card.
3. Insert the card into your TempoTec Variations V1 and select **System Update** from the recovery or settings menu.
