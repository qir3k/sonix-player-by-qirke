#include "spinner.h"
#include "src/gui/shell/theme.h"
#include <stdint.h>

// Slow 250 ms timer: updates discreet 3-dot animation without CPU/bus churn.
#define SPINNER_UPDATE_MS 250

static void spinner_timer_cb(lv_timer_t *timer) {
	lv_obj_t *label = (lv_obj_t *)lv_timer_get_user_data(timer);
	if (!label) return;

	if (!lv_obj_is_visible(label)) return;

	static const char * const dot_frames[] = {
		".  ",
		".. ",
		"...",
		"   "
	};
	static const char * const text_frames[] = {
		"Ładowanie   ",
		"Ładowanie.  ",
		"Ładowanie.. ",
		"Ładowanie..."
	};

	uintptr_t state = (uintptr_t)lv_obj_get_user_data(label);
	bool is_small = (state & 0x100) != 0;
	uint8_t frame = (uint8_t)(state & 0x3);
	frame = (frame + 1) & 0x3;

	lv_obj_set_user_data(label, (void *)(uintptr_t)((is_small ? 0x100 : 0) | frame));

	if (is_small) {
		lv_label_set_text(label, dot_frames[frame]);
	} else {
		lv_label_set_text(label, text_frames[frame]);
	}
}

static void spinner_delete_cb(lv_event_t *e) {
	lv_timer_t *timer = (lv_timer_t *)lv_event_get_user_data(e);
	if (timer) {
		lv_timer_delete(timer);
	}
}

lv_obj_t *spinner_create(lv_obj_t *parent, const lv_image_dsc_t *icon) {
	lv_obj_t *label = lv_label_create(parent);
	lv_obj_set_style_text_color(label, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
	lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

	bool is_small = (icon && icon->header.w <= 20);
	if (is_small) {
		lv_label_set_text(label, ".  ");
		lv_obj_set_user_data(label, (void *)(uintptr_t)0x100);
	} else {
		lv_label_set_text(label, "Ładowanie...");
		lv_obj_set_user_data(label, (void *)(uintptr_t)3);
	}

	lv_timer_t *timer = lv_timer_create(spinner_timer_cb, SPINNER_UPDATE_MS, label);
	lv_obj_add_event_cb(label, spinner_delete_cb, LV_EVENT_DELETE, timer);

	return label;
}
