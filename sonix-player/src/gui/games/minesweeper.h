#ifndef GUI_MINESWEEPER_H
#define GUI_MINESWEEPER_H

#include "src/gui/shell/gui.h"
#include "lvgl/lvgl.h"

extern lv_obj_t *minesweeper_screen;

void minesweeper_init(gui_config_t *cfg);
void minesweeper_open(void);

#endif /* GUI_MINESWEEPER_H */
