// sudoku.c -- Minimalist dark Sudoku for Sonix Player.
#include "sudoku.h"

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

#define SECTION "sudoku"
#define SUDOKU_SIZE 9
#define SUDOKU_CELLS 81
#define CELL_PX 21

lv_obj_t *sudoku_screen;

static game_frame_t frame;
static gui_config_t *sudoku_cfg;
static lv_obj_t *board;
static lv_obj_t *cell_obj[SUDOKU_CELLS];
static lv_obj_t *cell_label[SUDOKU_CELLS];
static lv_obj_t *num_bar;
static lv_obj_t *num_btn[10];

static uint8_t initial_grid[SUDOKU_CELLS];
static uint8_t current_grid[SUDOKU_CELLS];
static bool conflicts[SUDOKU_CELLS];
static int selected_cell = -1;

// History for undo
typedef struct {
	int cell;
	uint8_t old_val;
} move_t;

#define MAX_MOVES 128
static move_t move_history[MAX_MOVES];
static int move_count = 0;

// Curated seed puzzle
static const char *default_seed =
	"530070000"
	"600195000"
	"098000060"
	"800060003"
	"400803001"
	"700020006"
	"060000280"
	"000419005"
	"000080079";

static void check_conflicts(void) {
	memset(conflicts, 0, sizeof(conflicts));

	// Check rows and columns
	for (int i = 0; i < SUDOKU_SIZE; i++) {
		for (int j = 0; j < SUDOKU_SIZE; j++) {
			int r_idx = i * SUDOKU_SIZE + j;
			uint8_t val = current_grid[r_idx];
			if (val == 0) continue;

			// Check rest of row
			for (int k = j + 1; k < SUDOKU_SIZE; k++) {
				int idx2 = i * SUDOKU_SIZE + k;
				if (current_grid[idx2] == val) {
					conflicts[r_idx] = true;
					conflicts[idx2] = true;
				}
			}

			// Check rest of col
			for (int k = i + 1; k < SUDOKU_SIZE; k++) {
				int idx2 = k * SUDOKU_SIZE + j;
				if (current_grid[idx2] == val) {
					conflicts[r_idx] = true;
					conflicts[idx2] = true;
				}
			}
		}
	}

	// Check 3x3 blocks
	for (int bi = 0; bi < 3; bi++) {
		for (int bj = 0; bj < 3; bj++) {
			int cells[9];
			int cidx = 0;
			for (int r = 0; r < 3; r++) {
				for (int c = 0; c < 3; c++) {
					cells[cidx++] = (bi * 3 + r) * SUDOKU_SIZE + (bj * 3 + c);
				}
			}
			for (int a = 0; a < 9; a++) {
				uint8_t val = current_grid[cells[a]];
				if (val == 0) continue;
				for (int b = a + 1; b < 9; b++) {
					if (current_grid[cells[b]] == val) {
						conflicts[cells[a]] = true;
						conflicts[cells[b]] = true;
					}
				}
			}
		}
	}
}

static void update_cell_visual(int i) {
	uint8_t val = current_grid[i];
	if (val > 0 && val <= 9) {
		char buf[4];
		snprintf(buf, sizeof(buf), "%d", val);
		lv_label_set_text(cell_label[i], buf);
	} else {
		lv_label_set_text(cell_label[i], "");
	}

	bool is_selected = (i == selected_cell);
	if (is_selected) {
		lv_obj_set_style_bg_color(cell_obj[i], lv_color_hex(0x383424), 0);
		lv_obj_set_style_border_width(cell_obj[i], 2, 0);
		lv_obj_set_style_border_color(cell_obj[i], theme()->accent, 0);
	} else {
		lv_obj_set_style_bg_color(cell_obj[i], lv_color_hex(0x1a1a1c), 0);
		lv_obj_set_style_border_width(cell_obj[i], 1, 0);
		lv_obj_set_style_border_color(cell_obj[i], lv_color_hex(0x28282c), 0);
	}

	if (conflicts[i]) {
		lv_obj_set_style_text_color(cell_label[i], lv_color_hex(0xff4444), 0);
	} else if (initial_grid[i] > 0) {
		lv_obj_set_style_text_color(cell_label[i], lv_color_white(), 0);
	} else {
		lv_obj_set_style_text_color(cell_label[i], theme()->accent, 0);
	}
}

static void update_all_visuals(void) {
	check_conflicts();
	for (int i = 0; i < SUDOKU_CELLS; i++) {
		update_cell_visual(i);
	}
}

static bool check_win(void) {
	for (int i = 0; i < SUDOKU_CELLS; i++) {
		if (current_grid[i] == 0 || conflicts[i]) {
			return false;
		}
	}
	return true;
}

static void new_game(void) {
	game_frame_hide_overlay(&frame);
	move_count = 0;
	selected_cell = -1;

	// Generate a random digit permutation
	uint8_t perm[10];
	for (int i = 1; i <= 9; i++) perm[i] = (uint8_t)i;
	for (int i = 9; i > 1; i--) {
		int j = 1 + (rand() % i);
		uint8_t tmp = perm[i];
		perm[i] = perm[j];
		perm[j] = tmp;
	}

	for (int i = 0; i < SUDOKU_CELLS; i++) {
		char c = default_seed[i];
		if (c >= '1' && c <= '9') {
			initial_grid[i] = perm[c - '0'];
			current_grid[i] = initial_grid[i];
		} else {
			initial_grid[i] = 0;
			current_grid[i] = 0;
		}
	}

	game_frame_set_score(&frame, 0);
	game_frame_set_hint(&frame, "Tap a cell, then choose a number (1-9)");
	update_all_visuals();
}

static void set_selected_number(uint8_t num) {
	if (selected_cell < 0 || selected_cell >= SUDOKU_CELLS) return;
	if (initial_grid[selected_cell] > 0) return; // clue cell cannot be changed

	if (move_count < MAX_MOVES) {
		move_history[move_count].cell = selected_cell;
		move_history[move_count].old_val = current_grid[selected_cell];
		move_count++;
	}

	current_grid[selected_cell] = num;
	update_all_visuals();

	if (check_win()) {
		game_frame_add_score(&frame, 1000);
		game_frame_show_overlay(&frame, "Sudoku Solved!", "Congratulations! Puzzle completed.", "New Puzzle", NULL);
	}
}

static void num_btn_cb(lv_event_t *e) {
	uintptr_t val = (uintptr_t)lv_event_get_user_data(e);
	set_selected_number((uint8_t)val);
}

static void undo_move(void) {
	if (move_count <= 0) return;
	move_count--;
	int c = move_history[move_count].cell;
	current_grid[c] = move_history[move_count].old_val;
	selected_cell = c;
	update_all_visuals();
}

static int find_sudoku_coord(int pos) {
	for (int i = 0; i < SUDOKU_SIZE; i++) {
		int start = i * CELL_PX + (i / 3) * 2;
		int end = start + CELL_PX;
		if (pos >= start && pos < end) {
			return i;
		}
	}
	int approx = pos / CELL_PX;
	if (approx < 0) approx = 0;
	if (approx >= SUDOKU_SIZE) approx = SUDOKU_SIZE - 1;
	return approx;
}

static void tap_cb(const lv_point_t *p) {
	int board_size = SUDOKU_SIZE * CELL_PX + 4;
	int bx = (frame.field_w - board_size) / 2;
	int by = 4;
	int x = p->x - bx;
	int y = p->y - by;
	if (x < 0 || y < 0 || x >= board_size || y >= board_size) return;

	int col = find_sudoku_coord(x);
	int row = find_sudoku_coord(y);

	selected_cell = row * SUDOKU_SIZE + col;
	update_all_visuals();
}

static void show_cb(void) {
	if (selected_cell == -1 && move_count == 0) {
		new_game();
	}
}

static void theme_cb(void) {
	update_all_visuals();
}

static void sudoku_build(gui_config_t *cfg) {
	static const game_hooks_t hooks = {
		.on_show = show_cb,
		.on_tap = tap_cb,
		.on_theme = theme_cb,
	};

	frame.hooks = &hooks;
	frame.section = SECTION;
	frame.title = tr("sudoku");
	sudoku_screen = lv_obj_create(NULL);
	frame.screen = sudoku_screen;
	frame.on_overlay_primary = new_game;
	game_frame_build(&frame, cfg);

	int board_size = SUDOKU_SIZE * CELL_PX + 4;
	board = lv_obj_create(frame.field);
	lv_obj_remove_style_all(board);
	lv_obj_set_size(board, board_size, board_size);
	lv_obj_align(board, LV_ALIGN_TOP_MID, 0, 4);
	lv_obj_set_scrollable(board, false);
	lv_obj_set_clickable(board, false);

	for (int row = 0; row < SUDOKU_SIZE; row++) {
		for (int col = 0; col < SUDOKU_SIZE; col++) {
			int i = row * SUDOKU_SIZE + col;
			cell_obj[i] = lv_obj_create(board);
			lv_obj_remove_style_all(cell_obj[i]);
			lv_obj_set_size(cell_obj[i], CELL_PX, CELL_PX);

			int x_offset = col * CELL_PX + (col / 3) * 2;
			int y_offset = row * CELL_PX + (row / 3) * 2;
			lv_obj_set_pos(cell_obj[i], x_offset, y_offset);
			lv_obj_set_style_radius(cell_obj[i], 2, 0);
			lv_obj_set_style_bg_opa(cell_obj[i], LV_OPA_COVER, 0);
			lv_obj_set_scrollable(cell_obj[i], false);
			lv_obj_set_clickable(cell_obj[i], false);

			cell_label[i] = lv_label_create(cell_obj[i]);
			lv_obj_set_style_text_font(cell_label[i], &font_ui_14, 0);
			lv_obj_center(cell_label[i]);
		}
	}

	// Number selector bar at bottom of field
	num_bar = lv_obj_create(frame.field);
	lv_obj_remove_style_all(num_bar);
	lv_obj_set_size(num_bar, 216, 26);
	lv_obj_align(num_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_obj_set_scrollable(num_bar, false);

	for (int i = 0; i <= 9; i++) {
		num_btn[i] = lv_btn_create(num_bar);
		lv_obj_set_size(num_btn[i], 19, 24);
		lv_obj_set_pos(num_btn[i], i * 21 + 2, 1);
		lv_obj_set_style_radius(num_btn[i], 3, 0);
		lv_obj_set_style_bg_color(num_btn[i], lv_color_hex(0x28282c), 0);
		lv_obj_set_style_bg_opa(num_btn[i], LV_OPA_COVER, 0);
		lv_obj_set_style_shadow_width(num_btn[i], 0, 0);
		lv_obj_set_style_pad_all(num_btn[i], 0, 0);

		lv_obj_t *lbl = lv_label_create(num_btn[i]);
		lv_obj_set_style_text_font(lbl, &font_ui_14, 0);
		if (i == 9) {
			lv_label_set_text(lbl, "X");
			lv_obj_set_style_text_color(lbl, lv_color_hex(0xff5555), 0);
			lv_obj_add_event_cb(num_btn[i], num_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)0);
		} else {
			char nbuf[4];
			snprintf(nbuf, sizeof(nbuf), "%d", i + 1);
			lv_label_set_text(lbl, nbuf);
			lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
			lv_obj_add_event_cb(num_btn[i], num_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)(i + 1));
		}
		lv_obj_center(lbl);
	}

	lv_obj_move_foreground(frame.overlay);

	game_frame_add_header_button(&frame, &icon_game_undo, undo_move);
	game_frame_add_header_button(&frame, &icon_game_restart, new_game);

	new_game();
}

void sudoku_init(gui_config_t *cfg) {
	sudoku_cfg = cfg;
}

void sudoku_open(void) {
	if (!sudoku_screen) {
		sudoku_build(sudoku_cfg);
	}
	srand((unsigned)time(NULL));
	game_frame_open(&frame);
}
