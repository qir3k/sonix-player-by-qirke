#ifndef UI_GENERIC_LIST_H
#define UI_GENERIC_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Universal list view with a fixed pool of recycled row widgets for 240x320 screen.
// Only rows fitting within the viewport are created and dynamically bound.
#define UI_GENERIC_LIST_POOL_SIZE 8
#define UI_GENERIC_LIST_ROW_HEIGHT 44

typedef struct {
	int (*get_item_count)(void *user_data);
	// Fills title, subtitle, and an LVGL symbol string (e.g. LV_SYMBOL_AUDIO, LV_SYMBOL_DIRECTORY, LV_SYMBOL_FILE)
	void (*get_item_title)(int index, char *title_out, size_t title_size,
						   char *subtitle_out, size_t subtitle_size,
						   const char **symbol_out, void *user_data);
	void (*on_item_select)(int index, void *user_data);
} ui_generic_list_adapter_t;

typedef struct {
	lv_obj_t *container;
	lv_obj_t *rows[UI_GENERIC_LIST_POOL_SIZE];
	lv_obj_t *icon_labels[UI_GENERIC_LIST_POOL_SIZE];
	lv_obj_t *title_labels[UI_GENERIC_LIST_POOL_SIZE];
	lv_obj_t *subtitle_labels[UI_GENERIC_LIST_POOL_SIZE];
	int bound_indices[UI_GENERIC_LIST_POOL_SIZE];

	ui_generic_list_adapter_t adapter;
	void *user_data;
	int total_count;
} ui_generic_list_t;

// Creates a universal recycled list view on parent
lv_obj_t *ui_generic_list_create(lv_obj_t *parent, const ui_generic_list_adapter_t *adapter, void *user_data);

// Refreshes visible rows and updates virtual scroll container height
void ui_generic_list_refresh(lv_obj_t *obj);

// Dynamically sets a new adapter without destroying or reallocating row widgets
void ui_generic_list_set_adapter(lv_obj_t *obj, const ui_generic_list_adapter_t *adapter, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UI_GENERIC_LIST_H */
