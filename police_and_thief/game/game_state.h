#ifndef GAME_STATE_H
#define GAME_STATE_H

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER,
    GAME_STATE_SETTINGS,
    GAME_STATE_RESULT
} GameState;

typedef enum {
    MENU_START_GAME,
    MENU_SETTINGS,
    MENU_EXIT
} MenuSelection;

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

void game_state_init(void);
void game_state_update(void);
GameState game_state_get_current(void);

#endif
