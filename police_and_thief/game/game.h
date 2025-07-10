#ifndef GAME_H
#define GAME_H

#include "game_state.h"

void game_init(void);
void game_update(void);
void game_reset(void);
bool game_is_over(void);

#endif
