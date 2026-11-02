#ifndef GUI_BRICK_BREAKER_H
#define GUI_BRICK_BREAKER_H

#include "src/gui/shell/gui.h"
#include "lvgl/lvgl.h"

extern lv_obj_t *brick_breaker_screen;

void brick_breaker_init(gui_config_t *cfg);
void brick_breaker_open(void);

#endif /* GUI_BRICK_BREAKER_H */
