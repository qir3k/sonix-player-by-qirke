#ifndef GUI_STACK_IT_H
#define GUI_STACK_IT_H

#include "src/gui/shell/gui.h"
#include "lvgl/lvgl.h"

extern lv_obj_t *stack_it_screen;

void stack_it_init(gui_config_t *cfg);
void stack_it_open(void);

#endif /* GUI_STACK_IT_H */
