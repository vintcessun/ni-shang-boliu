#ifndef INPUT_H
#define INPUT_H

#include "game_state.h"

typedef enum {
    INPUT_NONE,
    INPUT_UP,
    INPUT_DOWN, 
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_CONFIRM,
    INPUT_CANCEL,
    INPUT_PAUSE
} InputCommand;

MenuSelection input_get_menu_selection(void);
bool input_get_pause(void);
bool input_get_resume(void); 
bool input_get_menu(void);
bool input_get_restart(void);
bool input_get_back(void);

#endif
