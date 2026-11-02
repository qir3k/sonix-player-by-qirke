#include "gamespage.h"

#include <string.h>

#include "lvgl/lvgl.h"

#include "src/gui/board_profile.h"
#include "src/gui/flappybird/flappybird.h"
#include "src/gui/games/game2048.h"
#include "src/gui/pocket_arcade/pocket_arcade.h"
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

	} else if (strncmp(label, "arcade", 6) == 0) {
		*icon = &icon_menu_arcade;
		*tone = THEME_SEMANTIC_PURPLE;
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
	{"arcade", &icon_menu_arcade, NULL, pocket_arcade_open},
	{"arcade_mines", &icon_menu_arcade, NULL, arcade_mines_open},
	{"arcade_bricks", &icon_menu_arcade, NULL, arcade_bricks_open},
	{"arcade_racer", &icon_menu_arcade, NULL, arcade_racer_open},
	{"arcade_stack", &icon_menu_arcade, NULL, arcade_stack_open},
	{"arcade_boxes", &icon_menu_arcade, NULL, arcade_boxes_open},
	{"arcade_golf", &icon_menu_arcade, NULL, arcade_golf_open},
	{"arcade_orbit", &icon_menu_arcade, NULL, arcade_orbit_open},
	{"arcade_melody", &icon_menu_arcade, NULL, arcade_melody_open},
	{"arcade_quiz", &icon_menu_arcade, NULL, arcade_quiz_open},
	{"arcade_beat", &icon_menu_arcade, NULL, arcade_beat_open},
	{"arcade_dungeon", &icon_menu_arcade, NULL, arcade_dungeon_open},
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

		} else if (strcmp(label, "arcade") == 0) {
			enabled = addons_enabled("pocket_arcade");
		} else if (strncmp(label, "arcade_", 7) == 0) {
			enabled = addons_enabled(label);
		}
		if (enabled) {
			out[count++] = ALL_GAMES[i];
		}
	}
	return count;
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

static void games_entries_refresh(void) {
	grid_entry_t entries[GAMES_MAX];
	int count = games_entries_build(entries, GAMES_MAX);

	if (games_grid) {
		int cols = count > 4 ? 3 : 2;
		int rows = LV_MAX((count + cols - 1) / cols, 1);
		gridpage_replace_entries(games_grid, games_cfg, entries, count, cols, rows);
	}
	if (games_list) {
		lv_obj_clean(games_list);
		for (int i = 0; i < count; i++) {
			games_list_add_entry(&entries[i]);
		}
	}
}

static void games_screen_loaded_cb(lv_event_t *e) {
	(void)e;
	games_entries_refresh();
}

void gamespage_open(void) {
	games_entries_refresh();
	switch_screen(gamespage_screen);
}

void gamespage_init(gui_config_t *cfg) {
	games_cfg = cfg;
	gamespage_screen = lv_obj_create(NULL);
	lv_obj_add_style(gamespage_screen, &theme_style_screen, 0);

	grid_entry_t entries[GAMES_MAX];
	int count = games_entries_build(entries, GAMES_MAX);

	int cols = count > 4 ? 3 : 2;
	int rows = LV_MAX((count + cols - 1) / cols, 1);
	games_grid = gridpage_build(gamespage_screen, cfg, entries, count, cols, rows, true);

	if (bp_is_tempotec_v1()) {
		games_list = settingsrow_page(gamespage_screen, cfg, "games");
		for (int i = 0; i < count; i++) {
			games_list_add_entry(&entries[i]);
		}
	} else {
		settingsrow_title(gamespage_screen, cfg, "games");
	}

	games_refresh_theme();
	theme_register_refresh(games_refresh_theme);

	lv_obj_add_event_cb(gamespage_screen, games_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
	switcher_attach_back_gesture(gamespage_screen);
}
