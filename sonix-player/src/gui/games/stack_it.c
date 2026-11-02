// stack_it.c -- Minimalist block stacking game for Sonix Player.
#include "stack_it.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lvgl/lvgl.h"
#include "src/gui/board_profile.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/games/games.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/theme.h"
#include "src/system/core/config.h"
#include "src/system/core/lang.h"

#define SECTION "stack_it"
#define FIELD_W 216
#define FIELD_H 216
#define BLOCK_H 16
#define MAX_VISIBLE 12
#define INITIAL_WIDTH 120

lv_obj_t *stack_it_screen;

static game_frame_t frame;
static gui_config_t *stack_cfg;

static lv_obj_t *block_objs[MAX_VISIBLE];
static int block_x[MAX_VISIBLE];
static int block_w[MAX_VISIBLE];

static lv_obj_t *active_block_obj;
static int active_x;
static int active_w;
static int active_dir;
static int active_speed;

static int current_floor;
static int base_x, base_w;
static bool game_over;
static int combo = 0;
static int flash_slot = -1;
static int flash_ticks = 0;

static const lv_color_t floor_colors[] = {
	LV_COLOR_MAKE(0x4a, 0x9e, 0xff), // blue
	LV_COLOR_MAKE(0x45, 0xd9, 0xd5), // cyan
	LV_COLOR_MAKE(0x42, 0xd6, 0x89), // green
	LV_COLOR_MAKE(0xab, 0x7f, 0xff), // purple
	LV_COLOR_MAKE(0xff, 0x69, 0xb4), // pink
	LV_COLOR_MAKE(0xff, 0x5c, 0x5c), // red
	LV_COLOR_MAKE(0xff, 0x88, 0x00), // orange
	LV_COLOR_MAKE(0xff, 0xcc, 0x00), // yellow
};

#define COLOR_COUNT (int)(sizeof(floor_colors) / sizeof(floor_colors[0]))

static lv_color_t color_for_floor(int floor) {
	return floor_colors[floor % COLOR_COUNT];
}

static void update_visuals(void) {
	for (int i = 0; i < MAX_VISIBLE; i++) {
		if (block_w[i] > 0) {
			lv_obj_set_hidden(block_objs[i], false);
			lv_obj_set_size(block_objs[i], block_w[i], BLOCK_H);
			int y = FIELD_H - (i + 1) * BLOCK_H;
			lv_obj_set_pos(block_objs[i], block_x[i], y);
			int f = (current_floor >= MAX_VISIBLE) ? (current_floor - MAX_VISIBLE + 1 + i) : i;
			if (i == flash_slot && flash_ticks > 0) {
				lv_obj_set_style_bg_color(block_objs[i], lv_color_white(), 0);
			} else {
				lv_obj_set_style_bg_color(block_objs[i], color_for_floor(f), 0);
			}
		} else {
			lv_obj_set_hidden(block_objs[i], true);
		}
	}

	if (!game_over) {
		lv_obj_set_hidden(active_block_obj, false);
		lv_obj_set_size(active_block_obj, active_w, BLOCK_H);
		int active_slot = (current_floor < MAX_VISIBLE) ? current_floor : (MAX_VISIBLE - 1);
		int y = FIELD_H - (active_slot + 1) * BLOCK_H;
		lv_obj_set_pos(active_block_obj, active_x, y);
		lv_obj_set_style_bg_color(active_block_obj, color_for_floor(current_floor), 0);
	} else {
		lv_obj_set_hidden(active_block_obj, true);
	}
}

static void spawn_active(void) {
	active_w = base_w;
	active_x = (rand() % 2 == 0) ? 0 : (FIELD_W - active_w);
	active_dir = (active_x == 0) ? 1 : -1;
	active_speed = 3 + (current_floor / 6);
	if (active_speed > 8) active_speed = 8;
	update_visuals();
}

static void new_game(void) {
	game_frame_hide_overlay(&frame);
	game_over = false;
	current_floor = 0;
	flash_slot = -1;
	flash_ticks = 0;
	combo = 0;
	base_x = (FIELD_W - INITIAL_WIDTH) / 2;
	base_w = INITIAL_WIDTH;

	for (int i = 0; i < MAX_VISIBLE; i++) {
		block_w[i] = 0;
	}

	// Base platform
	block_x[0] = base_x;
	block_w[0] = base_w;
	current_floor = 1;

	game_frame_set_score(&frame, 0);
	game_frame_set_hint(&frame, "Tap anywhere to drop block");
	spawn_active();
}

static void drop_block(void) {
	if (game_over) return;

	int left = LV_MAX(active_x, base_x);
	int right = LV_MIN(active_x + active_w, base_x + base_w);
	int overlap = right - left;

	if (overlap <= 0) {
		// Missed! Game Over
		game_over = true;
		update_visuals();
		char msg[64];
		snprintf(msg, sizeof(msg), "Stacked: %d blocks!", current_floor - 1);
		game_frame_show_overlay(&frame, "Game Over", msg, "Play Again", NULL);
		return;
	}

	bool perfect = (abs(active_x - base_x) <= 2);
	if (perfect) {
		left = base_x;
		overlap = base_w;
		combo++;
		game_frame_add_score(&frame, 5); // bonus
		char cbuf[32];
		snprintf(cbuf, sizeof(cbuf), "PERFECT! Combo x%d", combo);
		game_frame_set_hint(&frame, cbuf);
	} else {
		combo = 0;
		game_frame_set_hint(&frame, "Tap anywhere to drop block");
	}

	base_x = left;
	base_w = overlap;

	int placed_slot;
	if (current_floor < MAX_VISIBLE - 1) {
		placed_slot = current_floor;
		block_x[current_floor] = base_x;
		block_w[current_floor] = base_w;
	} else {
		// Scroll down
		for (int i = 0; i < MAX_VISIBLE - 2; i++) {
			block_x[i] = block_x[i + 1];
			block_w[i] = block_w[i + 1];
		}
		placed_slot = MAX_VISIBLE - 2;
		block_x[MAX_VISIBLE - 2] = base_x;
		block_w[MAX_VISIBLE - 2] = base_w;
	}

	if (perfect) {
		flash_slot = placed_slot;
		flash_ticks = 6; // ~150ms flash
	} else {
		flash_slot = -1;
		flash_ticks = 0;
	}
	current_floor++;
	game_frame_add_score(&frame, 1);
	spawn_active();
}

static void tap_cb(const lv_point_t *p) {
	(void)p;
	drop_block();
}

static void tick_cb(void) {
	if (flash_ticks > 0) {
		flash_ticks--;
		if (flash_ticks == 0 && flash_slot >= 0) {
			int f = (current_floor >= MAX_VISIBLE) ? (current_floor - MAX_VISIBLE + 1 + flash_slot) : flash_slot;
			if (flash_slot < MAX_VISIBLE && block_objs[flash_slot]) {
				lv_obj_set_style_bg_color(block_objs[flash_slot], color_for_floor(f), 0);
			}
			flash_slot = -1;
		}
	}
	if (game_over) return;

	active_x += active_dir * active_speed;
	if (active_x <= 0) {
		active_x = 0;
		active_dir = 1;
	} else if (active_x + active_w >= FIELD_W) {
		active_x = FIELD_W - active_w;
		active_dir = -1;
	}

	int active_slot = (current_floor < MAX_VISIBLE) ? current_floor : (MAX_VISIBLE - 1);
	int y = FIELD_H - (active_slot + 1) * BLOCK_H;
	lv_obj_set_pos(active_block_obj, active_x, y);
}

static void show_cb(void) {
	new_game();
	game_frame_set_period(&frame, 25);
}

static void hide_cb(void) {
	game_frame_set_period(&frame, 0);
}

static void theme_cb(void) {
	update_visuals();
}

static void stack_it_build(gui_config_t *cfg) {
	static const game_hooks_t hooks = {
		.on_show = show_cb,
		.on_hide = hide_cb,
		.on_tap = tap_cb,
		.on_tick = tick_cb,
		.on_theme = theme_cb,
	};

	frame.hooks = &hooks;
	frame.section = SECTION;
	frame.title = tr("stack_it");
	stack_it_screen = lv_obj_create(NULL);
	frame.screen = stack_it_screen;
	frame.on_overlay_primary = new_game;
	game_frame_build(&frame, cfg);

	lv_obj_t *container = lv_obj_create(frame.field);
	lv_obj_remove_style_all(container);
	lv_obj_set_size(container, FIELD_W, FIELD_H);
	lv_obj_center(container);
	lv_obj_set_scrollable(container, false);
	lv_obj_set_clickable(container, false);

	for (int i = 0; i < MAX_VISIBLE; i++) {
		block_objs[i] = lv_obj_create(container);
		lv_obj_remove_style_all(block_objs[i]);
		lv_obj_set_style_radius(block_objs[i], 3, 0);
		lv_obj_set_style_bg_opa(block_objs[i], LV_OPA_COVER, 0);
		lv_obj_set_scrollable(block_objs[i], false);
		lv_obj_set_clickable(block_objs[i], false);
	}

	active_block_obj = lv_obj_create(container);
	lv_obj_remove_style_all(active_block_obj);
	lv_obj_set_style_radius(active_block_obj, 3, 0);
	lv_obj_set_style_bg_opa(active_block_obj, LV_OPA_COVER, 0);
	lv_obj_set_scrollable(active_block_obj, false);
	lv_obj_set_clickable(active_block_obj, false);

	lv_obj_move_foreground(frame.overlay);

	game_frame_add_header_button(&frame, &icon_game_restart, new_game);
}

void stack_it_init(gui_config_t *cfg) {
	stack_cfg = cfg;
}

void stack_it_open(void) {
	if (!stack_it_screen) {
		stack_it_build(stack_cfg);
	}
	srand((unsigned)time(NULL));
	game_frame_open(&frame);
}
