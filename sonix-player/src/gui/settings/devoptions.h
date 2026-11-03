#ifndef DEVOPTIONS_H
#define DEVOPTIONS_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// Settings that only matter while debugging: ADB and diagnostic logging.
extern lv_obj_t *devoptions_screen;

void devoptions_init(gui_config_t *cfg);

#endif /* DEVOPTIONS_H */
