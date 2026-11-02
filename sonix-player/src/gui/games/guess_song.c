// guess_song.c -- Minimalist "Guess the Song" audio quiz for Sonix Player.
#include "guess_song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lvgl/lvgl.h"
#include "src/gui/board_profile.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/games/games.h"
#include "src/gui/nowplaying/player.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/theme.h"
#include "src/system/core/config.h"
#include "src/system/core/lang.h"
#include "src/system/library/library.h"

#define SECTION "guess_song"
#define MAX_POOL 64
#define CHOICES 4
#define ROUND_TIME_MS 10000

lv_obj_t *guess_song_screen;

static game_frame_t frame;
static gui_config_t *guess_cfg;

static char track_pool[MAX_POOL][256];
static int pool_count = 0;

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
	player_pause_for_loading();
}

static void update_hint(void) {
	char buf[64];
	snprintf(buf, sizeof(buf), "Lives: %d • Score: %d", lives, frame.score);
	game_frame_set_hint(&frame, buf);
}

static void load_library_tracks(void) {
	pool_count = library_list(LIBRARY_LIST_TRACKS, track_pool, MAX_POOL);
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
	if (pool_count < CHOICES) {
		load_library_tracks();
		if (pool_count < CHOICES) {
			game_frame_show_overlay(&frame, "No Music Found",
				"Add more audio files to your SD card to play Guess the Song!", "Refresh", NULL);
			return;
		}
	}

	game_frame_hide_overlay(&frame);
	round_active = true;
	time_left_ms = ROUND_TIME_MS;
	lv_bar_set_value(progress_bar, ROUND_TIME_MS, LV_ANIM_OFF);

	// Pick 4 unique tracks from pool
	int picked[CHOICES];
	for (int i = 0; i < CHOICES; i++) {
		while (1) {
			int r = rand() % pool_count;
			bool dup = false;
			for (int j = 0; j < i; j++) {
				if (picked[j] == r) { dup = true; break; }
			}
			if (!dup) {
				picked[i] = r;
				break;
			}
		}
	}

	correct_choice = rand() % CHOICES;

	for (int i = 0; i < CHOICES; i++) {
		const char *path = track_pool[picked[i]];
		snprintf(choice_paths[i], sizeof(choice_paths[i]), "%s", path);

		char title[128] = {0};
		if (!library_track_title(path, title, sizeof(title)) || !title[0]) {
			// Fallback: extract filename without extension
			const char *slash = strrchr(path, '/');
			const char *fn = slash ? slash + 1 : path;
			snprintf(title, sizeof(title), "%s", fn);
			char *dot = strrchr(title, '.');
			if (dot) *dot = '\0';
		}

		snprintf(choice_titles[i], sizeof(choice_titles[i]), "%s", title);
		lv_label_set_text(choice_lbl[i], choice_titles[i]);

		lv_obj_set_style_bg_color(choice_btn[i], lv_color_hex(0x222226), 0);
		lv_obj_set_style_border_width(choice_btn[i], 1, 0);
		lv_obj_set_style_border_color(choice_btn[i], lv_color_hex(0x333338), 0);
	}

	update_hint();

	// Play snippet of correct song
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
