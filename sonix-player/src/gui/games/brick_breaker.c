// brick_breaker.c -- Minimalist dark Brick Breaker for Sonix Player.
#include "brick_breaker.h"

#include <math.h>
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

#define SECTION "brick_breaker"
#define FIELD_W 216
#define FIELD_H 216

#define BRICK_ROWS 5
#define BRICK_COLS 6
#define TOTAL_BRICKS (BRICK_ROWS * BRICK_COLS)
#define BRICK_W 32
#define BRICK_H 10
#define BRICK_GAP 3

#define PADDLE_W 46
#define PADDLE_H 7
#define PADDLE_Y 202

#define BALL_SIZE 6

lv_obj_t *brick_breaker_screen;

static game_frame_t frame;
static gui_config_t *brick_cfg;

static lv_obj_t *brick_objs[TOTAL_BRICKS];
static bool brick_alive[TOTAL_BRICKS];

static lv_obj_t *paddle_obj;
static int paddle_x;

static lv_obj_t *ball_obj;
static float ball_x, ball_y;
static float ball_vx, ball_vy;
static bool ball_launched;

static int lives = 3;
static int bricks_left = TOTAL_BRICKS;
static bool game_over = false;

static const lv_color_t row_colors[BRICK_ROWS] = {
	LV_COLOR_MAKE(0xff, 0x33, 0x66), // Pink
	LV_COLOR_MAKE(0xff, 0x88, 0x00), // Orange
	LV_COLOR_MAKE(0xff, 0xcc, 0x00), // Amber
	LV_COLOR_MAKE(0x00, 0xe6, 0x76), // Green
	LV_COLOR_MAKE(0x00, 0xd4, 0xff), // Cyan
};

static void update_hint(void) {
	char buf[64];
	snprintf(buf, sizeof(buf), "Balls: %d • Score: %d", lives, frame.score);
	game_frame_set_hint(&frame, buf);
}

static void reset_ball(void) {
	ball_launched = false;
	ball_x = paddle_x + (PADDLE_W - BALL_SIZE) / 2.0f;
	ball_y = PADDLE_Y - BALL_SIZE - 2;
	ball_vx = 2.0f;
	ball_vy = -3.2f;

	lv_obj_set_pos(ball_obj, (int)ball_x, (int)ball_y);
	update_hint();
}

static void new_game(void) {
	game_frame_hide_overlay(&frame);
	game_over = false;
	lives = 3;
	bricks_left = TOTAL_BRICKS;
	paddle_x = (FIELD_W - PADDLE_W) / 2;
	lv_obj_set_pos(paddle_obj, paddle_x, PADDLE_Y);

	for (int i = 0; i < TOTAL_BRICKS; i++) {
		brick_alive[i] = true;
		lv_obj_set_hidden(brick_objs[i], false);
	}

	game_frame_set_score(&frame, 0);
	reset_ball();
}

static void launch_ball(void) {
	if (!ball_launched && !game_over) {
		ball_launched = true;
		ball_vx = ((rand() % 2 == 0) ? 2.2f : -2.2f);
		ball_vy = -3.5f;
	}
}

static void tap_cb(const lv_point_t *p) {
	if (game_over) return;

	if (!ball_launched) {
		launch_ball();
		return;
	}

	// Move paddle to tap X
	int target_x = p->x - PADDLE_W / 2;
	if (target_x < 0) target_x = 0;
	if (target_x > FIELD_W - PADDLE_W) target_x = FIELD_W - PADDLE_W;
	paddle_x = target_x;
	lv_obj_set_pos(paddle_obj, paddle_x, PADDLE_Y);
}

static void swipe_cb(lv_dir_t dir, const lv_point_t *p) {
	(void)dir;
	if (game_over) return;
	int target_x = p->x - PADDLE_W / 2;
	if (target_x < 0) target_x = 0;
	if (target_x > FIELD_W - PADDLE_W) target_x = FIELD_W - PADDLE_W;
	paddle_x = target_x;
	lv_obj_set_pos(paddle_obj, paddle_x, PADDLE_Y);
}

static void tick_cb(void) {
	if (game_over) return;

	if (!ball_launched) {
		// Keep docked on paddle
		ball_x = paddle_x + (PADDLE_W - BALL_SIZE) / 2.0f;
		ball_y = PADDLE_Y - BALL_SIZE - 2;
		lv_obj_set_pos(ball_obj, (int)ball_x, (int)ball_y);
		return;
	}

	ball_x += ball_vx;
	ball_y += ball_vy;

	// Bounce off left/right walls
	if (ball_x <= 0) {
		ball_x = 0;
		ball_vx = -ball_vx;
	} else if (ball_x + BALL_SIZE >= FIELD_W) {
		ball_x = FIELD_W - BALL_SIZE;
		ball_vx = -ball_vx;
	}

	// Bounce off top wall
	if (ball_y <= 0) {
		ball_y = 0;
		ball_vy = -ball_vy;
	}

	// Paddle collision
	if (ball_vy > 0 && ball_y + BALL_SIZE >= PADDLE_Y && ball_y <= PADDLE_Y + PADDLE_H) {
		if (ball_x + BALL_SIZE >= paddle_x && ball_x <= paddle_x + PADDLE_W) {
			ball_y = PADDLE_Y - BALL_SIZE;
			ball_vy = -fabsf(ball_vy);

			// Angle deflection
			float hit_offset = (ball_x + BALL_SIZE / 2.0f) - (paddle_x + PADDLE_W / 2.0f);
			ball_vx = hit_offset * 0.18f;
		}
	}

	// Brick collisions
	int start_x = (FIELD_W - (BRICK_COLS * BRICK_W + (BRICK_COLS - 1) * BRICK_GAP)) / 2;
	int start_y = 16;

	for (int r = 0; r < BRICK_ROWS; r++) {
		for (int c = 0; c < BRICK_COLS; c++) {
			int i = r * BRICK_COLS + c;
			if (!brick_alive[i]) continue;

			int bx = start_x + c * (BRICK_W + BRICK_GAP);
			int by = start_y + r * (BRICK_H + BRICK_GAP);

			if (ball_x + BALL_SIZE >= bx && ball_x <= bx + BRICK_W &&
			    ball_y + BALL_SIZE >= by && ball_y <= by + BRICK_H) {
				// Destroy brick
				brick_alive[i] = false;
				lv_obj_set_hidden(brick_objs[i], true);
				bricks_left--;
				ball_vy = -ball_vy;

				int pts = (BRICK_ROWS - r) * 10;
				game_frame_add_score(&frame, pts);
				update_hint();

				if (bricks_left <= 0) {
					game_over = true;
					game_frame_add_score(&frame, 500);
					game_frame_show_overlay(&frame, "Victory!", "All bricks destroyed!", "Play Again", NULL);
				}
				goto ball_done;
			}
		}
	}

ball_done:
	// Bottom floor: Ball lost!
	if (ball_y > FIELD_H) {
		lives--;
		if (lives <= 0) {
			game_over = true;
			char msg[64];
			snprintf(msg, sizeof(msg), "Final Score: %d", frame.score);
			game_frame_show_overlay(&frame, "Game Over", msg, "Play Again", NULL);
		} else {
			reset_ball();
		}
	}

	lv_obj_set_pos(ball_obj, (int)ball_x, (int)ball_y);
}

static void show_cb(void) {
	new_game();
	game_frame_set_period(&frame, 25);
}

static void hide_cb(void) {
	game_frame_set_period(&frame, 0);
}

static void theme_cb(void) {
	lv_obj_set_style_bg_color(paddle_obj, theme()->accent, 0);
	lv_obj_set_style_bg_color(ball_obj, lv_color_white(), 0);
}

static void brick_breaker_build(gui_config_t *cfg) {
	static const game_hooks_t hooks = {
		.on_show = show_cb,
		.on_hide = hide_cb,
		.on_tap = tap_cb,
		.on_swipe = swipe_cb,
		.on_tick = tick_cb,
		.on_theme = theme_cb,
	};

	frame.hooks = &hooks;
	frame.section = SECTION;
	frame.title = tr("brick_breaker");
	brick_breaker_screen = lv_obj_create(NULL);
	frame.screen = brick_breaker_screen;
	frame.on_overlay_primary = new_game;
	game_frame_build(&frame, cfg);

	lv_obj_t *container = lv_obj_create(frame.field);
	lv_obj_remove_style_all(container);
	lv_obj_set_size(container, FIELD_W, FIELD_H);
	lv_obj_center(container);
	lv_obj_set_scrollable(container, false);
	lv_obj_set_clickable(container, false);

	// Create bricks
	int start_x = (FIELD_W - (BRICK_COLS * BRICK_W + (BRICK_COLS - 1) * BRICK_GAP)) / 2;
	int start_y = 16;

	for (int r = 0; r < BRICK_ROWS; r++) {
		for (int c = 0; c < BRICK_COLS; c++) {
			int i = r * BRICK_COLS + c;
			brick_objs[i] = lv_obj_create(container);
			lv_obj_remove_style_all(brick_objs[i]);
			lv_obj_set_size(brick_objs[i], BRICK_W, BRICK_H);
			lv_obj_set_pos(brick_objs[i], start_x + c * (BRICK_W + BRICK_GAP),
				       start_y + r * (BRICK_H + BRICK_GAP));
			lv_obj_set_style_radius(brick_objs[i], 2, 0);
			lv_obj_set_style_bg_color(brick_objs[i], row_colors[r], 0);
			lv_obj_set_style_bg_opa(brick_objs[i], LV_OPA_COVER, 0);
			lv_obj_set_scrollable(brick_objs[i], false);
			lv_obj_set_clickable(brick_objs[i], false);
		}
	}

	// Paddle
	paddle_obj = lv_obj_create(container);
	lv_obj_remove_style_all(paddle_obj);
	lv_obj_set_size(paddle_obj, PADDLE_W, PADDLE_H);
	lv_obj_set_pos(paddle_obj, (FIELD_W - PADDLE_W) / 2, PADDLE_Y);
	lv_obj_set_style_radius(paddle_obj, 3, 0);
	lv_obj_set_style_bg_color(paddle_obj, theme()->accent, 0);
	lv_obj_set_style_bg_opa(paddle_obj, LV_OPA_COVER, 0);
	lv_obj_set_scrollable(paddle_obj, false);
	lv_obj_set_clickable(paddle_obj, false);

	// Ball
	ball_obj = lv_obj_create(container);
	lv_obj_remove_style_all(ball_obj);
	lv_obj_set_size(ball_obj, BALL_SIZE, BALL_SIZE);
	lv_obj_set_style_radius(ball_obj, 3, 0);
	lv_obj_set_style_bg_color(ball_obj, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(ball_obj, LV_OPA_COVER, 0);
	lv_obj_set_scrollable(ball_obj, false);
	lv_obj_set_clickable(ball_obj, false);

	lv_obj_move_foreground(frame.overlay);

	game_frame_add_header_button(&frame, &icon_game_restart, new_game);
}

void brick_breaker_init(gui_config_t *cfg) {
	brick_cfg = cfg;
}

void brick_breaker_open(void) {
	if (!brick_breaker_screen) {
		brick_breaker_build(brick_cfg);
	}
	srand((unsigned)time(NULL));
	game_frame_open(&frame);
}
