# Changelog

All notable changes to this project are documented in this file.

---

## [v1.2.18] - 2026-11-10

### 🛠️ Fixed
- **Playback Stops on Loading Screens**: `player_pause_for_loading()` no longer pauses the music or stops a radio station when a streaming page's spinner or the shared busy card appears; it is kept as a no-op for its callers.
- **Favourites Leaking Between Services and the Card**: YouTube Music, Tidal and Qobuz songs saved to the card no longer land in the card's favourites. A star on one of them now goes to that service's own list — YouTube Music's on the player (by video id), Tidal's and Qobuz's on the account — from the service pages, Music, Search, playlists and the phone app alike.
- **Saved Songs Keep Their Id**: the video id is written into the tags kept beside a YouTube Music download and the Tidal/Qobuz track id into theirs, so a downloaded song can still be starred after its playback cache is swept.
- **Old Rows Cleaned Once**: favourites the music database holds for the three service folders are removed at boot; the files stay on the card and stay playable.

---

## [v1.1.46] - 2026-11-09

### 🛠️ Fixed & Improved
- **Compilation & CI Build**:
  - Fixed undeclared font reference in player.c by referencing standard UI fonts (font_ui_14 on V1, font_ui_16 on V2) with dynamic width layout calculation.
- **Alternative & Studio Layout Streaming Badges**:
  - Scaled down badges via lv_image_set_scale to proportional dimensions (26 px on V1, 36 px on V2).
  - Anchored badges cleanly to top-right corner of album sleeve with proper inset padding; badges dynamically slide with lyrics swipe.
- **YouTube Music Quality Bitrate**:
  - Implemented automatic calculation of bitrate from downloaded file size and length when completed, with fallback to nominal quality setting (128 kbps / 256 kbps AAC).
- **Immersive & Studio Layout Polishing**:
  - Immersive layout: raised track title, artist, and favorite heart icon higher above bottom sheet border.
  - Studio layout: enlarged favorite heart icon, applied accent color, enabled album art tinting on controls, and vertically centered audio quality pill.
  - Expanded hit-test area on top-right 3-dots button.
  - Wired vinyl record sleeve layout in Music Settings.
- **Smart Mix Dialog Styling**:
  - Removed white border on loading modal dialog card.
- **YouTube Music Podcast Playback**:
  - Fixed podcast mode detection and restored playback speed control.

---

## [v1.1.41] - 2026-11-09

### 🛠️ Fixed
- **Streaming List Item Touch Responsiveness**:
  - In ytmusicpage.c, evaluated lv_event_get_current_target(e) instead of the clicked inner widget (lv_event_get_target(e)), ensuring taps in the middle of a track row correctly identify the container and start playback.
  - Set lv_obj_set_clickable(..., false) on inner row containers and labels across ytmusicpage.c, 	idalpage.c, qobuzpage.c, podcastpage.c, and 
adiopage.c.
- **Player Sheet Swipe Right-to-Left on Scrollable Lists**:
  - Removed premature lv_indev_get_scroll_obj(indev) abort in sheet_drag_cb (player.c) and ack_drag_cb (switcher.c).
  - Gestures now dynamically commit to horizontal player sheet transitions when horizontal displacement dominates (LV_ABS(dx) >= DRAG_COMMIT_PX && LV_ABS(dx) >= 2 * LV_ABS(dy)), while yielding to vertical scrolling when vertical displacement dominates.
- **Playback Spontaneous Pausing & Jack Detection Debounce**:
  - In lsa-controls.c, increased JACK_SETTLE_MS from 120ms to 600ms to debounce mechanical socket micro-bounce.
  - In headphone_jack_state_raw(), if /sys/class/switch/headset/state returns < 0 (transient read error), the current stable state is retained instead of defaulting to JACK_NONE.
- **Bluetooth Stack Boot RAM Reduction (~11MB Saved)**:
  - Removed stock S80_bt_init from rootfs packaging in sonix_firmware_packer.sh.
  - Updated luetooth_init() in luetooth.c to dispatch an explicit teardown job on boot whenever Bluetooth is disabled.
  - Added t-agent check to luetooth_power_down_stack().
- **ADB Daemon Teardown & USB Mass Storage**:
  - Fixed db_apply_saved_state() in db.c to explicitly terminate running dbd / dbserver.sh processes and release the USB controller gadget when ADB is toggled off in developer settings or disabled on boot.
  - Fixes USB Mass Storage export to host PCs when ADB is off.
- **Smart Mix Large Library Support**:
  - In geniusbuild.c, libraries with > 4096 tracks now cap the indexed graph at GENIUSBUILD_MAX_TRACKS and return success rather than failing the build with GENIUSBUILD_LIMIT (-5).
  - Updated library.c SQLite bind to GENIUSBUILD_MAX_TRACKS.
  - Removed misleading smartmix_start action from ITEMS_TIDAL in 	rackmenu.c.

---

## [v1.1.40] - 2026-11-08

### 🛠️ Fixed
- **ReplayGain Playback Volume & Metadata Timing**:
  - Restored synchronous metadata tag loading before starting the audio decoder in `device_state.c`.
  - ReplayGain values are now accurately applied to the gain scale before frame 0 is decoded, eliminating the abrupt volume drop previously heard ~1 second into playback.

---

## [v1.1.39] - 2026-11-08

### 🛠️ Fixed
- **Rotating Circle Spinner & Multilingual Layout**:
  - Replaced broken text label `"Ładowanie..."` in `spinner.c` with the native rotating image animation (`spin_cb` with `lv_image_set_rotation`).
  - Removed duplicate text labels across dialogs and restored full respect for user-chosen UI languages (English, Polish, etc.).
- **Bluetooth Diagnostics Teardown**:
  - Added termination of lingering `btmon` processes to `bluetooth_power_down_stack()` and `bring_stack_down()`.

---

## [v1.1.38] - 2026-11-08

### 🛠️ Fixed
- **Quickpanel Extended Status Bar & Toggle Grid Layout**:
  - Fixed button grid layout where unplaced/extra buttons (such as Gain `| + |`) lingered in `quick_row` and overflowed onto a 3rd row, shifting rows into the topbar and colliding with page indicator dots and brightness slider.
  - Pre-emptively hide all buttons prior to grid slot assignment in `order_apply()`.
  - Switched `track_cross_place` flex alignment to `LV_FLEX_ALIGN_START` and added corner clipping for pixel-perfect 2-row layout in the 92px buttons viewport.
- **USB Mass Storage & PC Connection (MicroSD Card Access)**:
  - Added support for the onboard TCS1421 Type-C CC controller (`/sys/devices/platform/tcs1421/tcs1421_cfg`), asserting `"Sink"` mode when connected to a host PC so the device correctly negotiates as a USB peripheral instead of idling in dual-role (DRP) charge-only mode.
  - Dynamically poll all power supply nodes under `/sys/class/power_supply/*/online`.
  - Safe audio release draining (up to 500ms), `sync()`, and `umount2(..., MNT_DETACH)` fallback during unmount.
  - Automatically resolves parent block device (e.g. `/dev/mmcblk0`) when exporting partitioned cards.
  - Removed permanent `await_unplug` lockouts on temporary export busy states.

---

## [1.7.0] - 2026-11-08

Wydanie **1.7.0** to kompleksowa przebudowa architektury systemowej, niskopoziomowej obsługi sprzętu i podsystemu audio, zaprojektowana specjalnie pod kątem ograniczeń jednordzeniowego SoC **Ingenic X1600 (MIPS32r2)** i budżetu **55 MB RAM** odtwarzacza TempoTec Variations V1.

### 🚀 Core & System Performance
* **Odchudzenie pakietu firmware `v1.upt`:**
  * Wdrożono kompresję `mksquashfs -comp xz -b 256K -noappend` redukującą rozmiar paczki z ~50 MB do ~20 MB.
  * Automatyczne usuwanie zbędnych symboli debugowania (`mipsel-linux-gnu-strip --strip-all`) ze wszystkich bibliotek współdzielonych i pliku wykonywalnego `sonix_player`.
  * Usunięcie zbędnych plików, logów, archiwów i nieskompresowanych fontów z drzewa `rootfs staging`.
* **Zoptymalizowany podsystem pamięci ZRAM & Kernel:**
  * Wdrożono kompresję **LZ4** na urządzeniu `/dev/zram0` (`echo lz4 > /sys/block/zram0/comp_algorithm`) z rozmiarem dysku zram 20 MB.
  * Ustawiono agresywność wymiany pamięci na optymalny poziom: `vm.swappiness = 20`.
  * Przydzielenie priorytetu procesora dla głównego procesu: `nice -n -10 /usr/bin/sonix_player &` oraz gubernatora `performance`.
* **Czysty i szybki start systemu (`/etc/init.d/rcS`):**
  * Wyłączono niepotrzebny autostart usług sieciowych i demonów w tle (`wpa_supplicant`, `udhcpc`, `bluetoothd`, `avahi-daemon`, `syslogd/klogd`).
  * Usługi Wi-Fi i Bluetooth są uruchamiane ściśle na żądanie (on-demand), co eliminuje głód procesora i wycieki pamięci w fazie bootowania.
* **Strojenie sterty glibc:**
  * Skonfigurowano progi alokatora pamięci przed startem UI: `mallopt(M_MMAP_THRESHOLD, 64 * 1024)` oraz `mallopt(M_TRIM_THRESHOLD, 128 * 1024)`.
  * Cykliczne i zdarzeniowe wywołania `malloc_trim(0)` przy zamykaniu widoków, powrocie do menu głównego oraz zmianie utworów. Typowe zużycie RAM-u spadło do zaledwie **15–18 MB**.

### 📱 UI, Touch & Display Stability
* **Podwójne częściowe buforowanie LVGL 9.6 dopasowane do pamięci podręcznej L1:**
  * Zrezygnowano z alokowania pełnych buforów ramek na stercie (240x320x2 bajtów).
  * Skonfigurowano podwójne bufory częściowe o wysokości 32–40 linii w formacie **RGB565** z 32-bajtowym wyrównaniem, idealnie mieszczące się w pamięci podręcznej L1 procesora MIPS (16 KB/32 KB D-Cache).
  * Ograniczono pamięć podręczną dekodera obrazów do maksymalnie 2 klatek jednocześnie w RAM (`LV_IMAGE_CACHE_DEF_NUM 2`) z formatem docelowym RGB565.
* **Eliminacja ghost-clicków i lagów startowych:**
  * Wdrożono funkcję `flush_input_events(int fd)` w sterowniku wejściowym, czyszczącą zakolejkowane pakiety `struct input_event` po starcie jądra.
  * Wprowadzono 1200 ms okres karencji (`lv_indev_enable(touch_indev, false)`), aktywujący dotyk dopiero po pełnym ustabilizowaniu warstwy graficznej.
* **Uniwersalny szablon listy i wirtualizacja ekranów (`ui_generic_list_view`):**
  * Zastąpiono tworzenie nowych drzew widżetów stałą pulą 8 zrecyklingowanych wierszy dopasowanych do ekranu 240x320.
  * Przewijanie list o dowolnej długości wykonuje zero alokacji `malloc`.
* **Lekki wskaźnik ładowania:**
  * Zastąpiono wektorowy spinner 60 FPS minimalistyczną 3-kropkową animacją z wolnym timerem (250 ms), likwidując obciążenie procesora i odwiązując wskaźnik od pauzowania dźwięku.

### 🎧 Audio Engine & Concurrency
* **Czas rzeczywisty dla wątku odtwarzania:**
  * Nadano wątkowi PCM priorytet czasu rzeczywistego: `SCHED_FIFO` o priorytecie 80 (`pthread_setschedparam`).
  * Wdrożono buforowanie wyprzedzające (pre-roll) min. 300 ms danych przed startem strumienia ALSA.
* **Atomowa sekwencyjna maszyna stanów odtwarzacza:**
  * Ścisły przepływ `[STOPPING] -> [CLEANUP] -> [BUFFERING] -> [PLAYING]` przy przełączaniu między plikami lokalnymi, YouTube Music, Tidalem i radiem internetowym.
  * Natychmiastowe cyfrowe wyciszenie DAC (`alsa_track_mute()`), bezpieczne zamknięcie gniazd sieciowych i odciążenie sterty przed inicjalizacją nowego dekodera.
* **Asynchroniczny worker metadanych (`metaloader`):**
  * Odwiązano operacje wejścia/wyjścia na karcie microSD od wątku renderowania UI. Ekstrakcja okładek i tagów odbywa się w tle bez blokowania `lv_timer_handler()`.

### 🔄 Upstream Sync (Usprawnienia sprzętowe od MrFoxa)
* **Zoptymalizowane wybudzanie ekranu (`Reduce screen wake latency`):**
  * Natychmiastowe wybudzanie na zboczu wciśnięcia przycisku zasilania (key-down) bez oczekiwania na zwolnienie przycisku.
  * Przeniesienie powolnej inicjalizacji radia poza krytyczną ścieżkę pierwszej klatki.
* **Ciche wybudzanie podczas transmisji Bluetooth (`Panel hold`):**
  * Wyciszenie podświetlenia zamiast odłączania zasilania szkła panelu podczas odtwarzania A2DP — brak zakłóceń PMIC, brak zacięć w dźwięku i brak rozregulowania rejestrów CS43131.
* **Naprawa routingu USB DAC & redukcja szumów (hiss):**
  * Zapobieganie utracie ścieżki dźwiękowej `plughw:` po uśpieniu modułu Bluetooth w tle.
  * Domyślne włączenie sprzętowej kontroli głośności dla zewnętrznych przetworników USB DAC (21 dB mniej szumu kwantyzacji).
* **Ochrona przed wyciekami deskryptorów na karcie microSD:**
  * Użycie flagi `O_CLOEXEC` przy otwieraniu logów i deskryptorów, uniemożliwiające blokowanie unmountowania partycji FAT32/exFAT.

### 🧩 Nowe Moduły i Pluginy
* **Układ odtwarzacza Vinyl (`layout_vinyl`):**
  * Opcjonalny, estetyczny układ stylizowany na kopertę płyty winylowej, dodany jako przełączalny moduł w Plugins.
* **Efekty wizualne (`visual_effects`):**
  * Pasek stanu z efektem matowego szkła (frosted) oraz dynamicznym barwieniem okładką (album tinting) dostępny jako opcjonalny dodatek.

---

## [1.6.0] - 2026-11-04
* Dodanie pełnej obsługi wyszukiwarki i odtwarzacza YouTube Music.
* Udoskonalenie integracji z Podcast Index oraz stacjami Radio Browser.
* Usprawnienia gestów bocznych i animacji Control Center.
