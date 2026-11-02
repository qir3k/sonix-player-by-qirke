#ifndef GUI_PEBBLE_DELUXE_H
#define GUI_PEBBLE_DELUXE_H

#include "lvgl/lvgl.h"
#include "src/gui/shell/gui.h"

extern lv_obj_t *pebble_deluxe_screen;

void pebble_deluxe_init(gui_config_t *cfg);
void pebble_deluxe_open(void);

#endif /* GUI_PEBBLE_DELUXE_H */
