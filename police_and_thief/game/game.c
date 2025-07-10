#include "game.h"
#include "game_state.h"
#include "player.h"
#include "enemy.h"
#include "collision.h"

static bool game_over = false;

void game_init(void) {
    player_init();
    enemy_init();
    collision_init();
    game_over = false;
}

void game_update(void) {
    player_update();
    enemy_update();
    collision_detection();
    
    if(player_is_caught() || time_is_up()) {
        game_over = true;
    }
}

void game_reset(void) {
    player_reset();
    enemy_reset();
    game_over = false;
}

bool game_is_over(void) {
    return game_over;
}
