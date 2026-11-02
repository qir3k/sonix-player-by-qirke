// minesweeper.c -- Minimalist dark Minesweeper for Sonix Player.
#include "minesweeper.h"

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

#define SECTION "minesweeper"
#define COLS 8
#define ROWS 8
#define TOTAL_CELLS (COLS * ROWS)
#define MINES_COUNT 10
#define CELL_SIZE 24
#define CELL_GAP 3

typedef enum {
	CELL_HIDDEN = 0,
	CELL_REVEALED,
	CELL_FLAGGED
} cell_state_t;

lv_obj_t *minesweeper_screen;

static game_frame_t frame;
static gui_config_t *mines_cfg;
static lv_obj_t *board;
static lv_obj_t *cell_obj[TOTAL_CELLS];
static lv_obj_t *cell_label[TOTAL_CELLS];

static cell_state_t state[TOTAL_CELLS];
static bool has_mine[TOTAL_CELLS];
static uint8_t adj_mines[TOTAL_CELLS];
static bool first_click;
static bool game_over;
static bool flag_mode;
static int flags_placed;
static int revealed_count;

static const lv_color_t num_colors[] = {
	LV_COLOR_MAKE(0x00, 0x00, 0x00), // 0: unused
	LV_COLOR_MAKE(0x4a, 0x9e, 0xff), // 1: blue
	LV_COLOR_MAKE(0x42, 0xd6, 0x89), // 2: green
	LV_COLOR_MAKE(0xff, 0x5c, 0x5c), // 3: red
	LV_COLOR_MAKE(0xab, 0x7f, 0xff), // 4: purple
	LV_COLOR_MAKE(0xff, 0xb3, 0x47), // 5: amber
	LV_COLOR_MAKE(0x45, 0xd9, 0xd5), // 6: cyan
	LV_COLOR_MAKE(0xff, 0x69, 0xb4), // 7: pink
	LV_COLOR_MAKE(0xaa, 0xaa, 0xaa)  // 8: gray
};

static void update_hint(void) {
	char buf[64];
	snprintf(buf, sizeof(buf), "Mines: %d • Mode: %s",
		 MINES_COUNT - flags_placed, flag_mode ? "Flag ⚑" : "Dig ⛏");
	game_frame_set_hint(&frame, buf);
}

static void update_cell_visual(int i) {
	if (state[i] == CELL_HIDDEN) {
		lv_obj_set_style_bg_color(cell_obj[i], lv_color_hex(0x28282c), 0);
		lv_obj_set_style_border_width(cell_obj[i], 0, 0);
		lv_label_set_text(cell_label[i], "");
	} else if (state[i] == CELL_FLAGGED) {
		lv_obj_set_style_bg_color(cell_obj[i], lv_color_hex(0x383428), 0);
		lv_obj_set_style_border_width(cell_obj[i], 1, 0);
		lv_obj_set_style_border_color(cell_obj[i], theme()->accent, 0);
		lv_label_set_text(cell_label[i], "⚑");
		lv_obj_set_style_text_color(cell_label[i], theme()->accent, 0);
	} else { // REVEALED
		lv_obj_set_style_bg_color(cell_obj[i], lv_color_hex(0x141416), 0);
		lv_obj_set_style_border_width(cell_obj[i], 1, 0);
		lv_obj_set_style_border_color(cell_obj[i], lv_color_hex(0x2e2e30), 0);
		if (has_mine[i]) {
			lv_obj_set_style_bg_color(cell_obj[i], lv_color_hex(0x8b1e1e), 0);
			lv_label_set_text(cell_label[i], "✹");
			lv_obj_set_style_text_color(cell_label[i], lv_color_white(), 0);
		} else if (adj_mines[i] > 0 && adj_mines[i] <= 8) {
			char nbuf[4];
			snprintf(nbuf, sizeof(nbuf), "%d", adj_mines[i]);
			lv_label_set_text(cell_label[i], nbuf);
			lv_obj_set_style_text_color(cell_label[i], num_colors[adj_mines[i]], 0);
		} else {
			lv_label_set_text(cell_label[i], "");
		}
	}
}

static void place_mines(int safe_idx) {
	int placed = 0;
	while (placed < MINES_COUNT) {
		int r = rand() % TOTAL_CELLS;
		if (r != safe_idx && !has_mine[r]) {
			has_mine[r] = true;
			placed++;
		}
	}
	for (int row = 0; row < ROWS; row++) {
		for (int col = 0; col < COLS; col++) {
			int idx = row * COLS + col;
			if (has_mine[idx]) continue;
			int count = 0;
			for (int dr = -1; dr <= 1; dr++) {
				for (int dc = -1; dc <= 1; dc++) {
					if (dr == 0 && dc == 0) continue;
					int nr = row + dr, nc = col + dc;
					if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
						if (has_mine[nr * COLS + nc]) count++;
					}
				}
			}
			adj_mines[idx] = (uint8_t)count;
		}
	}
}

static void flood_reveal(int start_idx) {
	int queue[TOTAL_CELLS];
	int head = 0, tail = 0;
	queue[tail++] = start_idx;

	while (head < tail) {
		int curr = queue[head++];
		if (state[curr] != CELL_HIDDEN) continue;
		state[curr] = CELL_REVEALED;
		revealed_count++;
		update_cell_visual(curr);
		game_frame_add_score(&frame, 10);

		if (adj_mines[curr] == 0) {
			int r = curr / COLS;
			int c = curr % COLS;
			for (int dr = -1; dr <= 1; dr++) {
				for (int dc = -1; dc <= 1; dc++) {
					int nr = r + dr, nc = c + dc;
					if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
						int nidx = nr * COLS + nc;
						if (state[nidx] == CELL_HIDDEN && !has_mine[nidx]) {
							queue[tail++] = nidx;
						}
					}
				}
			}
		}
	}
}

static void new_game(void) {
	game_frame_hide_overlay(&frame);
	memset(state, 0, sizeof(state));
	memset(has_mine, 0, sizeof(has_mine));
	memset(adj_mines, 0, sizeof(adj_mines));
	first_click = true;
	game_over = false;
	flags_placed = 0;
	revealed_count = 0;
	game_frame_set_score(&frame, 0);

	for (int i = 0; i < TOTAL_CELLS; i++) {
		update_cell_visual(i);
	}
	update_hint();
}

static void toggle_mode_cb(void) {
	flag_mode = !flag_mode;
	update_hint();
}

static void restart_cb(void) {
	new_game();
}

static void overlay_btn_cb(void) {
	new_game();
}

static void tap_cb(const lv_point_t *p) {
	if (game_over) return;
	int x = p->x - (frame.field_w - (COLS * CELL_SIZE + (COLS - 1) * CELL_GAP)) / 2;
	int y = p->y - (frame.field_h - (ROWS * CELL_SIZE + (ROWS - 1) * CELL_GAP)) / 2;
	if (x < 0 || y < 0) return;

	int col = x / (CELL_SIZE + CELL_GAP);
	int row = y / (CELL_SIZE + CELL_GAP);
	if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return;

	int idx = row * COLS + col;

	if (flag_mode) {
		if (state[idx] == CELL_HIDDEN) {
			state[idx] = CELL_FLAGGED;
			flags_placed++;
			update_cell_visual(idx);
			update_hint();
		} else if (state[idx] == CELL_FLAGGED) {
			state[idx] = CELL_HIDDEN;
			flags_placed--;
			update_cell_visual(idx);
			update_hint();
		}
		return;
	}

	// Dig mode
	if (state[idx] == CELL_FLAGGED) return;

	if (state[idx] == CELL_REVEALED) {
		// Chord: if revealed and flags match adjacent count, reveal other neighbors
		int flags = 0;
		for (int dr = -1; dr <= 1; dr++) {
			for (int dc = -1; dc <= 1; dc++) {
				int nr = row + dr, nc = col + dc;
				if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
					if (state[nr * COLS + nc] == CELL_FLAGGED) flags++;
				}
			}
		}
		if (flags == adj_mines[idx]) {
			for (int dr = -1; dr <= 1; dr++) {
				for (int dc = -1; dc <= 1; dc++) {
					int nr = row + dr, nc = col + dc;
					if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
						int nidx = nr * COLS + nc;
						if (state[nidx] == CELL_HIDDEN) {
							if (has_mine[nidx]) {
								// Hit a mine!
								state[nidx] = CELL_REVEALED;
								update_cell_visual(nidx);
								game_over = true;
								game_frame_show_overlay(&frame, "Game Over", "Boom! You hit a mine.", "Play Again", NULL);
								return;
							}
							flood_reveal(nidx);
						}
					}
				}
			}
		}
		return;
	}

	// Hidden cell
	if (first_click) {
		first_click = false;
		place_mines(idx);
	}

	if (has_mine[idx]) {
		// Boom! Reveal all mines
		game_over = true;
		for (int i = 0; i < TOTAL_CELLS; i++) {
			if (has_mine[i]) {
				state[i] = CELL_REVEALED;
				update_cell_visual(i);
			}
		}
		game_frame_show_overlay(&frame, "Game Over", "Boom! You hit a mine.", "Play Again", NULL);
		return;
	}

	flood_reveal(idx);

	if (revealed_count >= TOTAL_CELLS - MINES_COUNT) {
		game_over = true;
		game_frame_add_score(&frame, 500);
		game_frame_show_overlay(&frame, "Victory!", "All safe cells cleared!", "Play Again", NULL);
	}
}

static void show_cb(void) {
	if (first_click) {
		new_game();
	}
}

static void theme_cb(void) {
	for (int i = 0; i < TOTAL_CELLS; i++) {
		update_cell_visual(i);
	}
	update_hint();
}

static void minesweeper_build(gui_config_t *cfg) {
	static const game_hooks_t hooks = {
		.on_show = show_cb,
		.on_tap = tap_cb,
		.on_theme = theme_cb,
	};

	frame.hooks = &hooks;
	frame.section = SECTION;
	frame.title = tr("minesweeper");
	minesweeper_screen = lv_obj_create(NULL);
	frame.screen = minesweeper_screen;
	frame.on_overlay_primary = overlay_btn_cb;
	game_frame_build(&frame, cfg);

	int board_w = COLS * CELL_SIZE + (COLS - 1) * CELL_GAP;
	int board_h = ROWS * CELL_SIZE + (ROWS - 1) * CELL_GAP;

	board = lv_obj_create(frame.field);
	lv_obj_remove_style_all(board);
	lv_obj_set_size(board, board_w, board_h);
	lv_obj_center(board);
	lv_obj_set_scrollable(board, false);
	lv_obj_set_clickable(board, false);

	for (int row = 0; row < ROWS; row++) {
		for (int col = 0; col < COLS; col++) {
			int i = row * COLS + col;
			cell_obj[i] = lv_obj_create(board);
			lv_obj_remove_style_all(cell_obj[i]);
			lv_obj_set_size(cell_obj[i], CELL_SIZE, CELL_SIZE);
			lv_obj_set_pos(cell_obj[i], col * (CELL_SIZE + CELL_GAP), row * (CELL_SIZE + CELL_GAP));
			lv_obj_set_style_radius(cell_obj[i], 4, 0);
			lv_obj_set_style_bg_opa(cell_obj[i], LV_OPA_COVER, 0);
			lv_obj_set_scrollable(cell_obj[i], false);
			lv_obj_set_clickable(cell_obj[i], false);

			cell_label[i] = lv_label_create(cell_obj[i]);
			lv_obj_set_style_text_font(cell_label[i], &font_ui_14, 0);
			lv_obj_center(cell_label[i]);
		}
	}

	lv_obj_move_foreground(frame.overlay);

	game_frame_add_header_button(&frame, &icon_game_undo, toggle_mode_cb);
	game_frame_add_header_button(&frame, &icon_game_restart, restart_cb);

	new_game();
}

void minesweeper_init(gui_config_t *cfg) {
	mines_cfg = cfg;
}

void minesweeper_open(void) {
	if (!minesweeper_screen) {
		minesweeper_build(mines_cfg);
	}
	srand((unsigned)time(NULL));
	game_frame_open(&frame);
}
