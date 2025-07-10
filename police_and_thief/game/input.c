#include "input.h"
#include "touch.h"
#include "wifi.h"

static InputCommand current_input = INPUT_NONE;

MenuSelection input_get_menu_selection(void) {
    // 通过触摸屏获取菜单选择
    TouchPoint point = touch_get_point();
    // 根据触摸坐标返回对应菜单项
    return MENU_START_GAME; // 示例返回值
}

bool input_get_pause(void) {
    return current_input == INPUT_PAUSE;
}

bool input_get_resume(void) {
    return current_input == INPUT_CONFIRM; 
}

bool input_get_menu(void) {
    return current_input == INPUT_CANCEL;
}

bool input_get_restart(void) {
    return current_input == INPUT_CONFIRM;
}

bool input_get_back(void) {
    return current_input == INPUT_CANCEL;
}

void input_update(void) {
    // 从WiFi或触摸屏获取最新输入
    current_input = wifi_get_input();
    if(current_input == INPUT_NONE) {
        current_input = touch_get_input();
    }
}
