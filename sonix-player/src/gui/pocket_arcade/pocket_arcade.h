#ifndef GUI_POCKET_ARCADE_H
#define GUI_POCKET_ARCADE_H

#include "lvgl/lvgl.h"
#include "src/gui/shell/gui.h"

extern lv_obj_t *pocket_arcade_screen;
extern lv_obj_t *arcade2048_screen;
extern const lv_image_dsc_t icon_menu_arcade;

void pocket_arcade_init(gui_config_t *cfg);
void pocket_arcade_open(void);
void arcade_stack_open(void);
void arcade_racer_open(void);
void arcade_bricks_open(void);
void arcade_mines_open(void);
void arcade_boxes_open(void);
void arcade_golf_open(void);
void arcade_orbit_open(void);
void arcade_melody_open(void);
void arcade_quiz_open(void);
void arcade_beat_open(void);
void arcade_dungeon_open(void);

/* Allow Now Playing ONLY from the 6px extreme right edge of Arcade pages.
 * Unlike R22.6.2/3, the game screen and children do not attach a player-sheet
 * drag listener; game-board gestures are exclusively game input. */
void pocket_arcade_enable_nowplaying_swipe(lv_obj_t *screen);

#endif
