#include "collision.h"
#include "player.h"
#include "enemy.h"
#include "game_state.h"

void collision_init(void) {
    // 初始化碰撞检测
}

void collision_detection(void) {
    // 简单矩形碰撞检测
    if(abs(player_x - enemy_x) < 2 && 
       abs(player_y - enemy_y) < 2) {
        // 触发碰撞事件
        game_set_state(GAME_OVER);
    }
}
