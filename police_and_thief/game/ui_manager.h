#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "game_state.h"

void ui_init(void);
void ui_update(GameState state);
void ui_show_menu(void);
void ui_show_game(void);
void ui_show_result(int score);

#endif
