// pebble_deluxe.c -- Minimalist Pebble Deluxe (Peggle-style) for Sonix Player.
#include "pebble_deluxe.h"

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

#define SECTION "pebble_deluxe"
#define FIELD_W 216
#define FIELD_H 216

#define CANNON_X (FIELD_W / 2)
#define CANNON_Y 8
#define CANNON_R 6

#define BALL_SIZE 7
#define BALL_R 3.5f

#define BUCKET_W 40
#define BUCKET_H 8
#define BUCKET_Y (FIELD_H - BUCKET_H - 1)

#define MAX_PEGS 48
#define PEG_SIZE 10
#define PEG_R 5.0f

#define GUIDE_DOTS 6

lv_obj_t *pebble_deluxe_screen;

static game_frame_t frame;
static gui_config_t *pebble_cfg;

typedef enum {
	PEG_BLUE = 0,    // Regular pebble
	PEG_ORANGE,      // Target pebble (must clear all to win)
	PEG_PURPLE,      // High score bonus
	PEG_GREEN,       // Extra ball powerup
} peg_type_t;

typedef struct {
	float x, y;
	peg_type_t type;
	bool alive;
	bool hit;
	lv_obj_t *obj;
} peg_t;

static peg_t pegs[MAX_PEGS];
static int peg_count;
static int targets_left;

static lv_obj_t *cannon_obj;
static lv_obj_t *guide_dots[GUIDE_DOTS];

static lv_obj_t *bucket_obj;
static float bucket_x = (FIELD_W - BUCKET_W) / 2.0f;
static float bucket_dir = 1.0f;
static float bucket_speed = 1.6f;
static int last_bucket_draw_x = -999;
static int bucket_flash_ticks = 0;
static bool bucket_caught = false;
static lv_obj_t *catch_banner;

static lv_obj_t *ball_obj;
static float ball_x, ball_y;
static float ball_vx, ball_vy;
static bool ball_in_play = false;
static int last_ball_draw_x = -999;
static int last_ball_draw_y = -999;
static int shot_ticks = 0;

static float aim_angle = 1.57079f; // default straight down (PI/2)
static bool is_aiming = false;
static int balls_left = 10;
static int current_level = 1;
static bool game_over = false;

static const lv_color_t COLOR_BLUE   = LV_COLOR_MAKE(0x4a, 0x9e, 0xff);
static const lv_color_t COLOR_ORANGE = LV_COLOR_MAKE(0xff, 0x88, 0x00);
static const lv_color_t COLOR_PURPLE = LV_COLOR_MAKE(0xab, 0x7f, 0xff);
static const lv_color_t COLOR_GREEN  = LV_COLOR_MAKE(0x00, 0xe6, 0x76);
static const lv_color_t COLOR_WHITE  = LV_COLOR_MAKE(0xff, 0xff, 0xff);

static lv_color_t color_for_peg(peg_type_t type, bool hit) {
	if (hit) return COLOR_WHITE;
	switch (type) {
		case PEG_ORANGE: return COLOR_ORANGE;
		case PEG_PURPLE: return COLOR_PURPLE;
		case PEG_GREEN:  return COLOR_GREEN;
		case PEG_BLUE:
		default:         return COLOR_BLUE;
	}
}

static void update_hint(void) {
	char buf[64];
	if (ball_in_play) {
		snprintf(buf, sizeof(buf), "Targets: %d - Balls: %d", targets_left, balls_left);
	} else {
		snprintf(buf, sizeof(buf), "Lvl %d - Balls: %d - Targets: %d", current_level, balls_left, targets_left);
	}
	game_frame_set_hint(&frame, buf);
}

static void update_aim(float touch_x, float touch_y) {
	float dx = touch_x - CANNON_X;
	float dy = touch_y - CANNON_Y;
	if (dy < 12.0f) dy = 12.0f; // clamp so cannon always aims downward
	aim_angle = atan2f(dy, dx);

	for (int i = 0; i < GUIDE_DOTS; i++) {
		if (guide_dots[i]) {
			if (ball_in_play || game_over) {
				lv_obj_set_hidden(guide_dots[i], true);
			} else {
				float dist = 16.0f + i * 14.0f;
				float gx = CANNON_X + cosf(aim_angle) * dist - 2.0f;
				float gy = CANNON_Y + sinf(aim_angle) * dist - 2.0f;
				lv_obj_set_pos(guide_dots[i], (int)gx, (int)gy);
				lv_obj_set_hidden(guide_dots[i], false);
			}
		}
	}
}

static void add_peg(float x, float y, peg_type_t type) {
	if (peg_count >= MAX_PEGS) return;
	int idx = peg_count++;
	pegs[idx].x = x;
	pegs[idx].y = y;
	pegs[idx].type = type;
	pegs[idx].alive = true;
	pegs[idx].hit = false;
	if (type == PEG_ORANGE) {
		targets_left++;
	}

	if (pegs[idx].obj) {
		lv_obj_set_pos(pegs[idx].obj, (int)(x - PEG_R), (int)(y - PEG_R));
		lv_obj_set_style_bg_color(pegs[idx].obj, color_for_peg(type, false), 0);
		lv_obj_set_hidden(pegs[idx].obj, false);
	}
}

static void build_level(int lvl) {
	peg_count = 0;
	targets_left = 0;

	for (int i = 0; i < MAX_PEGS; i++) {
		if (pegs[i].obj) {
			lv_obj_set_hidden(pegs[i].obj, true);
		}
	}

	if (lvl == 1) {
		// Level 1: Classic Wedge / Pyramid
		int rows = 5;
		float start_y = 48.0f;
		for (int r = 0; r < rows; r++) {
			int count = r + 3;
			float spacing = 24.0f;
			float start_x = CANNON_X - ((count - 1) * spacing) / 2.0f;
			for (int c = 0; c < count; c++) {
				float px = start_x + c * spacing;
				float py = start_y + r * 22.0f;
				peg_type_t type = PEG_BLUE;
				if (r == 0 && c == 1) type = PEG_GREEN;
				else if ((r + c) % 3 == 0) type = PEG_ORANGE;
				else if (r == rows - 1 && (c == 0 || c == count - 1)) type = PEG_PURPLE;
				add_peg(px, py, type);
			}
		}
	} else if (lvl == 2) {
		// Level 2: Concentric Ring & Core
		add_peg(CANNON_X, 100.0f, PEG_ORANGE);
		add_peg(CANNON_X - 16.0f, 100.0f, PEG_ORANGE);
		add_peg(CANNON_X + 16.0f, 100.0f, PEG_ORANGE);
		add_peg(CANNON_X, 84.0f, PEG_GREEN);
		add_peg(CANNON_X, 116.0f, PEG_PURPLE);

		int ring_count = 14;
		float radius = 54.0f;
		for (int i = 0; i < ring_count; i++) {
			float ang = (float)i * (2.0f * 3.14159f / ring_count);
			float px = CANNON_X + cosf(ang) * radius;
			float py = 100.0f + sinf(ang) * radius;
			peg_type_t type = (i % 2 == 0) ? PEG_ORANGE : PEG_BLUE;
			add_peg(px, py, type);
		}
	} else if (lvl == 3) {
		// Level 3: Dual Crossing Waves
		for (int i = 0; i < 9; i++) {
			float x = 20.0f + i * 22.0f;
			float y1 = 70.0f + sinf(i * 0.8f) * 26.0f;
			float y2 = 135.0f - sinf(i * 0.8f) * 26.0f;
			add_peg(x, y1, (i % 2 == 0) ? PEG_ORANGE : PEG_BLUE);
			add_peg(x, y2, (i == 4) ? PEG_GREEN : ((i % 3 == 0) ? PEG_PURPLE : PEG_ORANGE));
		}
	} else {
		// Level 4: Constellation Matrix
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 6; c++) {
				float px = 30.0f + c * 31.0f + ((r % 2) * 15.0f);
				float py = 52.0f + r * 28.0f;
				peg_type_t type = PEG_BLUE;
				if ((r + c) % 3 == 1) type = PEG_ORANGE;
				else if (r == 1 && c == 2) type = PEG_GREEN;
				else if (r == 2 && c == 3) type = PEG_PURPLE;
				add_peg(px, py, type);
			}
		}
	}

	update_hint();
}

static void reset_ball_to_cannon(void) {
	ball_in_play = false;
	is_aiming = false;
	shot_ticks = 0;
	ball_x = CANNON_X - BALL_R;
	ball_y = CANNON_Y - BALL_R;
	last_ball_draw_x = (int)ball_x;
	last_ball_draw_y = (int)ball_y;
	if (ball_obj) {
		lv_obj_set_pos(ball_obj, last_ball_draw_x, last_ball_draw_y);
		lv_obj_set_hidden(ball_obj, false);
	}
	update_aim(CANNON_X, CANNON_Y + 40.0f);
	update_hint();
}

static void new_game(void) {
	game_frame_freeze(&frame, false);
	game_frame_hide_overlay(&frame);
	game_over = false;
	ball_in_play = false;
	is_aiming = false;
	shot_ticks = 0;
	bucket_flash_ticks = 0;
	if (catch_banner) lv_obj_set_hidden(catch_banner, true);
	current_level = 1;
	balls_left = 10;
	game_frame_set_score(&frame, 0);
	build_level(current_level);
	reset_ball_to_cannon();
	game_frame_set_period(&frame, 25);
}

static void next_level(void) {
	game_frame_freeze(&frame, false);
	game_frame_hide_overlay(&frame);
	game_over = false;
	ball_in_play = false;
	is_aiming = false;
	shot_ticks = 0;
	bucket_flash_ticks = 0;
	if (catch_banner) lv_obj_set_hidden(catch_banner, true);
	current_level++;
	balls_left += 5; // bonus balls for next level
	build_level(current_level);
	reset_ball_to_cannon();
	game_frame_set_period(&frame, 25);
}

static void fire_ball(void) {
	if (ball_in_play || game_over || balls_left <= 0) return;

	ball_in_play = true;
	balls_left--;
	shot_ticks = 0;

	float speed = 4.8f;
	ball_vx = cosf(aim_angle) * speed;
	ball_vy = sinf(aim_angle) * speed;
	ball_x = CANNON_X + cosf(aim_angle) * 12.0f - BALL_R;
	ball_y = CANNON_Y + sinf(aim_angle) * 12.0f - BALL_R;
	last_ball_draw_x = (int)ball_x;
	last_ball_draw_y = (int)ball_y;
	if (ball_obj) {
		lv_obj_set_pos(ball_obj, last_ball_draw_x, last_ball_draw_y);
		lv_obj_set_hidden(ball_obj, false);
	}

	for (int i = 0; i < GUIDE_DOTS; i++) {
		if (guide_dots[i]) lv_obj_set_hidden(guide_dots[i], true);
	}
	update_hint();
}

static void end_of_shot(void) {
	ball_in_play = false;
	is_aiming = false;
	shot_ticks = 0;
	if (ball_obj) lv_obj_set_hidden(ball_obj, true);

	// Pop all hit pegs
	for (int i = 0; i < peg_count; i++) {
		if (pegs[i].alive && pegs[i].hit) {
			pegs[i].alive = false;
			if (pegs[i].type == PEG_ORANGE) {
				targets_left--;
			}
			if (pegs[i].obj) {
				lv_obj_set_hidden(pegs[i].obj, true);
			}
		}
	}

	if (targets_left <= 0) {
		// Level cleared!
		game_frame_add_score(&frame, 2000);
		game_over = true;
		if (current_level >= 4) {
			char msg[64];
			snprintf(msg, sizeof(msg), "All Levels Beat! Score: %d", frame.score);
			game_frame_show_overlay(&frame, "Victory!", msg, "Play Again", NULL);
		} else {
			char msg[64];
			snprintf(msg, sizeof(msg), "Stage Cleared! +2000 pts");
			game_frame_show_overlay(&frame, "Awesome!", msg, "Next Level", NULL);
		}
		return;
	}

	if (balls_left <= 0) {
		// Game Over
		char msg[64];
		snprintf(msg, sizeof(msg), "Out of balls! Score: %d", frame.score);
		game_over = true;
		game_frame_show_overlay(&frame, "Game Over", msg, "Try Again", NULL);
		return;
	}

	reset_ball_to_cannon();
}

static void tap_cb(const lv_point_t *p) {
	if (game_over || ball_in_play) return;
	is_aiming = true;
	update_aim((float)p->x, (float)p->y);
}

static void drag_cb(const lv_point_t *p) {
	if (game_over || ball_in_play || !is_aiming) return;
	update_aim((float)p->x, (float)p->y);
}

static void release_cb(const lv_point_t *p) {
	if (game_over || ball_in_play || !is_aiming) {
		is_aiming = false;
		return;
	}
	is_aiming = false;
	if (p) {
		update_aim((float)p->x, (float)p->y);
	}
	fire_ball();
}

static void tick_cb(void) {
	// 1. Bucket flash / catch feedback animation
	if (bucket_flash_ticks > 0) {
		bucket_flash_ticks--;
		if (bucket_caught) {
			lv_obj_set_style_bg_color(bucket_obj, COLOR_GREEN, 0);
			lv_obj_set_style_border_color(bucket_obj, lv_color_white(), 0);
			lv_obj_set_style_border_width(bucket_obj, 2, 0);
		} else {
			lv_obj_set_style_bg_color(bucket_obj, LV_COLOR_MAKE(0xff, 0x44, 0x44), 0);
			lv_obj_set_style_border_color(bucket_obj, LV_COLOR_MAKE(0xff, 0x99, 0x99), 0);
			lv_obj_set_style_border_width(bucket_obj, 2, 0);
		}
		if (bucket_flash_ticks == 0) {
			lv_obj_set_style_bg_color(bucket_obj, theme()->accent, 0);
			lv_obj_set_style_border_color(bucket_obj, lv_color_white(), 0);
			lv_obj_set_style_border_width(bucket_obj, 1, 0);
			if (catch_banner) lv_obj_set_hidden(catch_banner, true);
		}
	}

	// 2. Move bucket at bottom
	bucket_x += bucket_dir * bucket_speed;
	if (bucket_x <= 0) {
		bucket_x = 0;
		bucket_dir = 1.0f;
	} else if (bucket_x >= FIELD_W - BUCKET_W) {
		bucket_x = FIELD_W - BUCKET_W;
		bucket_dir = -1.0f;
	}
	int ibx = (int)bucket_x;
	if (ibx != last_bucket_draw_x) {
		last_bucket_draw_x = ibx;
		lv_obj_set_pos(bucket_obj, ibx, BUCKET_Y);
	}

	if (!ball_in_play || game_over) return;

	shot_ticks++;
	// Anti-stuck watchdog: downward impulse if stuck too long
	if (shot_ticks > 450) {
		ball_vy += 0.25f;
	}
	if (shot_ticks > 750) {
		ball_y = FIELD_H + 2.0f;
	}

	// Ball physics: gravity
	ball_vy += 0.14f;

	// Cap terminal speed
	float spd = sqrtf(ball_vx * ball_vx + ball_vy * ball_vy);
	if (spd > 6.0f) {
		ball_vx = (ball_vx / spd) * 6.0f;
		ball_vy = (ball_vy / spd) * 6.0f;
	}

	ball_x += ball_vx;
	ball_y += ball_vy;

	// Bounce off side walls
	if (ball_x <= 0) {
		ball_x = 0;
		ball_vx = -ball_vx * 0.9f;
	} else if (ball_x >= FIELD_W - BALL_SIZE) {
		ball_x = FIELD_W - BALL_SIZE;
		ball_vx = -ball_vx * 0.9f;
	}

	// Bounce off top wall
	if (ball_y <= 0) {
		ball_y = 0;
		ball_vy = -ball_vy * 0.9f;
	}

	// Collisions with pegs
	float ball_cx = ball_x + BALL_R;
	float ball_cy = ball_y + BALL_R;

	for (int i = 0; i < peg_count; i++) {
		if (!pegs[i].alive) continue;

		float dx = ball_cx - pegs[i].x;
		float dy = ball_cy - pegs[i].y;
		float dist_sq = dx * dx + dy * dy;
		float min_dist = BALL_R + PEG_R;

		if (dist_sq <= min_dist * min_dist) {
			float dist = sqrtf(dist_sq);
			if (dist < 0.001f) dist = 0.001f;
			float nx = dx / dist;
			float ny = dy / dist;

			// Push ball outside peg to resolve penetration and eliminate lag/jitter
			float overlap = min_dist - dist;
			ball_x += nx * (overlap + 0.3f);
			ball_y += ny * (overlap + 0.3f);
			ball_cx = ball_x + BALL_R;
			ball_cy = ball_y + BALL_R;

			float dot = ball_vx * nx + ball_vy * ny;
			if (dot < 0) {
				ball_vx = (ball_vx - 1.85f * dot * nx) * 0.94f;
				ball_vy = (ball_vy - 1.85f * dot * ny) * 0.94f;
			}

			if (!pegs[i].hit) {
				pegs[i].hit = true;
				if (pegs[i].obj) {
					lv_obj_set_style_bg_color(pegs[i].obj, COLOR_WHITE, 0);
				}
				if (pegs[i].type == PEG_ORANGE) {
					game_frame_add_score(&frame, 300);
				} else if (pegs[i].type == PEG_PURPLE) {
					game_frame_add_score(&frame, 600);
				} else if (pegs[i].type == PEG_GREEN) {
					game_frame_add_score(&frame, 250);
					balls_left++; // Green peg grants bonus ball!
				} else {
					game_frame_add_score(&frame, 100);
				}
				update_hint();
			}
		}
	}

	// Check if ball dropped out of field
	if (ball_y >= FIELD_H) {
		// Check bucket catch with visual feedback
		if (ball_cx >= bucket_x - 2.0f && ball_cx <= (bucket_x + BUCKET_W + 2.0f)) {
			balls_left++;
			game_frame_add_score(&frame, 500);
			game_frame_set_hint(&frame, "FREE BALL! Caught in bucket!");
			bucket_caught = true;
			bucket_flash_ticks = 10;
			if (catch_banner) {
				lv_label_set_text(catch_banner, "+FREE BALL!");
				lv_obj_set_style_text_color(catch_banner, COLOR_GREEN, 0);
				int bx = (int)bucket_x + (BUCKET_W / 2) - 35;
				if (bx < 2) bx = 2;
				if (bx > FIELD_W - 75) bx = FIELD_W - 75;
				lv_obj_set_pos(catch_banner, bx, BUCKET_Y - 18);
				lv_obj_set_hidden(catch_banner, false);
			}
		} else {
			bucket_caught = false;
			bucket_flash_ticks = 6;
			if (catch_banner) {
				lv_label_set_text(catch_banner, "MISSED");
				lv_obj_set_style_text_color(catch_banner, LV_COLOR_MAKE(0xff, 0x55, 0x55), 0);
				int bx = (int)bucket_x + (BUCKET_W / 2) - 20;
				if (bx < 2) bx = 2;
				if (bx > FIELD_W - 45) bx = FIELD_W - 45;
				lv_obj_set_pos(catch_banner, bx, BUCKET_Y - 18);
				lv_obj_set_hidden(catch_banner, false);
			}
		}
		end_of_shot();
		return;
	}

	int ibx_ball = (int)ball_x;
	int iby_ball = (int)ball_y;
	if (ibx_ball != last_ball_draw_x || iby_ball != last_ball_draw_y) {
		last_ball_draw_x = ibx_ball;
		last_ball_draw_y = iby_ball;
		lv_obj_set_pos(ball_obj, ibx_ball, iby_ball);
	}
}

static void overlay_primary_cb(void) {
	is_aiming = false;
	if (targets_left <= 0 && current_level < 4) {
		next_level();
	} else {
		new_game();
	}
}

static void show_cb(void) {
	new_game();
	game_frame_set_period(&frame, 25);
}

static void hide_cb(void) {
	game_frame_set_period(&frame, 0);
}

static const game_hooks_t hooks = {
	.on_show = show_cb,
	.on_hide = hide_cb,
	.on_tick = tick_cb,
	.on_tap = tap_cb,
	.on_drag = drag_cb,
	.on_release = release_cb,
};

static void pebble_deluxe_build(gui_config_t *cfg) {
	pebble_cfg = cfg;
	frame.hooks = &hooks;
	frame.section = SECTION;
	frame.title = tr("pebble_deluxe");
	pebble_deluxe_screen = lv_obj_create(NULL);
	frame.screen = pebble_deluxe_screen;
	frame.on_overlay_primary = overlay_primary_cb;

	game_frame_build(&frame, cfg);

	// Cannon object (top center dome)
	cannon_obj = lv_obj_create(frame.field);
	lv_obj_set_size(cannon_obj, CANNON_R * 2, CANNON_R * 2);
	lv_obj_set_pos(cannon_obj, CANNON_X - CANNON_R, CANNON_Y - CANNON_R);
	lv_obj_set_style_radius(cannon_obj, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(cannon_obj, lv_color_white(), 0);
	lv_obj_set_style_border_width(cannon_obj, 0, 0);
	lv_obj_set_style_shadow_width(cannon_obj, 0, 0);

	// Aim guide dots
	for (int i = 0; i < GUIDE_DOTS; i++) {
		guide_dots[i] = lv_obj_create(frame.field);
		lv_obj_set_size(guide_dots[i], 4, 4);
		lv_obj_set_style_radius(guide_dots[i], LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(guide_dots[i], lv_color_white(), 0);
		lv_obj_set_style_border_width(guide_dots[i], 0, 0);
		lv_obj_set_style_shadow_width(guide_dots[i], 0, 0);
		lv_obj_set_style_bg_opa(guide_dots[i], LV_OPA_60 - i * 8, 0);
		lv_obj_set_hidden(guide_dots[i], true);
	}

	// Moving bucket at bottom
	bucket_obj = lv_obj_create(frame.field);
	lv_obj_set_size(bucket_obj, BUCKET_W, BUCKET_H);
	lv_obj_set_pos(bucket_obj, (int)bucket_x, BUCKET_Y);
	lv_obj_set_style_radius(bucket_obj, 4, 0);
	lv_obj_set_style_bg_color(bucket_obj, theme()->accent, 0);
	lv_obj_set_style_border_width(bucket_obj, 1, 0);
	lv_obj_set_style_border_color(bucket_obj, lv_color_white(), 0);
	lv_obj_set_style_shadow_width(bucket_obj, 0, 0);

	// Catch feedback banner
	catch_banner = lv_label_create(frame.field);
	lv_obj_set_style_text_font(catch_banner, &font_ui_14, 0);
	lv_obj_set_hidden(catch_banner, true);

	// Peg objects
	for (int i = 0; i < MAX_PEGS; i++) {
		pegs[i].obj = lv_obj_create(frame.field);
		lv_obj_set_size(pegs[i].obj, PEG_SIZE, PEG_SIZE);
		lv_obj_set_style_radius(pegs[i].obj, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_border_width(pegs[i].obj, 0, 0);
		lv_obj_set_style_shadow_width(pegs[i].obj, 0, 0);
		lv_obj_set_hidden(pegs[i].obj, true);
	}

	// Ball
	ball_obj = lv_obj_create(frame.field);
	lv_obj_set_size(ball_obj, BALL_SIZE, BALL_SIZE);
	lv_obj_set_style_radius(ball_obj, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(ball_obj, lv_color_white(), 0);
	lv_obj_set_style_border_width(ball_obj, 0, 0);
	lv_obj_set_style_shadow_width(ball_obj, 0, 0);
	lv_obj_set_hidden(ball_obj, false);

	// Add header restart button
	game_frame_add_header_button(&frame, &icon_game_restart, new_game);

	update_aim(CANNON_X, CANNON_Y + 40.0f);
}

void pebble_deluxe_init(gui_config_t *cfg) {
	pebble_cfg = cfg;
}

void pebble_deluxe_open(void) {
	if (!pebble_deluxe_screen) {
		pebble_deluxe_build(pebble_cfg);
	}
	game_frame_open(&frame);
}
