# Addons: what is in the player, and what can be left out

The player is one binary. What an addon switch changes is not the code that is linked, but whether that module's **pages are built at all**: a module that is off is never initialised, so the LVGL screens, lists, and rows it would have held are never allocated. Player layouts are the exception: they share one now-playing page, and their switches decide which layout choices and layout-specific resources are available. On a low-memory MIPS embedded DAP like the TempoTec V1 where free RAM is precious, this is the optimization that keeps everything fast and responsive.

### Fresh Install Defaults (RAM-First Philosophy)

Unlike earlier firmware where every single module was enabled by default, this fork introduces a **RAM-First, minimalist default** on clean installations:
* **Default ON:**
  - `subsonic` (Streaming)
  - `audiobooks` (Library)
  - `eq` (10-Band Graphic Equaliser)
  - `peq` (10-Band Parametric Equaliser)
  - `mseb` (MageSound 8-ball tuning)
  - `soundfield` (stereo width processing)
  - `crossfeed` (headphone crossfeed processing)
  - `channel_balance` (left/right channel correction)
  - `volume_limit` (per-output maximum volume ceiling)
  - `speex_resampler` (Bluetooth audio sample-rate conversion)
  - `layout_poster` (Poster now-playing layout)
* **Default OFF:**
  - All Games (`gearboy` when built with the Game Boy core, `flappy`, `game2048`, `minesweeper`, `sudoku`, `guess_song`, `stack_it`, `brick_breaker`, `pebble_deluxe`)
  - All Tools (`calculator`, `notes`, `tagmanager`, `metronome`, `ebook`)
  - Earbuds Bluetooth protocols (`airpods`, `earfun`)
  - Streaming providers requiring separate keys (`tidal`, `qobuz`, `ytmusic`, `podcasts`, `radio`)
  - Other player layouts and Cover Flow (`layout_standard`, `layout_waveform`, `layout_alternative`, `layout_immersive`, `layout_aam`, `coverflow`)

The exact free-memory figure depends on the device's current state and enabled features, so it is not fixed here. Existing user settings are preserved on update. A first-run welcome popup invites new users to open **Settings → Plugins** and choose which optional features to enable.

---

## Where the switches are

**Settings → Plugins**, a row for each module of this build with a one-line description and a switch, grouped under headings:
- **Streaming services**
- **Earbuds**
- **Bluetooth audio**
- **Library shelves**
- **Player layouts**
- **Music features**
- **Tools**
- **Games**

Poster is the default now-playing layout. Other layouts and Cover Flow can be
enabled here and require a restart; at least one player layout must remain
enabled. Waveform's buffer and worker are not allocated when that layout is off,
and the Cover Flow sheet is not built until its plugin is enabled.

Switching one writes `[addons] <id> = true|false` into `device_config.ini` and shows a *Restart to apply* toast: a page that has already been built cannot give its memory back mid-session, so the memory saving is real from the next clean start.

Below the modules, under **Interface**, sit the choices that are not modules — the favourite mark every page draws, the destination that takes the home screen's library tile while Audiobooks is off, and the Languages & Fonts page. They apply at once.

**Themes** has a heading of its own at the bottom of the page, with a switch for each palette (Dark, Light, Amber on Black, OLED). A theme switch applies immediately. The last active theme cannot be switched off.

---

## The modules list

| id | Section | Description |
|---|---|---|
| `tidal` | Streaming | Tidal pages, playlists, offline cache |
| `qobuz` | Streaming | Qobuz pages, playlists, offline cache |
| `ytmusic` | Streaming | YouTube Music search, charts, Explore, and downloads |
| `podcasts` | Streaming | Podcast search, subscriptions, saved episodes |
| `radio` | Streaming | Internet radio and the radio-browser directory |
| `subsonic` | Streaming | Subsonic / Navidrome / Gonic server streaming |
| `airpods` | Earbuds | Apple AirPods & Beats battery and ANC/Transparency control |
| `earfun` | Earbuds | EarFun earbuds over GAIA (battery, ANC, game mode) |
| `speex_resampler` | Bluetooth audio | Speex sample-rate conversion for Bluetooth audio |
| `audiobooks` | Library | Audiobooks section: books, series, authors, resume |
| `ebook` | Library | Books shelf and EPUB reader |
| `layout_standard` | Player layouts | Standard now-playing layout |
| `layout_waveform` | Player layouts | Waveform layout for local tracks |
| `layout_alternative` | Player layouts | Compact alternative layout |
| `layout_immersive` | Player layouts | Full-screen immersive artwork |
| `layout_poster` | Player layouts | Poster layout (enabled by default) |
| `layout_aam` | Player layouts | Album Art Max on TempoTec V1 |
| `coverflow` | Player layouts | Album carousel and its larger cover cache |
| `lyrics` | Music features | Synced lyrics pane and LRCLIB online lyrics search |
| `artwork` | Music features | Cover Art Fetcher: online sleeve search and ID3 writer |
| `eq` | Music features | 10-band graphic equaliser and tone controls |
| `peq` | Music features | 10-band parametric equaliser and AutoEq presets |
| `mseb` | Music features | HiBy MSEB acoustic tuning sliders |
| `soundfield` | Music features | Optional stereo-width processing and its Music settings page |
| `crossfeed` | Music features | Optional headphone crossfeed processing and its Music settings page |
| `channel_balance` | Music features | Optional left/right balance correction and its Music settings page |
| `volume_limit` | Music features | Optional per-output volume ceiling and its Music settings page |
| `sleep_timer` | Music features | Sleep timers on player pages and Control Center |
| `autoeq` | Music features | AutoEQ headphone correction database |
| `lastfm` | Music features | Scrobbling to Last.fm |
| `listenbrainz` | Music features | Scrobbling to ListenBrainz |
| `calculator` | Tools | Scientific touchscreen calculator |
| `notes` | Tools | Text note viewer and editor |
| `tagmanager` | Tools | On-device ID3/FLAC tag editor and cover art finder |
| `metronome` | Tools | Tactile tempo metronome |
| `gearboy` | Games | Gearboy Game Boy & Game Boy Color emulator |
| `flappy` | Games | Flappy Bird |
| `game2048` | Games | 2048 sliding number puzzle |
| `minesweeper` | Games | Minesweeper with tap/long-press and calibrated touch |
| `sudoku` | Games | Sudoku puzzle with calibrated grid touch |
| `guess_song` | Games | Guess The Song audio quiz with real music playback |
| `stack_it` | Games | Stack It timing block game |
| `brick_breaker` | Games | Brick Breaker arcade with continuous paddle touch drag |

Soundfield, Crossfeed, Channel Balance, Volume Limit, and Bluetooth Speex
resampling stay enabled as plugins by default to preserve existing behavior.
Disabling a plugin removes its settings row and disables its processing after
restart; saved settings are retained. With these plugins off, normal playback
continues: Soundfield and Crossfeed simply stop shaping stereo, Channel Balance
leaves the channels centered, and Bluetooth uses ALSA's default rate converter
instead of Speex when conversion is needed. Volume Limit is different only in
that its safety ceiling is removed, so leave it enabled if you rely on that
protection.

---

## How the gate is used

Two rules, both in the modules' own files rather than in the manager:

1. **The page is built only when the module is on.** `gui_init()` asks `addons_enabled("calculator")` before `calculator_init()`, and the streaming page asks before each `*page_init()`. A module that is off leaves its screen as the empty object `gui_init()` allocates.
2. **The row that opens it is not drawn.** `morepage.c` asks `more_addon_of(slug)` and skips the entry; `streaming.c` multiplies the arranged visibility by `addons_enabled(item->addon)`, so the tile is hidden even if it was arranged to the top.

Because both questions are asked, a disabled module is completely unreachable through the interface. Nothing calls `switch_screen()` with a page that was never built.
