#include "river_crossing.h"
#include <stdbool.h>

static GameMap current_map;
static Suggestion suggestions[3];
static int suggestion_count = 0;

void game_init() {
    current_map.left.police = 0;
    current_map.left.thief = 0;
    current_map.right.police = 3;
    current_map.right.thief = 3;
    current_map.boat_position = RIGHT_BANK;
    suggestion_count = 0;
}

GameState game_update(int police_move, int thief_move, bool to_left) {
    // 验证移动是否合法
    if(police_move < 0 || thief_move < 0 || 
       (police_move + thief_move) > 2 ||
       (police_move + thief_move) < 1) {
        return LOSE;
    }

    // 检查船的位置是否匹配移动方向
    if((to_left && current_map.boat_position != RIGHT_BANK) ||
       (!to_left && current_map.boat_position != LEFT_BANK)) {
        return LOSE;
    }

    Bank* from = to_left ? &current_map.right : &current_map.left;
    Bank* to = to_left ? &current_map.left : &current_map.right;

    // 检查船上是否有警察
    if(police_move == 0) {
        return LOSE;
    }

    // 检查岸上是否有足够的人
    if(from->police < police_move || from->thief < thief_move) {
        return LOSE;
    }

    // 更新两岸人数和船位置
    from->police -= police_move;
    from->thief -= thief_move;
    to->police += police_move;
    to->thief += thief_move;
    current_map.boat_position = to_left ? LEFT_BANK : RIGHT_BANK;

    // 检查游戏状态
    if(current_map.left.police == 3 && current_map.left.thief == 3) {
        return WIN;
    }

    // 检查小偷反扑条件
    if((current_map.left.police < current_map.left.thief && current_map.left.police > 0) ||
       (current_map.right.police < current_map.right.thief && current_map.right.police > 0)) {
        return LOSE;
    }

    return PROCESSING;
}

const Suggestion* get_suggestions(int* count) {
    suggestion_count = 0;
    
    // 生成合法移动建议
    Bank* from = (current_map.boat_position == RIGHT_BANK) ? 
                &current_map.right : &current_map.left;
    
    // 建议1: 1警察过河
    if(from->police >= 1) {
        suggestions[suggestion_count].police = 1;
        suggestions[suggestion_count].thief = 0;
        suggestions[suggestion_count].to_left = 
            (current_map.boat_position == RIGHT_BANK);
        suggestion_count++;
    }
    
    // 建议2: 2警察过河
    if(from->police >= 2) {
        suggestions[suggestion_count].police = 2;
        suggestions[suggestion_count].thief = 0;
        suggestions[suggestion_count].to_left = 
            (current_map.boat_position == RIGHT_BANK);
        suggestion_count++;
    }
    
    // 建议3: 1警察1小偷过河
    if(from->police >= 1 && from->thief >= 1) {
        suggestions[suggestion_count].police = 1;
        suggestions[suggestion_count].thief = 1;
        suggestions[suggestion_count].to_left = 
            (current_map.boat_position == RIGHT_BANK);
        suggestion_count++;
    }
    
    *count = suggestion_count;
    return suggestions;
}

GameMap get_current_map() {
    return current_map;
}
