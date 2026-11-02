#ifndef GUI_GUESS_SONG_H
#define GUI_GUESS_SONG_H

#include "src/gui/shell/gui.h"
#include "lvgl/lvgl.h"

extern lv_obj_t *guess_song_screen;

void guess_song_init(gui_config_t *cfg);
void guess_song_open(void);

#endif /* GUI_GUESS_SONG_H */
