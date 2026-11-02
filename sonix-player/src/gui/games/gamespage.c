#include "gamespage.h"

#include <string.h>

#include "lvgl/lvgl.h"

#include "src/gui/board_profile.h"
#include "src/gui/flappybird/flappybird.h"
#include "src/gui/games/game2048.h"
#include "src/gui/games/minesweeper.h"
#include "src/gui/games/sudoku.h"
#include "src/gui/games/guess_song.h"
#include "src/gui/games/stack_it.h"
#include "src/gui/games/brick_breaker.h"
#include "src/gui/games/pebble_deluxe.h"
#if GB_CORE
#include "src/gui/gearboy/gearboypage.h"
#endif
#include "src/gui/shell/gridpage.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "src/system/addons/addons.h"

lv_obj_t *gamespage_screen;

static lv_obj_t *games_grid;
static lv_obj_t *games_list;
static gui_config_t *games_cfg;

static void game_action_cb(lv_event_t *e) {
	void (*action)(void) = (void (*)(void))lv_event_get_user_data(e);
	if (action) {
		action();
	}
}

static bool games_tokyo_icon(const char *label, const lv_image_dsc_t **icon, theme_semantic_t *tone) {
	if (!label || !icon || !tone) {
		return false;
	}
	if (strcmp(label, "gearboy") == 0) {
		*icon = &icon_files_game;
		*tone = THEME_SEMANTIC_GREEN;
	} else if (strcmp(label, "flappy_bird") == 0) {
		*icon = &icon_bird;
		*tone = THEME_SEMANTIC_PURPLE;
	} else if (strcmp(label, "2048") == 0) {
		*icon = &icon_game_2048;
		*tone = THEME_SEMANTIC_BLUE;
	} else if (strcmp(label, "minesweeper") == 0) {
		*icon = &icon_game_mines;
		*tone = THEME_SEMANTIC_RED;
	} else if (strcmp(label, "sudoku") == 0) {
		*icon = &icon_game_sudoku;
		*tone = THEME_SEMANTIC_CYAN;
	} else if (strcmp(label, "guess_song") == 0) {
		*icon = &icon_game_guess_song;
		*tone = THEME_SEMANTIC_PURPLE;
	} else if (strcmp(label, "stack_it") == 0) {
		*icon = &icon_game_stack;
		*tone = THEME_SEMANTIC_AMBER;
	} else if (strcmp(label, "brick_breaker") == 0) {
		*icon = &icon_game_bricks;
		*tone = THEME_SEMANTIC_GREEN;
	} else if (strcmp(label, "pebble_deluxe") == 0) {
		*icon = &icon_game_pebble;
		*tone = THEME_SEMANTIC_AMBER;
	} else {
		return false;
	}
	return true;
}

static void games_list_add_entry(const grid_entry_t *entry) {
	lv_event_cb_t callback = entry->target ? switch_screen_cb : game_action_cb;
	void *user_data = entry->target ? (void *)*entry->target : (void *)entry->action;
	lv_obj_t *row = settingsrow_add(games_list, entry->label, NULL, callback, user_data);
	const lv_image_dsc_t *icon = NULL;
	theme_semantic_t tone;
	if (games_tokyo_icon(entry->label, &icon, &tone)) {
		settingsrow_add_icon(row, icon, tone);
	}
}

static const grid_entry_t ALL_GAMES[] = {
#if GB_CORE
	{"gearboy", &icon_menu_gearboy, &gearboypage_screen, NULL},
#endif
	{"flappy_bird", &icon_menu_flappy_bird, NULL, flappybird_open},
	{"2048", &icon_game_2048, NULL, game2048_open},
	{"minesweeper", &icon_game_mines, NULL, minesweeper_open},
	{"sudoku", &icon_game_sudoku, NULL, sudoku_open},
	{"guess_song", &icon_game_guess_song, NULL, guess_song_open},
	{"stack_it", &icon_game_stack, NULL, stack_it_open},
	{"brick_breaker", &icon_game_bricks, NULL, brick_breaker_open},
	{"pebble_deluxe", &icon_game_pebble, NULL, pebble_deluxe_open},
};

#define GAMES_MAX 24

static int games_entries_build(grid_entry_t *out, int max) {
	int count = 0;
	for (size_t i = 0; i < sizeof(ALL_GAMES) / sizeof(ALL_GAMES[0]) && count < max; i++) {
		const char *label = ALL_GAMES[i].label;
		bool enabled = false;
		if (strcmp(label, "gearboy") == 0) {
			enabled = addons_enabled("gearboy");
		} else if (strcmp(label, "flappy_bird") == 0) {
			enabled = addons_enabled("flappy");
		} else if (strcmp(label, "2048") == 0) {
			enabled = addons_enabled("game2048");
		} else if (strcmp(label, "minesweeper") == 0) {
			enabled = addons_enabled("minesweeper");
		} else if (strcmp(label, "sudoku") == 0) {
			enabled = addons_enabled("sudoku");
		} else if (strcmp(label, "guess_song") == 0) {
			enabled = addons_enabled("guess_song");
		} else if (strcmp(label, "stack_it") == 0) {
			enabled = addons_enabled("stack_it");
		} else if (strcmp(label, "brick_breaker") == 0) {
			enabled = addons_enabled("brick_breaker");
		} else if (strcmp(label, "pebble_deluxe") == 0) {
			enabled = addons_enabled("pebble_deluxe");
		}
		if (enabled) {
			out[count++] = ALL_GAMES[i];
		}
	}
	return count;
}

void games_screen_build(gui_config_t *cfg) {
	games_cfg = cfg;
	gamespage_screen = lv_obj_create(NULL);
	lv_obj_add_style(gamespage_screen, &theme_style_screen, 0);

	grid_entry_t entries[GAMES_MAX];
	int count = games_entries_build(entries, GAMES_MAX);

	games_grid = gridpage_build(gamespage_screen, cfg, entries, count, 2, LV_MAX((count + 1) / 2, 1), true);
	games_list = settingsrow_page(gamespage_screen, cfg, "games");

	for (int i = 0; i < count; i++) {
		games_list_add_entry(&entries[i]);
	}
}

static void games_refresh_theme(void) {
	bool tokyo_v1 = bp_is_tempotec_v1() && theme_uses_function_glyphs();
	if (games_grid) {
		lv_obj_set_hidden(games_grid, tokyo_v1);
	}
	if (games_list) {
		lv_obj_set_hidden(games_list, !tokyo_v1);
	}
}

static void games_build(gui_config_t *cfg) {
	games_screen_build(cfg);
	theme_register_refresh(games_refresh_theme);
	games_refresh_theme();
}

void gamespage_refresh_layout(void) {
	if (!games_cfg) {
		return;
	}
	grid_entry_t entries[GAMES_MAX];
	int count = games_entries_build(entries, GAMES_MAX);

	if (games_grid) {
		gridpage_replace_entries(games_grid, games_cfg, entries, count, 2, LV_MAX((count + 1) / 2, 1));
	}
	if (games_list) {
		lv_obj_clean(games_list);
		for (int i = 0; i < count; i++) {
			games_list_add_entry(&entries[i]);
		}
	}
	games_refresh_theme();
}

void gamespage_init(gui_config_t *cfg) {
	games_cfg = cfg;
	games_build(cfg);
}

void gamespage_open(void) {
	if (games_cfg) {
		gamespage_refresh_layout();
	}
	if (gamespage_screen) {
		switch_screen(gamespage_screen);
	}
}
