#include "vinylayout.h"

vinyl_box_t vinyl_layout_box(int screen_w, int screen_h, int deck_h) {
	vinyl_box_t b = {0};
	if (screen_w < 1)
		screen_w = 1;
	if (screen_h < 1)
		screen_h = 1;
	if (deck_h < 0)
		deck_h = 0;
	if (deck_h > screen_h - 32)
		deck_h = screen_h > 32 ? screen_h - 32 : 0;
	b.deck_y = screen_h - deck_h;
	if (b.deck_y < 0)
		b.deck_y = 0;
	b.controls_y = screen_h - 44 - 4;
	if (b.controls_y < 0)
		b.controls_y = 0;

	int box_w = screen_w;
	int box_h = b.deck_y;

	/* Maximum diameter that fits in the upper area with margins */
	int max_d = box_h - 12;
	if (max_d > box_w - 16) {
		max_d = box_w - 16;
	}
	max_d &= ~1;
	if (max_d < 60) {
		max_d = 60;
	}

	b.record_d = max_d;
	b.record_cx = box_w / 2;
	b.record_cy = box_h / 2;
	b.record_x = b.record_cx - max_d / 2;
	b.record_y = b.record_cy - max_d / 2;

	/* Circular center label (album art): 68% of disc diameter */
	int label_d = (max_d * 68 / 100) & ~1;
	if (label_d < 30) {
		label_d = 30;
	}
	b.art_side = label_d;
	b.art_x = b.record_cx - label_d / 2;
	b.art_y = b.record_cy - label_d / 2;

	return b;
}

static lv_obj_t *plain_circle(lv_obj_t *parent) {
	lv_obj_t *obj = lv_obj_create(parent);
	lv_obj_remove_style_all(obj);
	lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_pad_all(obj, 0, 0);
	lv_obj_set_style_border_width(obj, 0, 0);
	lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
	return obj;
}

void vinyl_record_build(vinyl_record_t *r, lv_obj_t *parent) {
	r->disc = plain_circle(parent);
	lv_obj_set_style_bg_color(r->disc, lv_color_make(18, 18, 18), 0);
	lv_obj_set_style_bg_opa(r->disc, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(r->disc, VINYL_ART_BORDER, 0);
	lv_obj_set_style_border_opa(r->disc, LV_OPA_COVER, 0);

	for (int i = 0; i < 4; i++) {
		r->grooves[i] = plain_circle(r->disc);
		lv_obj_set_style_border_width(r->grooves[i], 1, 0);
		lv_obj_set_style_border_color(r->grooves[i], lv_color_make(34, 34, 34), 0);
		lv_obj_set_style_border_opa(r->grooves[i], LV_OPA_COVER, 0);
	}

	r->shine = lv_obj_create(r->disc);
	lv_obj_remove_style_all(r->shine);
	lv_obj_set_style_radius(r->shine, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(r->shine, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(r->shine, 40, 0);
	lv_obj_clear_flag(r->shine, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(r->shine, LV_OBJ_FLAG_CLICKABLE);

	/* Spindle center hole (child of parent so it renders over cover_art_group) */
	r->spindle = plain_circle(parent);
	lv_obj_set_style_bg_color(r->spindle, lv_color_make(15, 15, 15), 0);
	lv_obj_set_style_bg_opa(r->spindle, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(r->spindle, 1, 0);
	lv_obj_set_style_border_color(r->spindle, lv_color_make(90, 90, 90), 0);
	lv_obj_set_style_border_opa(r->spindle, LV_OPA_COVER, 0);
}

void vinyl_record_apply(vinyl_record_t *r, const vinyl_box_t *b, lv_color_t rim) {
	if (!r || !r->disc || !b)
		return;
	int d = b->record_d;
	lv_obj_set_size(r->disc, d, d);
	lv_obj_align(r->disc, LV_ALIGN_TOP_LEFT, b->record_x, b->record_y);
	lv_obj_set_style_border_color(r->disc, rim, 0);

	/* Concentric grooves between disc rim and center label */
	int label_d = b->art_side;
	int groove_span = (d - label_d) / 2;
	for (int i = 0; i < 4; i++) {
		int gd = label_d + (groove_span * (i + 1) * 2 / 5);
		lv_obj_set_size(r->grooves[i], gd, gd);
		lv_obj_center(r->grooves[i]);
	}

	/* Shine reflection line */
	int mark_w = 2;
	int mark_h = groove_span - 6;
	if (mark_h < 4)
		mark_h = 4;
	lv_obj_set_size(r->shine, mark_w, mark_h);
	lv_obj_set_pos(r->shine, d / 2 + label_d / 2 + 3, d / 2 - mark_h / 2);
	lv_obj_set_style_transform_pivot_x(r->shine, -(label_d / 2 + 3), 0);
	lv_obj_set_style_transform_pivot_y(r->shine, mark_h / 2, 0);
	lv_obj_set_style_transform_rotation(r->shine, -150, 0);

	/* Spindle center hole */
	if (r->spindle) {
		int sp_d = 12;
		lv_obj_set_size(r->spindle, sp_d, sp_d);
		lv_obj_align(r->spindle, LV_ALIGN_TOP_LEFT, b->record_cx - sp_d / 2, b->record_cy - sp_d / 2);
		lv_obj_clear_flag(r->spindle, LV_OBJ_FLAG_HIDDEN);
	}

	lv_obj_clear_flag(r->disc, LV_OBJ_FLAG_HIDDEN);
}
