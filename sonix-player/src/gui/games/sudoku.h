#ifndef GUI_SUDOKU_H
#define GUI_SUDOKU_H

#include "src/gui/shell/gui.h"
#include "lvgl/lvgl.h"

extern lv_obj_t *sudoku_screen;

void sudoku_init(gui_config_t *cfg);
void sudoku_open(void);

#endif /* GUI_SUDOKU_H */
