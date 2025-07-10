#include "ui_manager.h"
#include "touch.h"
#include "lcd.h"

void ui_init(void)
{
    touch_init();
    lcd_init();
}

void ui_update(GameState state)
{
    switch(state) {
        case GAME_STATE_MENU:
            ui_show_menu();
            break;
        case GAME_STATE_PLAYING:
            ui_show_game();
            break;
        case GAME_STATE_RESULT:
            ui_show_result(0);
            break;
    }
}

void ui_show_menu(void)
{
    // 显示菜单界面
}

void ui_show_game(void)
{
    // 显示游戏界面
}

void ui_show_result(int score)
{
    // 显示结果界面
}
