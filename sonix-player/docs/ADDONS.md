# Addons: what is in the player, and what can be left out

The player is one binary. What an addon switch changes is not the code that is linked, but whether that module's **pages are built at all**: a module that is off is never initialised, so the LVGL screens, lists, and rows it would have held are never allocated. On a low-memory MIPS embedded DAP like the TempoTec V1 where free RAM is precious, this is the optimization that keeps everything fast and responsive.

### Fresh Install Defaults (RAM-First Philosophy)

Unlike earlier firmware where every single module was enabled by default, this fork introduces a **RAM-First, minimalist default** on clean installations:
* **Default ON:**
  - `subsonic` (Streaming)
  - `audiobooks` (Library)
  - `eq` (10-Band Graphic Equaliser)
  - `peq` (10-Band Parametric Equaliser)
  - `mseb` (MageSound 8-ball tuning)
* **Default OFF:**
  - All Games (`gearboy`, `flappy`, `game2048`, `minesweeper`, `sudoku`, `guess_song`, `stack_it`, `brick_breaker`)
  - All Tools (`calculator`, `notes`, `tagmanager`, `metronome`, `ebook`)
  - Earbuds Bluetooth protocols (`airpods`, `earfun`)
  - Streaming providers requiring separate keys (`tidal`, `qobuz`, `ytmusic`, `podcasts`, `radio`)

This ensures that upon first boot, the player launches with ~45–50 MB of free RAM. Existing user settings are never overwritten upon update. A friendly first-run welcome popup invites new users to open **Settings → Plugins** and toggle whichever features they desire.

---

## Where the switches are

**Settings → Plugins**, a row for each module of this build with a one-line description and a switch, grouped under headings:
- **Streaming services**
- **Earbuds**
- **Library shelves**
- **Music features**
- **Tools**
- **Games**

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
| `audiobooks` | Library | Audiobooks section: books, series, authors, resume |
| `ebook` | Library | Books shelf and EPUB reader |
| `lyrics` | Music features | Synced lyrics pane and LRCLIB online lyrics search |
| `artwork` | Music features | Cover Art Fetcher: online sleeve search and ID3 writer |
| `eq` | Music features | 10-band graphic equaliser and tone controls |
| `peq` | Music features | 10-band parametric equaliser and AutoEq presets |
| `mseb` | Music features | HiBy MSEB acoustic tuning sliders |
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

---

## How the gate is used

Two rules, both in the modules' own files rather than in the manager:

1. **The page is built only when the module is on.** `gui_init()` asks `addons_enabled("calculator")` before `calculator_init()`, and the streaming page asks before each `*page_init()`. A module that is off leaves its screen as the empty object `gui_init()` allocates.
2. **The row that opens it is not drawn.** `morepage.c` asks `more_addon_of(slug)` and skips the entry; `streaming.c` multiplies the arranged visibility by `addons_enabled(item->addon)`, so the tile is hidden even if it was arranged to the top.

Because both questions are asked, a disabled module is completely unreachable through the interface. Nothing calls `switch_screen()` with a page that was never built.
