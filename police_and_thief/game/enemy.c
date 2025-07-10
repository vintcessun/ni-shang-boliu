#include "enemy.h"
#include "game_state.h"
#include "player.h"

static int enemy_x = 10;
static int enemy_y = 10;

void enemy_init(void) {
    enemy_x = 10;
    enemy_y = 10;
}

void enemy_update(void) {
    // 简单AI: 向玩家移动
    if(enemy_x < player_x) enemy_x++;
    else if(enemy_x > player_x) enemy_x--;
    
    if(enemy_y < player_y) enemy_y++;
    else if(enemy_y > player_y) enemy_y--;
}

void enemy_reset(void) {
    enemy_init();
}
