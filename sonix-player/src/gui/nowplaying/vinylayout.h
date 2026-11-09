#ifndef VINYLLAYOUT_H
#define VINYLLAYOUT_H

#include "lvgl/lvgl.h"

/* Gramophone vinyl record geometry for player.
 * The vinyl disc is a circular record with concentric grooves,
 * with the circular album art placed in its center like an LP label. */
#define VINYL_ART_BORDER 2

typedef struct {
	int art_x, art_y, art_side;
	int record_x, record_y, record_d;
	int record_cx, record_cy;
	int deck_y;
	int controls_y;
} vinyl_box_t;

/* All coordinates are relative to the player sheet. */
vinyl_box_t vinyl_layout_box(int screen_w, int screen_h, int deck_h);

typedef struct {
	lv_obj_t *disc;
	lv_obj_t *grooves[4];
	lv_obj_t *spindle;
	lv_obj_t *shine;
} vinyl_record_t;

/* Build once, then move/paint on layout or theme changes. No timers, canvas,
 * image buffer or per-frame allocation. */
void vinyl_record_build(vinyl_record_t *record, lv_obj_t *parent);
void vinyl_record_apply(vinyl_record_t *record, const vinyl_box_t *box, lv_color_t rim);

#endif /* VINYLLAYOUT_H */
