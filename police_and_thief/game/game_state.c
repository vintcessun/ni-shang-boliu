#include "game_state.h"
#include "input.h"
#include "game.h"
#include "player.h"
#include "enemy.h"
#include "collision.h"
#include "settings.h"

static GameState current_state = GAME_STATE_MENU;

void game_state_init(void)
{
    current_state = GAME_STATE_MENU;
}

void game_state_update(void)
{
    switch(current_state) {
        case GAME_STATE_MENU:
            // 菜单状态处理
            if (input_get_menu_selection() == MENU_START_GAME) {
                game_state_set(GAME_STATE_PLAYING);
                game_init();
            } else if (input_get_menu_selection() == MENU_SETTINGS) {
                game_state_set(GAME_STATE_SETTINGS);
            }
            break;

        case GAME_STATE_PLAYING:
            // 游戏进行状态处理
            game_update();
            player_update();
            enemy_update();
            collision_detection();
            
            if (game_is_over()) {
                game_state_set(GAME_STATE_GAME_OVER);
            } else if (input_get_pause()) {
                game_state_set(GAME_STATE_PAUSED);
            }
            break;

        case GAME_STATE_PAUSED:
            // 暂停状态处理
            if (input_get_resume()) {
                game_state_set(GAME_STATE_PLAYING);
            } else if (input_get_menu()) {
                game_state_set(GAME_STATE_MENU);
            }
            break;

        case GAME_STATE_GAME_OVER:
            // 游戏结束状态处理
            if (input_get_restart()) {
                game_state_set(GAME_STATE_PLAYING);
                game_reset();
            } else if (input_get_menu()) {
                game_state_set(GAME_STATE_MENU);
            }
            break;

        case GAME_STATE_SETTINGS:
            // 设置状态处理
            settings_update();
            if (input_get_back()) {
                game_state_set(GAME_STATE_MENU);
            }
            break;

        case GAME_STATE_RESULT:
            // 结果状态处理
            if (input_get_restart()) {
                game_state_set(GAME_STATE_PLAYING);
                game_reset();
            } else if (input_get_menu()) {
                game_state_set(GAME_STATE_MENU);
            }
            break;

        default:
            break;
    }
}

GameState game_state_get_current(void)
{
    return current_state;
}
