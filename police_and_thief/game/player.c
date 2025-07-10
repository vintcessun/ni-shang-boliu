#include "player.h"
#include "input.h"
#include "game_state.h"

static int player_x = 0;
static int player_y = 0;

void player_init(void) {
    player_x = 0;
    player_y = 0;
}

void player_update(void) {
    InputCommand cmd = input_get_command();
    switch(cmd) {
        case INPUT_UP:
            player_y--;
            break;
        case INPUT_DOWN:
            player_y++;
            break;
        case INPUT_LEFT:
            player_x--;
            break;
        case INPUT_RIGHT:
            player_x++;
            break;
        default:
            break;
    }
}

void player_reset(void) {
    player_init();
}
