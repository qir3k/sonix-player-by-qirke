#ifndef GAMESPAGE_H
#define GAMESPAGE_H

#include "lvgl/lvgl.h"
#include "src/gui/shell/gui.h"

extern lv_obj_t *gamespage_screen;

void gamespage_init(gui_config_t *cfg);
void gamespage_open(void);

#endif /* GAMESPAGE_H */
