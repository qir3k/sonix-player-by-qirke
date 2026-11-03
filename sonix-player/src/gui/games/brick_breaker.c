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
#define MAX_LEVEL 4

#define PADDLE_W 46
#define PADDLE_WIDE 68
#define PADDLE_H 7
#define PADDLE_Y 202

#define BALL_SIZE 6
#define MAX_BALLS 3
#define MAX_POWERUPS 4
#define POWERUP_W 24
#define POWERUP_H 16
#define PADDLE_POWERUP_TICKS 600

lv_obj_t *brick_breaker_screen;

static game_frame_t frame;
static gui_config_t *brick_cfg;

static lv_obj_t *brick_objs[TOTAL_BRICKS];
static bool brick_alive[TOTAL_BRICKS];

static lv_obj_t *paddle_obj;
static int paddle_x;
static int paddle_w;
static int paddle_powerup_ticks;

typedef struct {
	lv_obj_t *obj;
	float x, y;
	float vx, vy;
	bool active;
} ball_t;

typedef enum {
	POWERUP_NONE,
	POWERUP_MULTIBALL,
	POWERUP_WIDE_PADDLE,
} powerup_type_t;

typedef struct {
	lv_obj_t *obj;
	powerup_type_t type;
	float x, y;
	bool active;
} powerup_t;

static ball_t balls[MAX_BALLS];
static powerup_t powerups[MAX_POWERUPS];
static bool ball_launched;

static int lives = 3;
static int bricks_left = TOTAL_BRICKS;
static bool game_over = false;
static int current_level = 1;

static const lv_color_t row_colors[BRICK_ROWS] = {
	LV_COLOR_MAKE(0xff, 0x33, 0x66), // Pink
	LV_COLOR_MAKE(0xff, 0x88, 0x00), // Orange
	LV_COLOR_MAKE(0xff, 0xcc, 0x00), // Amber
	LV_COLOR_MAKE(0x00, 0xe6, 0x76), // Green
	LV_COLOR_MAKE(0x00, 0xd4, 0xff), // Cyan
};

static void update_hint(void) {
	char buf[64];
	snprintf(buf, sizeof(buf), "Level %d • Lives: %d • Score: %d", current_level, lives, frame.score);
	game_frame_set_hint(&frame, buf);
}

static void reset_ball(void) {
	ball_launched = false;
	for (int i = 0; i < MAX_BALLS; i++) {
		balls[i].active = i == 0;
		if (!balls[i].active) {
			lv_obj_set_hidden(balls[i].obj, true);
			continue;
		}
		balls[i].x = paddle_x + (paddle_w - BALL_SIZE) / 2.0f;
		balls[i].y = PADDLE_Y - BALL_SIZE - 2;
		balls[i].vx = 2.0f;
		balls[i].vy = -3.2f;
		lv_obj_set_pos(balls[i].obj, (int)balls[i].x, (int)balls[i].y);
		lv_obj_set_hidden(balls[i].obj, false);
	}
	update_hint();
}

static void reset_powerups(void) {
	for (int i = 0; i < MAX_POWERUPS; i++) {
		powerups[i].active = false;
		powerups[i].type = POWERUP_NONE;
		lv_obj_set_hidden(powerups[i].obj, true);
	}
}

static void set_paddle_width(int width) {
	int center = paddle_x + paddle_w / 2;
	paddle_w = width;
	paddle_x = center - paddle_w / 2;
	if (paddle_x < 0) paddle_x = 0;
	if (paddle_x > FIELD_W - paddle_w) paddle_x = FIELD_W - paddle_w;
	lv_obj_set_width(paddle_obj, paddle_w);
	lv_obj_set_pos(paddle_obj, paddle_x, PADDLE_Y);
}

static void new_game(void);

static void next_level(void) {
	game_frame_hide_overlay(&frame);
	game_over = false;
	current_level++;
	if (current_level > MAX_LEVEL) {
		new_game();
		return;
	}
	bricks_left = 0;
	for (int r = 0; r < BRICK_ROWS; r++) {
		for (int c = 0; c < BRICK_COLS; c++) {
			int i = r * BRICK_COLS + c;
			bool alive;
			switch (current_level) {
			case 2:
				alive = r < 4 && ((r + c) % 2 == 0 || r == 0);
				break;
			case 3:
				alive = (r == 0 || r == BRICK_ROWS - 1 || c == 0 || c == BRICK_COLS - 1 ||
					 (r == 2 && (c == 2 || c == 3)));
				break;
			default:
				alive = ((r + c) % 3 != 1) || (r == 2 && (c == 2 || c == 3));
				break;
			}
			brick_alive[i] = alive;
			lv_obj_set_hidden(brick_objs[i], !alive);
			if (alive) bricks_left++;
		}
	}
	reset_powerups();
	paddle_powerup_ticks = 0;
	set_paddle_width(PADDLE_W);
	reset_ball();
	update_hint();
}

static void new_game(void) {
	game_frame_hide_overlay(&frame);
	game_over = false;
	lives = 3;
	current_level = 1;
	bricks_left = TOTAL_BRICKS;
	paddle_w = PADDLE_W;
	paddle_powerup_ticks = 0;
	paddle_x = (FIELD_W - paddle_w) / 2;
	lv_obj_set_size(paddle_obj, paddle_w, PADDLE_H);
	lv_obj_set_pos(paddle_obj, paddle_x, PADDLE_Y);
	reset_powerups();

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
		balls[0].vx = ((rand() % 2 == 0) ? 2.2f : -2.2f);
		balls[0].vy = -3.5f;
	}
}

static void move_paddle_to(int field_x) {
	int offset_x = (frame.field_w - FIELD_W) / 2;
	int local_x = field_x - offset_x;
	int target_x = local_x - paddle_w / 2;
	if (target_x < 0) target_x = 0;
	if (target_x > FIELD_W - paddle_w) target_x = FIELD_W - paddle_w;
	paddle_x = target_x;
	lv_obj_set_pos(paddle_obj, paddle_x, PADDLE_Y);
}

static void drop_powerup(int x, int y) {
	if (rand() % 5 != 0) {
		return;
	}
	for (int i = 0; i < MAX_POWERUPS; i++) {
		if (powerups[i].active) continue;
		powerups[i].active = true;
		powerups[i].type = rand() % 2 ? POWERUP_MULTIBALL : POWERUP_WIDE_PADDLE;
		powerups[i].x = (float)x - POWERUP_W / 2.0f;
		powerups[i].y = (float)y;
		lv_obj_set_style_bg_color(powerups[i].obj,
					  powerups[i].type == POWERUP_MULTIBALL
						  ? lv_color_hex(0x55cc77)
						  : lv_color_hex(0x55aaff),
					  0);
		lv_label_set_text(lv_obj_get_child(powerups[i].obj, 0),
				  powerups[i].type == POWERUP_MULTIBALL ? "2x" : "W");
		lv_obj_set_pos(powerups[i].obj, (int)powerups[i].x, (int)powerups[i].y);
		lv_obj_set_hidden(powerups[i].obj, false);
		return;
	}
}

static void apply_powerup(powerup_type_t type) {
	if (type == POWERUP_WIDE_PADDLE) {
		paddle_powerup_ticks = PADDLE_POWERUP_TICKS;
		set_paddle_width(PADDLE_WIDE);
		game_frame_set_hint(&frame, "Wide paddle!");
		return;
	}

	int source = -1;
	int target = -1;
	for (int i = 0; i < MAX_BALLS; i++) {
		if (balls[i].active && source < 0) source = i;
		if (!balls[i].active && target < 0) target = i;
	}
	if (source < 0 || target < 0) {
		game_frame_set_hint(&frame, "Maximum balls!");
		return;
	}

	lv_obj_t *target_obj = balls[target].obj;
	balls[target] = balls[source];
	balls[target].obj = target_obj;
	balls[target].active = true;
	balls[target].vx = -balls[source].vx;
	balls[target].vy = balls[source].vy;
	balls[target].x += balls[target].vx < 0 ? -BALL_SIZE : BALL_SIZE;
	if (balls[target].x < 0) balls[target].x = 0;
	if (balls[target].x > FIELD_W - BALL_SIZE) balls[target].x = FIELD_W - BALL_SIZE;
	lv_obj_set_pos(target_obj, (int)balls[target].x, (int)balls[target].y);
	lv_obj_set_hidden(target_obj, false);
	game_frame_set_hint(&frame, "Extra ball!");
}

static void tick_powerups(void) {
	for (int i = 0; i < MAX_POWERUPS; i++) {
		if (!powerups[i].active) continue;
		powerups[i].y += 1.5f;
		lv_obj_set_pos(powerups[i].obj, (int)powerups[i].x, (int)powerups[i].y);
		if (powerups[i].y + POWERUP_H >= PADDLE_Y &&
		    powerups[i].x + POWERUP_W >= paddle_x &&
		    powerups[i].x <= paddle_x + paddle_w) {
			apply_powerup(powerups[i].type);
			powerups[i].active = false;
			lv_obj_set_hidden(powerups[i].obj, true);
		} else if (powerups[i].y > FIELD_H) {
			powerups[i].active = false;
			lv_obj_set_hidden(powerups[i].obj, true);
		}
	}
	if (paddle_powerup_ticks > 0 && --paddle_powerup_ticks == 0) {
		set_paddle_width(PADDLE_W);
	}
}

static void tap_cb(const lv_point_t *p) {
	if (game_over) return;
	if (!ball_launched) {
		launch_ball();
	}
	move_paddle_to(p->x);
}

static void drag_cb(const lv_point_t *p) {
	if (game_over) return;
	if (!ball_launched) {
		launch_ball();
	}
	move_paddle_to(p->x);
}

static void tick_cb(void) {
	if (game_over) return;

	tick_powerups();
	if (!ball_launched) {
		balls[0].x = paddle_x + (paddle_w - BALL_SIZE) / 2.0f;
		balls[0].y = PADDLE_Y - BALL_SIZE - 2;
		lv_obj_set_pos(balls[0].obj, (int)balls[0].x, (int)balls[0].y);
		return;
	}

	int start_x = (FIELD_W - (BRICK_COLS * BRICK_W + (BRICK_COLS - 1) * BRICK_GAP)) / 2;
	int start_y = 16;
	for (int b = 0; b < MAX_BALLS; b++) {
		ball_t *ball = &balls[b];
		if (!ball->active) continue;

		ball->x += ball->vx;
		ball->y += ball->vy;
		if (ball->x <= 0) {
			ball->x = 0;
			ball->vx = -ball->vx;
		} else if (ball->x + BALL_SIZE >= FIELD_W) {
			ball->x = FIELD_W - BALL_SIZE;
			ball->vx = -ball->vx;
		}
		if (ball->y <= 0) {
			ball->y = 0;
			ball->vy = -ball->vy;
		}

		if (ball->vy > 0 && ball->y + BALL_SIZE >= PADDLE_Y &&
		    ball->y <= PADDLE_Y + PADDLE_H &&
		    ball->x + BALL_SIZE >= paddle_x && ball->x <= paddle_x + paddle_w) {
			ball->y = PADDLE_Y - BALL_SIZE;
			ball->vy = -fabsf(ball->vy);
			float hit_offset = (ball->x + BALL_SIZE / 2.0f) - (paddle_x + paddle_w / 2.0f);
			ball->vx = hit_offset * 0.18f;
			if (fabsf(ball->vx) < 0.7f) {
				ball->vx = hit_offset < 0 ? -0.7f : 0.7f;
			}
		}

		for (int r = 0; r < BRICK_ROWS; r++) {
			for (int c = 0; c < BRICK_COLS; c++) {
				int i = r * BRICK_COLS + c;
				if (!brick_alive[i]) continue;
				int bx = start_x + c * (BRICK_W + BRICK_GAP);
				int by = start_y + r * (BRICK_H + BRICK_GAP);
				if (ball->x + BALL_SIZE < bx || ball->x > bx + BRICK_W ||
				    ball->y + BALL_SIZE < by || ball->y > by + BRICK_H) {
					continue;
				}

				brick_alive[i] = false;
				lv_obj_set_hidden(brick_objs[i], true);
				bricks_left--;
				ball->vy = -ball->vy;
				drop_powerup(bx + BRICK_W / 2, by + BRICK_H);
				game_frame_add_score(&frame, (BRICK_ROWS - r) * 10);
				update_hint();

				if (bricks_left <= 0) {
					game_over = true;
					game_frame_add_score(&frame, 500);
					if (current_level < MAX_LEVEL) {
						game_frame_show_overlay(&frame, "Level Complete!",
									"Catch upgrades and get ready for the next board.",
									"Next Level", NULL);
					} else {
						game_frame_show_overlay(&frame, "All Levels Cleared!",
									"Excellent game! Play again?",
									"Play Again", NULL);
					}
				}
				goto ball_done;
			}
		}

	ball_done:
		if (game_over) return;
		if (ball->y > FIELD_H) {
			ball->active = false;
			lv_obj_set_hidden(ball->obj, true);
			continue;
		}
		lv_obj_set_pos(ball->obj, (int)ball->x, (int)ball->y);
	}

	bool any_ball = false;
	for (int i = 0; i < MAX_BALLS; i++) {
		if (balls[i].active) {
			any_ball = true;
			break;
		}
	}
	if (!any_ball) {
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
	for (int i = 0; i < MAX_BALLS; i++) {
		lv_obj_set_style_bg_color(balls[i].obj, lv_color_white(), 0);
	}
}

static void overlay_primary_cb(void) {
	if (bricks_left <= 0 && current_level < MAX_LEVEL) {
		next_level();
	} else {
		new_game();
	}
}

static void brick_breaker_build(gui_config_t *cfg) {
	static const game_hooks_t hooks = {
		.on_show = show_cb,
		.on_hide = hide_cb,
		.on_tap = tap_cb,
		.on_drag = drag_cb,
		.on_tick = tick_cb,
		.on_theme = theme_cb,
	};

	frame.hooks = &hooks;
	frame.section = SECTION;
	frame.title = tr("brick_breaker");
	brick_breaker_screen = lv_obj_create(NULL);
	frame.screen = brick_breaker_screen;
	frame.on_overlay_primary = overlay_primary_cb;
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

	for (int i = 0; i < MAX_POWERUPS; i++) {
		powerups[i].obj = lv_obj_create(container);
		lv_obj_remove_style_all(powerups[i].obj);
		lv_obj_set_size(powerups[i].obj, POWERUP_W, POWERUP_H);
		lv_obj_set_style_radius(powerups[i].obj, 5, 0);
		lv_obj_set_style_bg_opa(powerups[i].obj, LV_OPA_COVER, 0);
		lv_obj_set_scrollable(powerups[i].obj, false);
		lv_obj_set_clickable(powerups[i].obj, false);
		lv_obj_t *label = lv_label_create(powerups[i].obj);
		lv_label_set_text(label, "");
		lv_obj_set_style_text_font(label, &font_ui_14, 0);
		lv_obj_set_style_text_color(label, lv_color_white(), 0);
		lv_obj_center(label);
		lv_obj_set_hidden(powerups[i].obj, true);
	}

	// Paddle
	paddle_obj = lv_obj_create(container);
	lv_obj_remove_style_all(paddle_obj);
	paddle_w = PADDLE_W;
	paddle_x = (FIELD_W - paddle_w) / 2;
	lv_obj_set_size(paddle_obj, paddle_w, PADDLE_H);
	lv_obj_set_pos(paddle_obj, paddle_x, PADDLE_Y);
	lv_obj_set_style_radius(paddle_obj, 3, 0);
	lv_obj_set_style_bg_color(paddle_obj, theme()->accent, 0);
	lv_obj_set_style_bg_opa(paddle_obj, LV_OPA_COVER, 0);
	lv_obj_set_scrollable(paddle_obj, false);
	lv_obj_set_clickable(paddle_obj, false);

	for (int i = 0; i < MAX_BALLS; i++) {
		balls[i].obj = lv_obj_create(container);
		lv_obj_remove_style_all(balls[i].obj);
		lv_obj_set_size(balls[i].obj, BALL_SIZE, BALL_SIZE);
		lv_obj_set_style_radius(balls[i].obj, 3, 0);
		lv_obj_set_style_bg_color(balls[i].obj, lv_color_white(), 0);
		lv_obj_set_style_bg_opa(balls[i].obj, LV_OPA_COVER, 0);
		lv_obj_set_scrollable(balls[i].obj, false);
		lv_obj_set_clickable(balls[i].obj, false);
		balls[i].active = false;
		lv_obj_set_hidden(balls[i].obj, true);
	}

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
