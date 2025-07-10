#include "bflb_mtimer.h"
#include "board.h"
#include "lcd_conf_user.h"
#include "lcd.h"
#include "game/river_crossing.h"
#include "game/evaluation.h"
#include "touch.h"
#include "wifi.h"
#include "log.h"

static void init_system_components(void) {
    board_init();
    bflb_mtimer_delay_ms(50);
    lcd_init();
    lcd_clear(0x0000);
    wifi_init();
    game_init();
    eval_init();
}

static void show_prompt_screen(void) {
    lcd_draw_str_ascii16(10, 50, 0xFA60, 0xFFFF, "请输入prompt:", 12);
    lcd_draw_str_ascii16(10, 70, 0xFA60, 0xFFFF, "1. 按左键确认", 12);
    lcd_draw_str_ascii16(10, 90, 0xFA60, 0xFFFF, "2. 按右键取消", 12);
    bflb_mtimer_delay_ms(2000);
}

static InputCommand get_user_input(void) {
    InputCommand cmd = touch_get_input();
    if(cmd == INPUT_NONE) {
        cmd = wifi_get_input();
    }
    return cmd;
}

static void process_game_state(GameState state, uint32_t start_time) {
    if(state != PROCESSING) {
        char report[256];
        eval_generate_report(report, sizeof(report));
        
        char text[256];
        if(wifi_receive_game_data("192.168.1.100", 8080, text, sizeof(text)) > 0) {
            text_to_speech(text);
            lcd_draw_str_ascii16(10, 120, 0xFA60, 0xFFFF, text, strlen(text));
            bflb_mtimer_delay_ms(3000);
        }
    }
}

static void display_game_status(void) {
    GameMap map = get_current_map();
    char left_str[32], right_str[32];
    snprintf(left_str, sizeof(left_str), "左岸: 警察%d 小偷%d", map.left.police, map.left.thief);
    snprintf(right_str, sizeof(right_str), "右岸: 警察%d 小偷%d", map.right.police, map.right.thief);
    lcd_draw_str_ascii16(10, 10, 0xFA60, 0xFFFF, left_str, strlen(left_str));
    lcd_draw_str_ascii16(10, 30, 0xFA60, 0xFFFF, right_str, strlen(right_str));
}

int main() {
    init_system_components();
    show_prompt_screen();

    uint32_t start_time = HAL_GetTick();
    GameState state = PROCESSING;

    while(1) {
        InputCommand cmd = get_user_input();
        bool to_left = (cmd == INPUT_LEFT);
        state = game_update(to_left ? 1 : 0, 0, to_left);
        
        eval_record_move(state != LOSE, false);
        eval_update_time(HAL_GetTick() - start_time);

        process_game_state(state, start_time);
        if(state != PROCESSING) {
            break;
        }

        display_game_status();
    }

    return 0;
}
