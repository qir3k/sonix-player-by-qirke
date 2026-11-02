// guess_song.c -- Minimalist "Guess the Song" audio quiz for Sonix Player.
#include "guess_song.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "src/gui/board_profile.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/games/games.h"
#include "src/gui/nowplaying/player.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/theme.h"
#include "src/system/audio/audio.h"
#include "src/system/core/config.h"
#include "src/system/core/lang.h"
#include "src/system/device/system.h"
#include "src/system/library/library.h"

#define SECTION "guess_song"
#define MAX_POOL 512
#define CHOICES 4
#define ROUND_TIME_MS 10000
#define PLAYED_HISTORY_MAX 512

typedef struct {
	char title[128];
	char path[256];
	char artist[128];
} song_entry_t;

lv_obj_t *guess_song_screen;

static game_frame_t frame;
static gui_config_t *guess_cfg;

static library_index_t *lib_ix = NULL;
static song_entry_t track_pool[MAX_POOL];
static int pool_count = 0;

static char played_history[PLAYED_HISTORY_MAX][256];
static int played_history_count = 0;

static int correct_choice = -1;
static char choice_titles[CHOICES][128];
static char choice_paths[CHOICES][256];
static lv_obj_t *choice_btn[CHOICES];
static lv_obj_t *choice_lbl[CHOICES];
static lv_obj_t *progress_bar;

static int lives = 3;
static int rounds_won = 0;
static int time_left_ms = ROUND_TIME_MS;
static bool round_active = false;
static lv_timer_t *round_timer = NULL;
static lv_timer_t *next_timer = NULL;

static void stop_audio(void) {
	audio_pause();
}

static void update_hint(void) {
	char buf[64];
	snprintf(buf, sizeof(buf), "Lives: %d • Score: %d", lives, frame.score);
	game_frame_set_hint(&frame, buf);
}

static bool collect_track_cb(const char *name, const char *path, const char *artist, void *user) {
	(void)user;
	if (!path || !path[0] || pool_count >= MAX_POOL) {
		return false;
	}
	if (access(path, R_OK) != 0) {
		return true;
	}
	snprintf(track_pool[pool_count].path, sizeof(track_pool[pool_count].path), "%s", path);
	snprintf(track_pool[pool_count].title, sizeof(track_pool[pool_count].title), "%s", (name && name[0]) ? name : "Unknown");
	snprintf(track_pool[pool_count].artist, sizeof(track_pool[pool_count].artist), "%s", (artist && artist[0]) ? artist : "");
	pool_count++;
	return true;
}

static bool is_audio_file(const char *name) {
	const char *ext = strrchr(name, '.');
	if (!ext) return false;
	return (strcasecmp(ext, ".flac") == 0 || strcasecmp(ext, ".mp3") == 0 ||
			strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".m4a") == 0 ||
			strcasecmp(ext, ".ogg") == 0 || strcasecmp(ext, ".ape") == 0 ||
			strcasecmp(ext, ".dsf") == 0 || strcasecmp(ext, ".dff") == 0 ||
			strcasecmp(ext, ".opus") == 0);
}

static void scan_dir_recursive(const char *dir_path, int depth) {
	if (depth > 4 || pool_count >= MAX_POOL) return;
	DIR *d = opendir(dir_path);
	if (!d) return;
	struct dirent *de;
	while ((de = readdir(d)) != NULL && pool_count < MAX_POOL) {
		if (de->d_name[0] == '.') continue;
		char full[300];
		snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);
		struct stat st;
		if (stat(full, &st) == 0) {
			if (S_ISDIR(st.st_mode)) {
				scan_dir_recursive(full, depth + 1);
			} else if (S_ISREG(st.st_mode) && is_audio_file(de->d_name)) {
				snprintf(track_pool[pool_count].path, sizeof(track_pool[pool_count].path), "%s", full);
				char title[128];
				snprintf(title, sizeof(title), "%s", de->d_name);
				char *dot = strrchr(title, '.');
				if (dot) *dot = '\0';
				snprintf(track_pool[pool_count].title, sizeof(track_pool[pool_count].title), "%s", title);
				track_pool[pool_count].artist[0] = '\0';
				pool_count++;
			}
		}
	}
	closedir(d);
}

static void load_library_tracks(void) {
	pool_count = 0;
	library_for_each(LIBRARY_LIST_TRACKS, LIBRARY_FILTER_NONE, NULL, collect_track_cb, NULL);
	if (pool_count < CHOICES) {
		const char *root = storage_sd_root();
		if (root && root[0]) {
			char music_dir[256];
			snprintf(music_dir, sizeof(music_dir), "%s/Music", root);
			scan_dir_recursive(music_dir, 0);
			if (pool_count < CHOICES) {
				scan_dir_recursive(root, 0);
			}
		}
	}
	printf("guess_song: loaded %d playable tracks\n", pool_count);
}

static bool is_in_history(const char *track_path) {
	if (!track_path || !track_path[0]) return false;
	for (int i = 0; i < played_history_count; i++) {
		if (strcmp(played_history[i], track_path) == 0) {
			return true;
		}
	}
	return false;
}

static void add_to_history(const char *track_path) {
	if (!track_path || !track_path[0]) return;
	if (played_history_count < PLAYED_HISTORY_MAX) {
		snprintf(played_history[played_history_count++], sizeof(played_history[0]), "%s", track_path);
	} else {
		memmove(played_history[0], played_history[1], sizeof(played_history[0]) * (PLAYED_HISTORY_MAX - 1));
		snprintf(played_history[PLAYED_HISTORY_MAX - 1], sizeof(played_history[0]), "%s", track_path);
	}
}

static bool collect_single_track_cb(const char *name, const char *path, const char *artist, void *user) {
	song_entry_t *out = user;
	if (path && path[0]) {
		snprintf(out->path, sizeof(out->path), "%s", path);
		snprintf(out->title, sizeof(out->title), "%s", (name && name[0]) ? name : "Unknown");
		snprintf(out->artist, sizeof(out->artist), "%s", (artist && artist[0]) ? artist : "");
	}
	return false;
}

static int get_total_track_count(void) {
	if (lib_ix && !library_index_stale(lib_ix)) {
		return library_index_count(lib_ix);
	}
	return pool_count;
}

static bool get_track_at_index(int idx, song_entry_t *out) {
	if (!out) return false;
	out->path[0] = '\0';
	out->title[0] = '\0';
	out->artist[0] = '\0';
	if (lib_ix && !library_index_stale(lib_ix)) {
		library_index_window(lib_ix, idx, 1, collect_single_track_cb, out);
		return out->path[0] != '\0';
	}
	if (idx >= 0 && idx < pool_count) {
		*out = track_pool[idx];
		return true;
	}
	return false;
}

static void ensure_tracks_loaded(void) {
	if (lib_ix && library_index_stale(lib_ix)) {
		library_index_close(lib_ix);
		lib_ix = NULL;
	}
	if (!lib_ix) {
		lib_ix = library_index_open(LIBRARY_LIST_TRACKS, LIBRARY_FILTER_NONE, NULL, LIBRARY_ORDER_DEFAULT, false);
	}
	if (lib_ix && library_index_count(lib_ix) >= CHOICES) {
		return;
	}
	if (pool_count < CHOICES) {
		load_library_tracks();
	}
}

static void start_round(void);

static void next_round_cb(lv_timer_t *t) {
	(void)t;
	next_timer = NULL;
	if (lives > 0) {
		start_round();
	}
}

static void round_timer_cb(lv_timer_t *t) {
	(void)t;
	if (!round_active) return;

	time_left_ms -= 100;
	if (time_left_ms < 0) time_left_ms = 0;

	lv_bar_set_value(progress_bar, time_left_ms, LV_ANIM_OFF);

	if (time_left_ms <= 0) {
		round_active = false;
		stop_audio();
		lives--;
		update_hint();

		// Highlight correct choice
		if (correct_choice >= 0 && correct_choice < CHOICES) {
			lv_obj_set_style_bg_color(choice_btn[correct_choice], lv_color_hex(0x2d8a4e), 0);
		}

		if (lives <= 0) {
			game_frame_show_overlay(&frame, "Game Over", "Out of time! Better luck next round.", "Play Again", NULL);
		} else {
			next_timer = lv_timer_create(next_round_cb, 1200, NULL);
			lv_timer_set_repeat_count(next_timer, 1);
		}
	}
}

static void start_round(void) {
	ensure_tracks_loaded();
	int total = get_total_track_count();
	if (total < CHOICES) {
		game_frame_show_overlay(&frame, "No Music Found",
			"Add more audio files to your SD card to play Guess the Song!", "Refresh", NULL);
		return;
	}

	game_frame_hide_overlay(&frame);
	round_active = true;
	time_left_ms = ROUND_TIME_MS;
	lv_bar_set_value(progress_bar, ROUND_TIME_MS, LV_ANIM_OFF);

	// Reset history if we have played almost all tracks
	if (played_history_count >= total - 3) {
		played_history_count = 0;
	}

	// Pick the correct song first, avoiding recent songs in history
	song_entry_t correct_song = {0};
	int attempts = 0;
	while (attempts++ < 120) {
		int r = rand() % total;
		if (get_track_at_index(r, &correct_song) && correct_song.path[0]) {
			if (access(correct_song.path, R_OK) == 0 && (!is_in_history(correct_song.path) || attempts > 100)) {
				break;
			}
		}
	}
	if (!correct_song.path[0]) {
		get_track_at_index(rand() % total, &correct_song);
	}
	add_to_history(correct_song.path);

	correct_choice = rand() % CHOICES;
	snprintf(choice_paths[correct_choice], sizeof(choice_paths[correct_choice]), "%s", correct_song.path);
	snprintf(choice_titles[correct_choice], sizeof(choice_titles[correct_choice]), "%s", correct_song.title);

	// Pick 3 unique decoy songs
	for (int i = 0; i < CHOICES; i++) {
		if (i == correct_choice) continue;

		song_entry_t decoy = {0};
		int decoy_attempts = 0;
		while (decoy_attempts++ < 120) {
			int r = rand() % total;
			if (get_track_at_index(r, &decoy) && decoy.path[0]) {
				bool dup = (strcmp(decoy.path, correct_song.path) == 0 ||
				            strcasecmp(decoy.title, correct_song.title) == 0);
				for (int j = 0; j < i; j++) {
					if (j == correct_choice) continue;
					if (strcmp(choice_paths[j], decoy.path) == 0 ||
					    strcasecmp(choice_titles[j], decoy.title) == 0) {
						dup = true;
						break;
					}
				}
				if (!dup) break;
			}
		}
		snprintf(choice_paths[i], sizeof(choice_paths[i]), "%s", decoy.path);
		snprintf(choice_titles[i], sizeof(choice_titles[i]), "%s", decoy.title);
	}

	for (int i = 0; i < CHOICES; i++) {
		lv_label_set_text(choice_lbl[i], choice_titles[i]);
		lv_obj_set_style_bg_color(choice_btn[i], lv_color_hex(0x222226), 0);
		lv_obj_set_style_border_width(choice_btn[i], 1, 0);
		lv_obj_set_style_border_color(choice_btn[i], lv_color_hex(0x333338), 0);
	}

	update_hint();

	// Play snippet of correct song
	printf("guess_song: playing '%s'\n", choice_paths[correct_choice]);
	player_play_file(choice_paths[correct_choice]);

	if (round_timer) {
		lv_timer_reset(round_timer);
		lv_timer_resume(round_timer);
	}
}

static void choice_clicked_cb(lv_event_t *e) {
	if (!round_active) return;
	uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
	if (idx >= CHOICES) return;

	round_active = false;
	stop_audio();
	if (round_timer) lv_timer_pause(round_timer);

	if ((int)idx == correct_choice) {
		// Correct!
		lv_obj_set_style_bg_color(choice_btn[idx], lv_color_hex(0x2d8a4e), 0);
		int bonus = 100 + (time_left_ms / 100);
		game_frame_add_score(&frame, bonus);
		rounds_won++;
		update_hint();

		next_timer = lv_timer_create(next_round_cb, 1000, NULL);
		lv_timer_set_repeat_count(next_timer, 1);
	} else {
		// Wrong!
		lv_obj_set_style_bg_color(choice_btn[idx], lv_color_hex(0x8a2d2d), 0);
		if (correct_choice >= 0 && correct_choice < CHOICES) {
			lv_obj_set_style_bg_color(choice_btn[correct_choice], lv_color_hex(0x2d8a4e), 0);
		}
		lives--;
		update_hint();

		if (lives <= 0) {
			char score_msg[64];
			snprintf(score_msg, sizeof(score_msg), "Score: %d • Rounds: %d", frame.score, rounds_won);
			game_frame_show_overlay(&frame, "Game Over", score_msg, "Play Again", NULL);
		} else {
			next_timer = lv_timer_create(next_round_cb, 1200, NULL);
			lv_timer_set_repeat_count(next_timer, 1);
		}
	}
}

static void new_game(void) {
	lives = 3;
	rounds_won = 0;
	game_frame_set_score(&frame, 0);
	start_round();
}

static void show_cb(void) {
	if (!round_timer) {
		round_timer = lv_timer_create(round_timer_cb, 100, NULL);
	}
	new_game();
}

static void hide_cb(void) {
	round_active = false;
	stop_audio();
	if (round_timer) {
		lv_timer_pause(round_timer);
	}
	if (next_timer) {
		lv_timer_delete(next_timer);
		next_timer = NULL;
	}
	if (lib_ix) {
		library_index_close(lib_ix);
		lib_ix = NULL;
	}
}

static void guess_song_build(gui_config_t *cfg) {
	static const game_hooks_t hooks = {
		.on_show = show_cb,
		.on_hide = hide_cb,
	};

	frame.hooks = &hooks;
	frame.section = SECTION;
	frame.title = tr("guess_song");
	guess_song_screen = lv_obj_create(NULL);
	frame.screen = guess_song_screen;
	frame.on_overlay_primary = new_game;
	game_frame_build(&frame, cfg);

	// Progress bar at top of field
	progress_bar = lv_bar_create(frame.field);
	lv_obj_set_size(progress_bar, 216, 6);
	lv_obj_align(progress_bar, LV_ALIGN_TOP_MID, 0, 4);
	lv_bar_set_range(progress_bar, 0, ROUND_TIME_MS);
	lv_bar_set_value(progress_bar, ROUND_TIME_MS, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x28282c), LV_PART_MAIN);
	lv_obj_set_style_bg_color(progress_bar, theme()->accent, LV_PART_INDICATOR);
	lv_obj_set_style_radius(progress_bar, 3, 0);

	// 4 choices
	int start_y = 20;
	int card_h = 40;
	int card_gap = 8;

	for (int i = 0; i < CHOICES; i++) {
		choice_btn[i] = lv_btn_create(frame.field);
		lv_obj_set_size(choice_btn[i], 216, card_h);
		lv_obj_align(choice_btn[i], LV_ALIGN_TOP_MID, 0, start_y + i * (card_h + card_gap));
		lv_obj_set_style_radius(choice_btn[i], 8, 0);
		lv_obj_set_style_bg_opa(choice_btn[i], LV_OPA_COVER, 0);
		lv_obj_set_style_shadow_width(choice_btn[i], 0, 0);
		lv_obj_set_style_pad_hor(choice_btn[i], 8, 0);
		lv_obj_set_style_pad_ver(choice_btn[i], 0, 0);

		choice_lbl[i] = lv_label_create(choice_btn[i]);
		lv_obj_set_style_text_font(choice_lbl[i], &font_ui_14, 0);
		lv_label_set_long_mode(choice_lbl[i], LV_LABEL_LONG_DOT);
		lv_obj_set_width(choice_lbl[i], 200);
		lv_obj_align(choice_lbl[i], LV_ALIGN_LEFT_MID, 0, 0);

		lv_obj_add_event_cb(choice_btn[i], choice_clicked_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
	}

	lv_obj_move_foreground(frame.overlay);

	game_frame_add_header_button(&frame, &icon_game_restart, new_game);
}

void guess_song_init(gui_config_t *cfg) {
	guess_cfg = cfg;
}

void guess_song_open(void) {
	if (!guess_song_screen) {
		guess_song_build(guess_cfg);
	}
	srand((unsigned)time(NULL));
	game_frame_open(&frame);
}
