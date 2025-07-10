#ifndef __TOUCH_H__
#define __TOUCH_H__

#include "touch.h"
#include "bflb_mtimer.h"

typedef struct {
    int16_t coord_x;
    int16_t coord_y;
} touch_coord_t;

void touch_init(void);
int touch_read(touch_coord_t *coord);

#endif
