#include "ui_generic_list.h"
#include "src/gui/shell/theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

typedef struct {
	ui_generic_list_t list_data;
	lv_obj_t *spacer;
} ui_generic_list_ctx_t;

static void row_click_cb(lv_event_t *e) {
	lv_obj_t *row = lv_event_get_target(e);
	lv_obj_t *container = (lv_obj_t *)lv_event_get_user_data(e);
	if (!row || !container) return;

	ui_generic_list_ctx_t *ctx = (ui_generic_list_ctx_t *)lv_obj_get_user_data(container);
	if (!ctx) return;

	int item_idx = (int)(intptr_t)lv_obj_get_user_data(row);
	if (item_idx >= 0 && item_idx < ctx->list_data.total_count && ctx->list_data.adapter.on_item_select) {
		ctx->list_data.adapter.on_item_select(item_idx, ctx->list_data.user_data);
	}
}

static void update_visible_rows(ui_generic_list_ctx_t *ctx) {
	if (!ctx) return;

	lv_obj_t *cont = ctx->list_data.container;
	int scroll_y = (int)lv_obj_get_scroll_y(cont);
	if (scroll_y < 0) scroll_y = 0;

	int start_idx = scroll_y / UI_GENERIC_LIST_ROW_HEIGHT;
	if (start_idx < 0) start_idx = 0;

	for (int i = 0; i < UI_GENERIC_LIST_POOL_SIZE; i++) {
		int item_idx = start_idx + i;
		lv_obj_t *row = ctx->list_data.rows[i];

		if (item_idx < ctx->list_data.total_count) {
			lv_obj_set_y(row, item_idx * UI_GENERIC_LIST_ROW_HEIGHT);
			lv_obj_set_user_data(row, (void *)(intptr_t)item_idx);

			if (ctx->list_data.bound_indices[i] != item_idx) {
				char title[128] = {0};
				char subtitle[128] = {0};
				const char *symbol = NULL;

				if (ctx->list_data.adapter.get_item_title) {
					ctx->list_data.adapter.get_item_title(item_idx, title, sizeof(title),
														  subtitle, sizeof(subtitle),
														  &symbol, ctx->list_data.user_data);
				}

				lv_label_set_text(ctx->list_data.icon_labels[i], symbol ? symbol : LV_SYMBOL_AUDIO);
				lv_label_set_text(ctx->list_data.title_labels[i], title);

				if (subtitle[0] != '\0') {
					lv_label_set_text(ctx->list_data.subtitle_labels[i], subtitle);
					lv_obj_clear_flag(ctx->list_data.subtitle_labels[i], LV_OBJ_FLAG_HIDDEN);
					lv_obj_align(ctx->list_data.title_labels[i], LV_ALIGN_TOP_LEFT, 30, 2);
				} else {
					lv_obj_add_flag(ctx->list_data.subtitle_labels[i], LV_OBJ_FLAG_HIDDEN);
					lv_obj_align(ctx->list_data.title_labels[i], LV_ALIGN_LEFT_MID, 30, 0);
				}

				ctx->list_data.bound_indices[i] = item_idx;
			}

			lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
			ctx->list_data.bound_indices[i] = -1;
		}
	}
}

static void list_scroll_cb(lv_event_t *e) {
	lv_obj_t *cont = lv_event_get_target(e);
	if (!cont) return;

	ui_generic_list_ctx_t *ctx = (ui_generic_list_ctx_t *)lv_obj_get_user_data(cont);
	if (ctx) {
		update_visible_rows(ctx);
	}
}

static void list_delete_cb(lv_event_t *e) {
	lv_obj_t *cont = lv_event_get_target(e);
	if (!cont) return;

	ui_generic_list_ctx_t *ctx = (ui_generic_list_ctx_t *)lv_obj_get_user_data(cont);
	if (ctx) {
		free(ctx);
		lv_obj_set_user_data(cont, NULL);
#if defined(__GLIBC__)
		malloc_trim(0);
#endif
	}
}

lv_obj_t *ui_generic_list_create(lv_obj_t *parent, const ui_generic_list_adapter_t *adapter, void *user_data) {
	lv_obj_t *cont = lv_obj_create(parent);
	lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
	lv_obj_set_scroll_dir(cont, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
	lv_obj_set_style_pad_all(cont, 0, 0);
	lv_obj_set_style_border_width(cont, 0, 0);
	lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);

	ui_generic_list_ctx_t *ctx = (ui_generic_list_ctx_t *)calloc(1, sizeof(ui_generic_list_ctx_t));
	if (!ctx) return cont;

	ctx->list_data.container = cont;
	if (adapter) {
		ctx->list_data.adapter = *adapter;
	}
	ctx->list_data.user_data = user_data;

	// Invisible spacer defining the virtual scroll container height
	ctx->spacer = lv_obj_create(cont);
	lv_obj_set_size(ctx->spacer, 1, 1);
	lv_obj_set_style_opa(ctx->spacer, LV_OPA_TRANSP, 0);
	lv_obj_clear_flag(ctx->spacer, LV_OBJ_FLAG_CLICKABLE);

	// Pre-allocate fixed pool of recycled row widgets
	for (int i = 0; i < UI_GENERIC_LIST_POOL_SIZE; i++) {
		lv_obj_t *row = lv_obj_create(cont);
		lv_obj_set_size(row, lv_pct(100), UI_GENERIC_LIST_ROW_HEIGHT);
		lv_obj_set_style_pad_hor(row, 8, 0);
		lv_obj_set_style_pad_ver(row, 4, 0);
		lv_obj_set_style_border_width(row, 0, 0);
		lv_obj_set_style_radius(row, 6, 0);
		lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
		lv_obj_set_style_bg_opa(row, LV_OPA_30, LV_STATE_PRESSED);
		lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

		lv_obj_t *icon_lbl = lv_label_create(row);
		lv_obj_add_style(icon_lbl, &theme_style_text, 0);
		lv_obj_align(icon_lbl, LV_ALIGN_LEFT_MID, 4, 0);

		lv_obj_t *title_lbl = lv_label_create(row);
		lv_obj_add_style(title_lbl, &theme_style_text, 0);
		lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
		lv_obj_set_width(title_lbl, 180);
		lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 30, 2);

		lv_obj_t *sub_lbl = lv_label_create(row);
		lv_obj_add_style(sub_lbl, &theme_style_text_dim, 0);
		lv_label_set_long_mode(sub_lbl, LV_LABEL_LONG_DOT);
		lv_obj_set_width(sub_lbl, 180);
		lv_obj_align(sub_lbl, LV_ALIGN_BOTTOM_LEFT, 30, -2);

		ctx->list_data.rows[i] = row;
		ctx->list_data.icon_labels[i] = icon_lbl;
		ctx->list_data.title_labels[i] = title_lbl;
		ctx->list_data.subtitle_labels[i] = sub_lbl;
		ctx->list_data.bound_indices[i] = -1;

		lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, cont);
	}

	lv_obj_set_user_data(cont, ctx);
	lv_obj_add_event_cb(cont, list_scroll_cb, LV_EVENT_SCROLL, NULL);
	lv_obj_add_event_cb(cont, list_delete_cb, LV_EVENT_DELETE, NULL);

	ui_generic_list_refresh(cont);
	return cont;
}

void ui_generic_list_refresh(lv_obj_t *obj) {
	if (!obj) return;
	ui_generic_list_ctx_t *ctx = (ui_generic_list_ctx_t *)lv_obj_get_user_data(obj);
	if (!ctx) return;

	if (ctx->list_data.adapter.get_item_count) {
		ctx->list_data.total_count = ctx->list_data.adapter.get_item_count(ctx->list_data.user_data);
	} else {
		ctx->list_data.total_count = 0;
	}

	int total_h = ctx->list_data.total_count * UI_GENERIC_LIST_ROW_HEIGHT;
	if (total_h > 0) {
		lv_obj_set_pos(ctx->spacer, 0, total_h - 1);
		lv_obj_clear_flag(ctx->spacer, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(ctx->spacer, LV_OBJ_FLAG_HIDDEN);
	}

	// Invalidate bound row indices to force repaint
	for (int i = 0; i < UI_GENERIC_LIST_POOL_SIZE; i++) {
		ctx->list_data.bound_indices[i] = -1;
	}

	update_visible_rows(ctx);
}

void ui_generic_list_set_adapter(lv_obj_t *obj, const ui_generic_list_adapter_t *adapter, void *user_data) {
	if (!obj) return;
	ui_generic_list_ctx_t *ctx = (ui_generic_list_ctx_t *)lv_obj_get_user_data(obj);
	if (!ctx) return;

	if (adapter) {
		ctx->list_data.adapter = *adapter;
	} else {
		memset(&ctx->list_data.adapter, 0, sizeof(ui_generic_list_adapter_t));
	}
	ctx->list_data.user_data = user_data;

	ui_generic_list_refresh(obj);
}
