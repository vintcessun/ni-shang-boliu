#include "touch.h"
#include "bflb_mtimer.h"
#include "board.h"
#include "touch_conf_user.h"
#include "log.h"

#define DBG_TAG "TOUCH"

static touch_coord_t touch_max_point = {
    .coord_x = 240,
    .coord_y = 320
};

void touch_init(void)
{
    board_init();
    bflb_mtimer_delay_ms(50);
    
    if (touch_init(&touch_max_point) != 0) {
        LOG_E("Touch init failed");
    } else {
        LOG_I("Touch initialized");
    }
}

int touch_read(touch_coord_t *coord)
{
    uint8_t point_num = 0;
    touch_coord_t touch_coord;
    
    touch_read(&point_num, &touch_coord, 1);
    
    if (point_num) {
        coord->coord_x = touch_coord.coord_x;
        coord->coord_y = touch_coord.coord_y <= 160 ? 
                        touch_coord.coord_y + 160 : 
                        touch_coord.coord_y - 160;
        return 1;
    }
    return 0;
}
