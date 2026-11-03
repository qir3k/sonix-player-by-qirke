#include <assert.h>
#include <stdio.h>

#include "src/gui/shell/edge_lock.h"

int main(void) {
	lv_init();
	lv_display_create(240, 320);

	lv_obj_t *screen = lv_screen_active();
	lv_obj_t *target = lv_obj_create(screen);
	lv_obj_set_scrollable(target, true);
	lv_obj_set_scroll_chain_hor(target, false);
	lv_obj_set_scroll_chain_ver(target, true);
	lv_obj_set_press_lock(target, false);

	edge_lock_t lock = {0};
	edge_lock_acquire(&lock, target);
	assert(!lv_obj_is_scrollable(target));
	assert(!lv_obj_is_scroll_chain_hor(target));
	assert(!lv_obj_is_scroll_chain_ver(target));
	assert(lv_obj_is_press_lock(target));

	edge_lock_release(&lock);
	assert(lv_obj_is_scrollable(target));
	assert(!lv_obj_is_scroll_chain_hor(target));
	assert(lv_obj_is_scroll_chain_ver(target));
	assert(!lv_obj_is_press_lock(target));

	edge_lock_acquire(&lock, target);
	lv_obj_delete(target);
	assert(lock.obj == NULL);
	edge_lock_release(&lock);

	puts("edge lock: state restoration and deleted targets passed");
	return 0;
}
